#include "dfa.hpp"
#include <cstdio>
#include <iostream>
#include <queue>

namespace dfa {

PtrState NullPtrAnalyzer::join(const PtrState &l, const PtrState &r) {
    if ((l == PtrState::N_NotNull && r == PtrState::Z_Null) &&
        (l == PtrState::Z_Null && r == PtrState::N_NotNull))
        return PtrState::T_MaybeNull;
    return (l > r ? l : r);
}

PtrTable NullPtrAnalyzer::join(const PtrTable &l, const PtrTable &r, uint nodeIdx) {
    PtrTable res;
    for (const auto &[id, state] : l) {
        if (state == PtrState::B_NotNull)
            res[id] = (nodeIdx ? PtrState::Z_Null : PtrState::N_NotNull);
        else if (state == PtrState::B_Null)
            res[id] = (nodeIdx ? PtrState::N_NotNull : PtrState::Z_Null);
        else
            res[id] = r.contains(id) ? join(state, r.at(id)) : state;
    }
    for (const auto &[id, state] : r) {
        if (!res.contains(id)) {
            if (state == PtrState::B_NotNull)
                res[id] = (nodeIdx ? PtrState::Z_Null : PtrState::N_NotNull);
            else if (state == PtrState::B_Null)
                res[id] = (nodeIdx ? PtrState::N_NotNull : PtrState::Z_Null);
            else
                res[id] = state;
        }
    }
    return res;
}

PtrTable NullPtrAnalyzer::flow(const node_t &node, const PtrTable &nodeIn) {
    PtrTable nodeOut;
    for (const auto &[id, state] : nodeIn)
        nodeOut[id] = state;
    for (const auto &pa : node->ptrActions) {
        if (std::holds_alternative<cfg::PtrActionTypes::Declare>(pa)) {
            const auto &decl = std::get<cfg::PtrActionTypes::Declare>(pa);
            if (!nodeOut.contains(decl.refId))
                nodeOut[decl.refId] = PtrState::K_Unknown;
        } else if (std::holds_alternative<cfg::PtrActionTypes::AssignConst>(pa)) {
            const auto &assign = std::get<cfg::PtrActionTypes::AssignConst>(pa);
            switch (assign.assignType) {
            case cfg::PtrActionTypes::AssignConst::AssignType::NullPtr:
                nodeOut[assign.refId] = PtrState::Z_Null;
                break;
            case cfg::PtrActionTypes::AssignConst::AssignType::Reference:
                nodeOut[assign.refId] = PtrState::N_NotNull;
                break;
            default:
                nodeOut[assign.refId] = PtrState::T_MaybeNull;
            }
        } else if (std::holds_alternative<cfg::PtrActionTypes::AssignPtr>(pa)) {
            const auto &assign = std::get<cfg::PtrActionTypes::AssignPtr>(pa);
            nodeOut[assign.refIdL] = nodeIn.contains(assign.refIdR)
                                         ? nodeIn.at(assign.refIdR)
                                         : PtrState::T_MaybeNull;
        } else if (std::holds_alternative<cfg::PtrActionTypes::MemMgmt>(pa)) {
            const auto &mgmt = std::get<cfg::PtrActionTypes::MemMgmt>(pa);
            switch (mgmt.memType) {
            case cfg::PtrActionTypes::MemMgmt::MemType::Alloc:
                nodeOut[mgmt.refId] = PtrState::T_MaybeNull;
                break;
            case cfg::PtrActionTypes::MemMgmt::MemType::Dealloc:
                nodeOut[mgmt.refId] = PtrState::Z_Null;
                break;
            }
        } else if (std::holds_alternative<cfg::PtrActionTypes::Branch>(pa)) {
            const auto &branch = std::get<cfg::PtrActionTypes::Branch>(pa);
            switch (branch.branchType) {
            case cfg::PtrActionTypes::Branch::BranchType::NotNull:
                nodeOut[branch.refId] = PtrState::B_NotNull;
                break;
            case cfg::PtrActionTypes::Branch::BranchType::Null:
                nodeOut[branch.refId] = PtrState::B_Null;
                break;
            }
        }
    }
    return nodeOut;
}

bool operator==(const PtrTable &l, const PtrTable &r) {
    if (l.size() != r.size())
        return false;
    for (const auto &[id, state] : l)
        if (!r.contains(id) || r.at(id) != state)
            return false;
    return true;
}

bool operator!=(const PtrTable &l, const PtrTable &r) {
    return !operator==(l, r);
}

std::ostream &operator<<(std::ostream &stream, const PtrState &state) {
    switch (state) {
    case PtrState::K_Unknown:
        stream << "Unknown";
        break;
    case PtrState::N_NotNull:
        stream << "Not Null";
        break;
    case PtrState::B_NotNull:
        stream << "Branch Not Null";
        break;
    case PtrState::Z_Null:
        stream << "Null";
        break;
    case PtrState::B_Null:
        stream << "Branch Null";
        break;
    case PtrState::T_MaybeNull:
        stream << "Maybe Null";
        break;
    }
    return stream;
}

std::unordered_set<node_t> NullPtrAnalyzer::analyze(node_t begin_node) {
    std::unordered_map<node_t, std::unordered_map<std::string, PtrState>> input;
    std::unordered_map<std::string, std::string> symTable;
    std::queue<node_t> workList;
    workList.push(begin_node);
    while (!workList.empty()) {
        const auto node = workList.front();
        workList.pop();
        const auto output = flow(node, input[node]);
        for (const auto &pa : node->ptrActions)
            if (std::holds_alternative<cfg::PtrActionTypes::Declare>(pa)) {
                const auto &decl = std::get<cfg::PtrActionTypes::Declare>(pa);
                symTable[decl.refId] = decl.name;
            }
        uint succIdx = 0;
        for (const auto &succ : node->next) {
            if (!input.contains(succ) || output != input[succ] && succ != node) {
                input[succ] = join(output, input[succ], succIdx);
                workList.push(succ);
                for (const auto &pa : node->ptrActions)
                if (std::holds_alternative<cfg::PtrActionTypes::Declare>(pa)) {
                    const auto &decl = std::get<cfg::PtrActionTypes::Declare>(pa);
                    symTable[decl.refId] = decl.name;
                }
                // debug
                // std::cerr << succ->label << " :\n";
                // for (const auto &[id, state] : input[succ]) {
                //     std::cerr << symTable[id] << " : " << state << "\n";
                // }
                // std::cerr << "\n";
            }
            succIdx++;
        }
        // std::cerr << "-----------------------------------\n";
        // getchar();
    }

    for (const auto &[node, table] : input) {
        std::cerr << node->label << " :\n";
        for (const auto &[id, state] : table) {
            std::cerr << symTable[id] << " : " << state << "\n";
        }
        std::cerr << "\n";
    }

    std::unordered_set<node_t> res;
    for (const auto &[node, table] : input) {
        for (const auto &[id, state] : table)
            if (state > PtrState::B_NotNull)
                for (const auto &pa : node->ptrActions)
                    if (std::holds_alternative<cfg::PtrActionTypes::Deref>(pa)) {
                        const auto &deref = std::get<cfg::PtrActionTypes::Deref>(pa);
                        if (deref.refId == id)
                            res.insert(node);
                    }
    }
    return res;
}

} // namespace dfa
