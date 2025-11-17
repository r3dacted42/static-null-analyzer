#include "cfg.hpp"
#include <format>

namespace cfg {

node_ptr new_node() {
    return std::make_shared<CFGNode>();
}

node_ptr new_node(const json &data) {
    auto node = std::make_shared<CFGNode>();
    node->hasReturn = false;
    node->metadata.id = data["id"].get<std::string>();
    node->metadata.kind = data["kind"].get<std::string>();
    if (data.contains("loc") && data["loc"].contains("line"))
        node->metadata.line = data["loc"]["line"];
    else if (data.contains("range") && data["range"]["begin"].contains("line"))
        node->metadata.line = data["range"]["begin"]["line"];
    return node;
}

RValue CFG::handleExpr(const json &data) {
    const std::string kind = data["kind"];
    if (kind == "IntegerLiteral")
        return {{}, data["value"]};

    const std::string qualType = data.contains("type") && data["type"].contains("qualType")
                                     ? data["type"]["qualType"]
                                     : "?";
    const bool isPtr = qualType.back() == '*';
    const bool isFunc = qualType.contains('(');
    if (kind == "DeclRefExpr") {
        const auto refDecl = data["referencedDecl"];
        const std::string name = refDecl["name"];
        if (isPtr && !isFunc)
            return {{{NodePtrType::ANY, refDecl["id"], name, PtrState::PTR_REF}}, name};
        else
            return {{}, name};
    }
    if (kind == "ImplicitCastExpr") {
        const std::string castKind = data["castKind"];
        if (isPtr && castKind == "NullToPointer") {
            return {{{NodePtrType::ANY, "", "", PtrState::NULLPTR}}, "nullptr"};
        }
        return handleExpr(data["inner"][0]);
    }
    if (kind == "CXXNewExpr") {
        const auto label = std::format("new {}", qualType);
        return {{{NodePtrType::ANY, "", "", PtrState::ALLOC}}, label};
    }
    if (kind == "CXXDeleteExpr") {
        auto refPtr = handleExpr(data["inner"][0]);
        refPtr.ptrData[0].state = PtrState::DEALLOC;
        refPtr.label = std::format("delete {}", refPtr.label);
        return refPtr;
    }
    if (kind == "CallExpr") {
        const auto inner = data["inner"];
        const auto func = handleExpr(inner[0]);
        const auto numArgs = inner.size() - 1;
        if (numArgs == 0)
            return {{}, std::format("{}()", func.label)};
        std::vector<RValue> args(numArgs);
        for (size_t i = 0; i < numArgs; i++)
            args[i] = handleExpr(inner[i + 1]);
        const auto arg0 = args[0];
        if (numArgs == 1 && func.label == "free" && arg0.ptrData[0].state == PtrState::PTR_REF) {
            const auto ptrData0 = arg0.ptrData[0];
            return {{{NodePtrType::ANY, ptrData0.refId, ptrData0.name, PtrState::DEALLOC}},
                    std::format("free({})", ptrData0.name)};
        }
        if (numArgs == 1 && func.label == "malloc" && inner[1]["type"]["qualType"] == "size_t")
            return {{{NodePtrType::ANY, "", "", PtrState::ALLOC}},
                    std::format("malloc({})", arg0.label)};
        std::string argsStr = "";
        for (const auto &arg : args) {
            if (argsStr == "") {
                argsStr = arg.label;
                continue;
            }
            argsStr = std::format("{}, {}", argsStr, arg.label);
        }
        return {{}, std::format("{}( {} )", func.label, argsStr)};
    }
    if (kind == "UnaryExprOrTypeTraitExpr")
        return {{}, std::format("{} {}", data["name"].get<std::string>(), qualType)};
    if (kind == "CXXConstructExpr")
        return {{}, "constructor"};
    if (kind == "MemberExpr") {
        const auto ptr = handleExpr(data["inner"][0]);
        const std::string funcName = data["name"];
        return {{{NodePtrType::DEREF, ptr.ptrData[0].refId, ptr.label, PtrState::DONTCARE}},
                std::format("{}->{}()", ptr.label, funcName)};
    }

    if (!data.contains("opcode")) {
        if (data.contains("inner"))
            return handleExpr(data["inner"][0]);
        return {};
    }

    const std::string opCode = data["opcode"];
    if (kind == "UnaryOperator") {
        const auto innerValue = handleExpr(data["inner"][0]);
        if (isPtr) {
            if (opCode == "&")
                return {{{NodePtrType::ANY, "", "", PtrState::REFERENCE}}, "&" + innerValue.label};
            if (opCode == "*")
                return {{{NodePtrType::DEREF, innerValue.ptrData[0].refId, innerValue.label, PtrState::DONTCARE}},
                        "*" + innerValue.label};
        }
        return {{}, opCode + innerValue.label};
    }
    if (kind == "BinaryOperator") {
        const auto inner = data["inner"];
        const std::string valCategory = data["valueCategory"];
        const auto lhsValue = handleExpr(inner[0]);
        const auto rhsValue = handleExpr(inner[1]);
        const auto label = std::format("{} {} {}", lhsValue.label, opCode, rhsValue.label);
        if (isPtr && opCode == "=" && valCategory == "lvalue" && lhsValue.ptrData.size() == 1 &&
            lhsValue.ptrData[0].state == PtrState::PTR_REF && rhsValue.ptrData.size() == 1) {
            return {{{NodePtrType::ASSIGN, lhsValue.ptrData[0].refId, lhsValue.label, rhsValue.ptrData[0].state}},
                    label};
        }
        std::vector<PtrData> resultData;
        resultData.reserve(lhsValue.ptrData.size() + rhsValue.ptrData.size());
        for (const auto &p : lhsValue.ptrData)
            resultData.push_back(p);
        for (const auto &p : rhsValue.ptrData)
            resultData.push_back(p);
        if (qualType == "bool" && resultData.size() == 2) {
            int nullCount = 0, refCount = 0;
            for (const auto &d : resultData) {
                nullCount += (d.state == PtrState::NULLPTR);
                refCount += (d.state == PtrState::PTR_REF);
            }
            if (nullCount == 1 && refCount == 1) {
                auto ptrRefData = resultData.front().state == PtrState::PTR_REF
                                      ? resultData.front()
                                      : resultData.back();
                ptrRefData.n_type = opCode == "!="
                                        ? NodePtrType::B_NOT_NULL
                                        : NodePtrType::B_NULL;
                return {{ptrRefData}, label};
            }
        }
        return {resultData, label};
    }
    if (isPtr) {
        return {{{NodePtrType::ANY, "", "", PtrState::UNKNOWN}}, "PTR_VAL"};
    }

    return {};
}

const std::vector<std::string> exprStmt{
    "UnaryOperator",
    "BinaryOperator",
    "CXXMemberCallExpr",
    "CXXDeleteExpr"};

node_ptr CFG::recWalkAST(const json &data, node_ptr prev) {
    if (!data.contains("kind"))
        return prev;
    const std::string kind = data["kind"];
    const auto node = new_node(data);
    if (kind == "ReturnStmt") {
        node->label = "RETURN";
        if (data.contains("inner")) {
            const auto value = handleExpr(data["inner"][0]);
            node->label = std::format("return {}", value.label);
            node->ptrData = value.ptrData;
        }
        node->hasReturn = true;
        prev->next.push_back(node);
        return node;
    } else if (kind == "NullStmt") {
        node->label = "nil";
        prev->next.push_back(node);
        return node;
    }

    const std::string qualType = data.contains("type") && data["type"].contains("qualType")
                                     ? data["type"]["qualType"]
                                     : "?";
    bool hasPostAction = false;
    if (kind == "FunctionDecl") {
        const std::string name = data["name"];
        node->label = std::format("{} {}(...)", qualType, name);
    } else if (kind == "ParmVarDecl") {
        const std::string name = data["name"];
        node->label = std::format("{} {}", qualType, name);
        if (qualType.back() == '*') {
            node->ptrData.push_back({NodePtrType::DECL, node->metadata.id,
                                     name, PtrState::UNKNOWN});
        }
        prev->next.push_back(node);
        return node;
    } else if (kind == "CompoundStmt") {
        node->label = "{";
        hasPostAction = true;
    } else if (kind == "VarDecl") {
        const std::string name = data["name"];
        node->label = std::format("{} {}", qualType, name);
        if (data.contains("inner")) {
            const auto value = handleExpr(data["inner"][0]);
            node->label = std::format("{} = {}", node->label, value.label);
            node->ptrData = value.ptrData;
        } else if (qualType.back() == '*') {
            node->ptrData.push_back({NodePtrType::DECL, node->metadata.id,
                                     name, PtrState::UNKNOWN});
        }
        prev->next.push_back(node);
        return node;
    } else if (kind == "IfStmt") {
        const auto inner = data["inner"];
        const bool hasElse = data.contains("hasElse");
        const auto cond = handleExpr(inner[0]);
        node->label = std::format("if ( {} )", cond.label);
        node->ptrData = cond.ptrData;
        const auto thenEndNode = recWalkAST(inner[1], node);
        node_ptr elseEndNode;
        if (hasElse)
            elseEndNode = recWalkAST(inner[2], node);
        const auto fiNode = new_node();
        fiNode->label = ".";
        if (!thenEndNode->hasReturn)
            thenEndNode->next.push_back(fiNode);
        if (hasElse) {
            if (!elseEndNode->hasReturn)
                elseEndNode->next.push_back(fiNode);
            if (thenEndNode->hasReturn && elseEndNode->hasReturn)
                fiNode->hasReturn = true;
        } else
            node->next.push_back(fiNode);
        prev->next.push_back(node);
        return fiNode;
    } else if (kind == "ForStmt") {
        node->label = "for";
        const auto inner = data["inner"];
        const auto initData = inner[0];
        // empty slot at [1]
        const auto condData = inner[2];
        const auto updateData = inner[3];
        const auto bodyData = inner[4];
        node_ptr last = node;
        if (initData.contains("kind"))
            last = recWalkAST(initData, last);
        if (condData.contains("kind"))
            last = recWalkAST(condData, last);
        const auto condNode = last;
        last = recWalkAST(bodyData, condNode);
        if (updateData.contains("kind"))
            last = recWalkAST(updateData, last);
        const auto doneNode = new_node();
        doneNode->label = ".";
        last->next.push_back(condNode);
        condNode->next.push_back(doneNode);
        node->next.push_back(doneNode);
        prev->next.push_back(node);
        return doneNode;
    } else if (kind == "WhileStmt") {
        const auto inner = data["inner"];
        const auto cond = handleExpr(inner[0]);
        node->label = std::format("while ( {} )", cond.label);
        node->ptrData = cond.ptrData;
        const auto bodyNode = recWalkAST(inner[1], node);
        const auto doneNode = new_node();
        doneNode->label = ".";
        bodyNode->next.push_back(node);
        node->next.push_back(doneNode);
        prev->next.push_back(node);
        return doneNode;
    } else if (std::find(exprStmt.begin(), exprStmt.end(), kind) != exprStmt.end()) {
        const auto exprValue = handleExpr(data);
        if (!exprValue.label.empty()) {
            node->label = exprValue.label;
            node->ptrData = exprValue.ptrData;
            prev->next.push_back(node);
            return node;
        } else
            return prev;
    }

    node_ptr last;
    if (!node->label.empty()) {
        prev->next.push_back(node);
        last = node;
    } else
        last = prev;

    if (data.contains("inner")) {
        for (const auto &innerData : data["inner"]) {
            last = recWalkAST(innerData, last);
            if (last->hasReturn)
                break;
        }
    }

    if (hasPostAction) {
        if (kind == "CompoundStmt") {
            const auto endNode = new_node();
            endNode->label = "}";
            last->next.push_back(endNode);
            endNode->hasReturn = last->hasReturn;
            return endNode;
        }
    }
    return last;
}

CFG::CFG(const json &ast) {
    begin = new_node();
    begin->label = "BEGIN";
    const auto n = recWalkAST(ast, begin);
    end = new_node();
    end->label = "END";
    n->next.push_back(end);
}

} // namespace cfg
