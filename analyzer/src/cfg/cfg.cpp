#include "cfg.hpp"
#include <format>
#include <iostream>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace cfg {

NodeKind strToNodeKind(const std::string &kind) {
    if (kind == "BinaryOperator")
        return NodeKind::BinaryOperator;
    if (kind == "BreakStmt")
        return NodeKind::BreakStmt;
    if (kind == "CallExpr")
        return NodeKind::CallExpr;
    if (kind == "CXXDeleteExpr")
        return NodeKind::CXXDeleteExpr;
    if (kind == "ImplicitCastExpr")
        return NodeKind::ImplicitCastExpr;
    if (kind == "MemberExpr")
        return NodeKind::MemberExpr;
    if (kind == "ParmVarDecl")
        return NodeKind::ParmVarDecl;
    if (kind == "ReturnStmt")
        return NodeKind::ReturnStmt;
    if (kind == "UnaryOperator")
        return NodeKind::UnaryOperator;
    if (kind == "VarDecl")
        return NodeKind::VarDecl;
    if (kind == "IfStmt" || kind == "ForStmt" || kind == "WhileStmt")
        return NodeKind::BranchStmt;
    return NodeKind::Other;
}

RValKind strToRValKind(const std::string &kind) {
    if (kind == "UnaryOperator")
        return RValKind::UnaryOperator;
    if (kind == "BinaryOperator")
        return RValKind::BinaryOperator;
    if (kind == "DeclRefExpr")
        return RValKind::DeclRefExpr;
    if (kind == "CallExpr")
        return RValKind::CallExpr;
    if (kind == "CXXNewExpr")
        return RValKind::CXXNewExpr;
    if (kind == "CXXConstructExpr")
        return RValKind::CXXConstructExpr;
    if (kind == "ImplicitCastExpr")
        return RValKind::ImplicitCastExpr;
    return RValKind::Other;
}

node_t CFG::make_node(const std::string &id) {
    pool.emplace(id, new CFGNode());
    const auto &node = pool[id].get();
    node->metadata.id = id;
    return node;
}

node_t CFG::make_node(const std::string &id, const json &data) {
    pool.emplace(id, new CFGNode());
    const auto &node = pool[id].get();
    const std::string qualType = data.contains("type")
                                     ? data["type"]["qualType"]
                                     : "?";
    node->metadata = {id, 0, 0, strToNodeKind(data["kind"]),
                      qualType.back() == '*', qualType};
    if (data.contains("range")) {
        try {
            node->metadata.offsetBegin = data["range"]["begin"]["offset"];
            node->metadata.offsetEnd = data["range"]["end"]["offset"];
        } catch (json::exception &e) {
            // couldn't grab range but that's okay
        }
    }
    return node;
}

void CFG::erase_node(const std::string &id) {
    if (pool.contains(id))
        pool.erase(id);
}

std::string getPtrActLabel(const PtrAction &pact) {
    if (std::holds_alternative<PtrActionTypes::Declare>(pact))
        return std::format("Declare {}", std::get<PtrActionTypes::Declare>(pact).name);
    if (std::holds_alternative<PtrActionTypes::AssignConst>(pact)) {
        const auto &assignConst = std::get<PtrActionTypes::AssignConst>(pact);
        const std::string pref = std::format("Assign {} = ", assignConst.name);
        switch (assignConst.assignType) {
        case PtrActionTypes::AssignConst::AssignType::NullPtr:
            return pref + "nullptr";
        case PtrActionTypes::AssignConst::AssignType::Reference:
            return pref + "&var";
        default:
            return pref + "?";
        }
    }
    if (std::holds_alternative<PtrActionTypes::AssignPtr>(pact)) {
        const auto &assignPtr = std::get<PtrActionTypes::AssignPtr>(pact);
        return std::format("Assign {} = {}", assignPtr.nameL, assignPtr.nameR);
    }
    if (std::holds_alternative<PtrActionTypes::MemMgmt>(pact)) {
        const auto &memMgmt = std::get<PtrActionTypes::MemMgmt>(pact);
        switch (memMgmt.memType) {
        case PtrActionTypes::MemMgmt::MemType::Alloc:
            return std::format("Alloc {}", memMgmt.name);
        case PtrActionTypes::MemMgmt::MemType::Dealloc:
            return std::format("Dealloc {}", memMgmt.name);
        }
    }
    if (std::holds_alternative<PtrActionTypes::Deref>(pact))
        return std::format("Deref {}", std::get<PtrActionTypes::Deref>(pact).name);
    if (std::holds_alternative<PtrActionTypes::Branch>(pact)) {
        const auto &branch = std::get<PtrActionTypes::Branch>(pact);
        switch (branch.branchType) {
        case PtrActionTypes::Branch::BranchType::NotNull:
            return std::format("Branch {} Not Null", branch.name);
        case PtrActionTypes::Branch::BranchType::Null:
            return std::format("Branch {} Null", branch.name);
        }
    }
    return ""; // None
}

std::string getRValLabel(const RValue &rval) {
    if (std::holds_alternative<RValTypes::Other>(rval))
        return std::get<RValTypes::Other>(rval).label;
    if (std::holds_alternative<RValTypes::VarRef>(rval))
        return ("&" + std::get<RValTypes::VarRef>(rval).label);
    if (std::holds_alternative<RValTypes::CallExpr>(rval))
        return std::get<RValTypes::CallExpr>(rval).label;
    if (std::holds_alternative<RValTypes::OtherPtr>(rval))
        return std::get<RValTypes::OtherPtr>(rval).name;
    if (std::holds_alternative<RValTypes::PtrDeref>(rval))
        return ("*" + std::get<RValTypes::PtrDeref>(rval).name);
    if (std::holds_alternative<RValTypes::NullPtr>(rval))
        return "nullptr";
    return "?";
}

RValue CFG::handleRValue(const json &data) {
    const auto kind = data.contains("kind")
                          ? strToRValKind(data["kind"])
                          : RValKind::Other;
    switch (kind) {
    case RValKind::UnaryOperator: {
        const std::string opcode = data["opcode"];
        const auto &innerData = data["inner"][0];
        const auto res = handleRValue(innerData);
        if (std::holds_alternative<RValTypes::OtherPtr>(res) && opcode == "*") {
            const auto &otherPtr = std::get<RValTypes::OtherPtr>(res);
            return RValTypes::PtrDeref{otherPtr.id, otherPtr.name};
        }
        if (std::holds_alternative<RValTypes::Other>(res) && opcode == "&") {
            const auto &other = std::get<RValTypes::Other>(res);
            return RValTypes::VarRef{other.label};
        }
        return RValTypes::Other{opcode + getRValLabel(res)};
    }
    case RValKind::BinaryOperator: {
        const std::string opcode = data["opcode"];
        const auto &innerData = data["inner"];
        const auto lhs = handleRValue(innerData[0]);
        const auto rhs = handleRValue(innerData[1]);
        return RValTypes::Other{std::format("{} {} {}", getRValLabel(lhs), opcode, getRValLabel(rhs))};
    }
    case RValKind::DeclRefExpr: {
        const auto refDecl = data["referencedDecl"];
        if (data["type"]["qualType"].get<std::string>().back() == '*') // pointer
            return RValTypes::OtherPtr{refDecl["id"], refDecl["name"]};
        return RValTypes::Other{refDecl["name"]};
    }
    case RValKind::CallExpr: {
        const auto &innerData = data["inner"];
        const auto func = handleRValue(innerData[0]);
        const auto funcName = getRValLabel(func);
        if (funcName == "free") {
            const auto arg0 = handleRValue(innerData[1]);
            if (std::holds_alternative<RValTypes::OtherPtr>(arg0)) {
                const auto ptr = std::get<RValTypes::OtherPtr>(arg0);
                return RValTypes::CallExpr{std::format("free({})", ptr.name), false, ptr.id, ptr.name};
            }
        }
        if (funcName == "malloc") {
            const auto arg0 = handleRValue(innerData[1]);
            return RValTypes::CallExpr{"malloc(...)", true, "", ""};
        }
        return RValTypes::Other{funcName + "(...)"};
    }
    case RValKind::CXXNewExpr: {
        const std::string qualType = data["type"]["qualType"];
        return RValTypes::CallExpr{"new " + qualType.substr(0, qualType.size() - 1), true, "", ""};
    }
    case RValKind::CXXConstructExpr: {
        return RValTypes::Other{"constructor(...)"};
    }
    case RValKind::ImplicitCastExpr: {
        if (data["castKind"] == "NullToPointer")
            return RValTypes::NullPtr{};
    }
    default:
        break;
    }

    if (data.contains("inner")) {
        return handleRValue(data["inner"][0]);
    }

    std::stringstream labelss;
    if (data.contains("name"))
        labelss << data["name"].get<std::string>();
    else if (data.contains("value"))
        labelss << data["value"].get<std::string>();
    else
        labelss << "?";
    return RValTypes::Other{labelss.str()};
}

void CFG::populatePool(const json &data) {
    if (!data.contains("id")) // empty
        return;
    const std::string id = data["id"];
    const node_t node = data.contains("kind")
                            ? make_node(id, data)
                            : make_node(id);
    const auto &kind = node->metadata.kind;
    const auto &qualType = node->metadata.qualType;
    switch (kind) {
    case NodeKind::ParmVarDecl: {
        const std::string &name = data["name"];
        node->label = std::format("{} {}", qualType, name);
        if (node->metadata.isPtr)
            node->ptrActions.push_back(PtrActionTypes::Declare{node->metadata.id, name});
        return;
    }
    case NodeKind::VarDecl: {
        const std::string &name = data["name"];
        if (node->metadata.isPtr)
            node->ptrActions.push_back(PtrActionTypes::Declare{node->metadata.id, name});
        node->label = std::format("{} {}", qualType, name);
        if (data.contains("inner")) { // init value
            const auto res = handleRValue(data["inner"][0]);
            if (std::holds_alternative<RValTypes::NullPtr>(res)) {
                node->ptrActions.push_back(PtrActionTypes::AssignConst{
                    PtrActionTypes::AssignConst::AssignType::NullPtr,
                    node->metadata.id, name});
                node->label += " = nullptr";
            } else if (std::holds_alternative<RValTypes::VarRef>(res)) {
                const auto &varRef = std::get<RValTypes::VarRef>(res);
                node->ptrActions.push_back(PtrActionTypes::AssignConst{
                    PtrActionTypes::AssignConst::AssignType::Reference,
                    node->metadata.id, name});
                node->label = std::format("{} = &{}", node->label, varRef.label);
            } else if (std::holds_alternative<RValTypes::OtherPtr>(res)) {
                const auto &otherPtr = std::get<RValTypes::OtherPtr>(res);
                node->ptrActions.push_back(PtrActionTypes::AssignPtr{
                    node->metadata.id, name,
                    otherPtr.id, otherPtr.name});
                node->label = std::format("{} = {}", node->label, otherPtr.name);
            } else if (std::holds_alternative<RValTypes::CallExpr>(res)) {
                const auto &call = std::get<RValTypes::CallExpr>(res);
                node->ptrActions.push_back(PtrActionTypes::MemMgmt{
                    (call.isAlloc
                         ? PtrActionTypes::MemMgmt::MemType::Alloc
                         : PtrActionTypes::MemMgmt::MemType::Dealloc),
                    node->metadata.id, name});
                node->label = std::format("{} = {}", node->label, call.label);
            } else if (std::holds_alternative<RValTypes::Other>(res)) {
                const auto &other = std::get<RValTypes::Other>(res);
                if (node->metadata.isPtr)
                    node->ptrActions.push_back(PtrActionTypes::AssignConst{
                        PtrActionTypes::AssignConst::AssignType::Other,
                        node->metadata.id, name});
                node->label = std::format("{} = {}", node->label, other.label);
            }
        }
        return;
    }
    case NodeKind::BinaryOperator: {
        const std::string &opcode = data["opcode"];
        const auto &innerData = data["inner"];
        const auto lhs = handleRValue(innerData[0]);
        const auto rhs = handleRValue(innerData[1]);
        if (node->metadata.isPtr && opcode == "=") { // assignment to ptr
            if (!std::holds_alternative<RValTypes::OtherPtr>(lhs)) {
                return; // idk
            }
            const auto &ptr = std::get<RValTypes::OtherPtr>(lhs);
            if (std::holds_alternative<RValTypes::NullPtr>(rhs)) {
                node->ptrActions.push_back(PtrActionTypes::AssignConst{
                    PtrActionTypes::AssignConst::AssignType::NullPtr,
                    ptr.id, ptr.name});
            } else if (std::holds_alternative<RValTypes::VarRef>(rhs)) {
                node->ptrActions.push_back(PtrActionTypes::AssignConst{
                    PtrActionTypes::AssignConst::AssignType::Reference,
                    ptr.id, ptr.name});
            } else if (std::holds_alternative<RValTypes::OtherPtr>(rhs)) {
                const auto &otherPtr = std::get<RValTypes::OtherPtr>(rhs);
                node->ptrActions.push_back(PtrActionTypes::AssignPtr{
                    ptr.id, ptr.name,
                    otherPtr.id, otherPtr.name});
            } else if (std::holds_alternative<RValTypes::CallExpr>(rhs)) {
                const auto &call = std::get<RValTypes::CallExpr>(rhs);
                node->ptrActions.push_back(PtrActionTypes::MemMgmt{
                    (call.isAlloc
                         ? PtrActionTypes::MemMgmt::MemType::Alloc
                         : PtrActionTypes::MemMgmt::MemType::Dealloc),
                    ptr.id, ptr.name});
            } else if (std::holds_alternative<RValTypes::Other>(rhs)) {
                node->ptrActions.push_back(PtrActionTypes::AssignConst{
                    PtrActionTypes::AssignConst::AssignType::Other,
                    ptr.id, ptr.name});
            }
        } else if (qualType == "bool") { // branch
            const bool isEq = opcode == "==" || opcode == "!=";
            const auto &ptr = std::holds_alternative<RValTypes::OtherPtr>(lhs)
                                  ? lhs
                                  : (std::holds_alternative<RValTypes::OtherPtr>(rhs)
                                         ? rhs
                                         : RValTypes::Other{""});
            const auto &nul = std::holds_alternative<RValTypes::NullPtr>(lhs)
                                  ? lhs
                                  : (std::holds_alternative<RValTypes::NullPtr>(rhs)
                                         ? rhs
                                         : RValTypes::Other{""});
            if (isEq && !std::holds_alternative<RValTypes::Other>(ptr) &&
                !std::holds_alternative<RValTypes::Other>(nul)) {
                const auto &_ptr = std::get<RValTypes::OtherPtr>(ptr);
                node->ptrActions.push_back(PtrActionTypes::Branch{
                    (opcode == "!="
                         ? PtrActionTypes::Branch::BranchType::NotNull
                         : PtrActionTypes::Branch::BranchType::Null),
                    _ptr.id, _ptr.name});
            }
        } else if (opcode == "=" && std::holds_alternative<RValTypes::PtrDeref>(lhs)) { // assign to deref
            const auto &ptr = std::get<RValTypes::PtrDeref>(lhs);
            node->ptrActions.push_back(PtrActionTypes::Deref{ptr.id, ptr.name});
        }
        node->label = std::format("{} {} {}", getRValLabel(lhs), opcode, getRValLabel(rhs));
        return;
    }
    case NodeKind::UnaryOperator: {
        const auto res = handleRValue(data);
        if (std::holds_alternative<RValTypes::Other>(res)) {
            const auto other = std::get<RValTypes::Other>(res);
            node->label = other.label;
        }
        return; // don't care about ptr increment etc.
    }
    case NodeKind::ImplicitCastExpr: {
        const std::string &castKind = data["castKind"];
        if (qualType == "bool") { // branch
            const auto res = handleRValue(data);
            if (castKind == "PointerToBoolean") {
                if (std::holds_alternative<RValTypes::OtherPtr>(res)) {
                    const auto &ptr = std::get<RValTypes::OtherPtr>(res);
                    node->ptrActions.push_back(PtrActionTypes::Branch{
                        PtrActionTypes::Branch::BranchType::NotNull,
                        ptr.id, ptr.name});
                    node->label = ptr.name + " != nullptr";
                    return;
                }
            }
            node->label = getRValLabel(res);
        }
        break;
    }
    case NodeKind::CallExpr: {
        const auto res = handleRValue(data);
        if (std::holds_alternative<RValTypes::CallExpr>(res)) {
            const auto &call = std::get<RValTypes::CallExpr>(res);
            node->ptrActions.push_back(PtrActionTypes::MemMgmt{
                (call.isAlloc
                     ? PtrActionTypes::MemMgmt::MemType::Alloc
                     : PtrActionTypes::MemMgmt::MemType::Dealloc),
                call.id, call.name});
        }
        node->label = getRValLabel(res);
        return;
    }
    case NodeKind::ReturnStmt: {
        node->label = "return";
        if (data.contains("inner")) {
            const auto res = handleRValue(data["inner"][0]);
            if (std::holds_alternative<RValTypes::PtrDeref>(res)) {
                const auto &ptr = std::get<RValTypes::PtrDeref>(res);
                node->ptrActions.push_back(PtrActionTypes::Deref{ptr.id, ptr.name});
            } else
                std::cerr << "RETURN got " << res.index() << "\n";
            node->label = std::format("return {}", getRValLabel(res));
        }
        return;
    }
    case NodeKind::CXXDeleteExpr: {
        const auto ptr = handleRValue(data["inner"][0]);
        if (!std::holds_alternative<RValTypes::OtherPtr>(ptr))
            return;
        const auto &_ptr = std::get<RValTypes::OtherPtr>(ptr);
        node->ptrActions.push_back(PtrActionTypes::MemMgmt{
            PtrActionTypes::MemMgmt::MemType::Dealloc,
            _ptr.id, _ptr.name});
        node->label = "delete " + _ptr.name;
        return;
    }
    case NodeKind::MemberExpr: {
        const std::string memName = data["name"];
        const bool isArrow = data["isArrow"];
        const auto obj = handleRValue(data["inner"][0]);
        node->label = std::format("{}{}{}{}", getRValLabel(obj), (isArrow ? "->" : "."),
                                  memName, (qualType.contains("function") ? "(...)" : ""));
        if (isArrow) {
            auto const &ptr = std::get<RValTypes::OtherPtr>(obj);
            node->ptrActions.push_back(PtrActionTypes::Deref{ptr.id, ptr.name});
        }
        return;
    }
    case NodeKind::BranchStmt: {
        const auto joinNode = make_node(id + "_join");
        joinNode->label = ".";
        break;
    }
    case NodeKind::BreakStmt: {
        node->label = "break";
        break;
    }
    default:
        break;
    }
    if (node->label.empty())
        erase_node(id);
    if (data.contains("inner"))
        for (const auto &innerData : data["inner"])
            populatePool(innerData);
}

node_t CFG::linkNodes(const json &data, const node_t &prev) {
    if (!data.contains("id")) // empty
        return prev;
    const std::string id = data["id"];
    const std::string kind = data["kind"];
    const auto _kind = strToNodeKind(kind);
    node_t last = prev;
    if (pool.contains(id)) {
        const auto &node = pool[id].get();
        prev->next.push_back(node);
        node->prev.push_back(prev);
        last = node;
    }

    if (_kind == NodeKind::BranchStmt) {
        const auto &innerData = data["inner"];
        const auto &joinNode = pool[id + "_join"].get();
        if (kind == "IfStmt") {
            const auto condNode = linkNodes(innerData[0], last);
            const auto thenEndNode = linkNodes(innerData[1], condNode);
            if (data.contains("hasElse"))
                last = linkNodes(innerData[2], condNode); // elseEndNode
            else
                last = condNode;
            thenEndNode->next.push_back(joinNode);
            joinNode->prev.push_back(thenEndNode);
            last->next.push_back(joinNode);
            joinNode->prev.push_back(last);
            last = joinNode;
        } else if (kind == "WhileStmt") {
            const auto condNode = linkNodes(innerData[0], last);
            const auto doNode = linkNodes(innerData[1], condNode);
            doNode->next.push_back(condNode);
            condNode->prev.push_back(doNode);
            condNode->next.push_back(joinNode);
            joinNode->prev.push_back(condNode);
            last = joinNode;
        } else if (kind == "ForStmt") {
            const auto initNode = linkNodes(innerData[0], last);
            // empty index 1
            const auto condNode = linkNodes(innerData[2], initNode);
            const auto doNode = linkNodes(innerData[4], condNode);
            const auto updateNode = linkNodes(innerData[3], doNode);
            updateNode->next.push_back(condNode);
            condNode->prev.push_back(updateNode);
            condNode->next.push_back(joinNode);
            joinNode->prev.push_back(condNode);
            last = joinNode;
        }
        return last;
    }

    if (data.contains("inner"))
        for (const auto &innerData : data["inner"])
            last = linkNodes(innerData, last);
    switch (_kind) {
    default:
        break;
    }
    return last;
}

void CFG::handleJumps() {
    std::queue<node_t> q, jumpQ;
    std::unordered_set<node_t> vis;
    q.push(begin);
    while (!q.empty()) {
        const auto node = q.front();
        q.pop();
        if (vis.contains(node))
            continue;
        vis.insert(node);
        if (node->metadata.kind == NodeKind::BreakStmt || node->metadata.kind == NodeKind::ReturnStmt)
            jumpQ.push(node);
        for (const auto &succ : node->next)
            if (!vis.contains(succ))
                q.push(succ);
    }
    while (!jumpQ.empty()) {
        const auto jumpNode = jumpQ.front();
        jumpQ.pop();
        if (jumpNode->metadata.kind == NodeKind::BreakStmt) {
            std::queue<node_t> q;
            q.push(jumpNode);
            vis.clear();
            while (!q.empty()) {
                const auto n = q.front();
                q.pop();
                vis.insert(n);
                if (n->next.size() > 1) {
                    jumpNode->next.clear();
                    const auto &joinNode = n->next.back();
                    jumpNode->next.push_back(joinNode);
                    joinNode->prev.push_back(jumpNode);
                    break;
                }
                for (const auto &pred : n->prev)
                    if (!vis.contains(pred))
                        q.push(pred);
            }
        } else if (jumpNode->metadata.kind == NodeKind::ReturnStmt) {
            jumpNode->next.clear();
            jumpNode->next.push_back(end);
            end->prev.push_back(jumpNode);
        }
    }
}

CFG::CFG(const json &ast) {
    populatePool(ast);
    begin = make_node("BEGIN");
    begin->label = "BEGIN";
    const auto last = linkNodes(ast, begin);
    end = make_node("END");
    end->label = "END";
    last->next.push_back(end);
    end->prev.push_back(last);
    handleJumps();
}

} // namespace cfg
