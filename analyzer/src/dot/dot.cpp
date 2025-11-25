#include "dot.hpp"
#include <queue>
#include <sstream>
#include <unordered_set>

namespace dot {

std::string DotExporter::getNodeId(node_t node) {
    if (node_ids.find(node) == node_ids.end()) {
        node_ids[node] = "Node" + std::to_string(node_counter++);
    }
    return node_ids[node];
}

std::string DotExporter::escapeDotLabel(const std::string &s) {
    std::string result;
    result.reserve(s.length());
    for (const char c : s) {
        if (c == '"' || c == '\\') {
            result.push_back('\\');
        }
        result.push_back(c);
    }
    return result;
}

std::string DotExporter::toDot(node_t begin_node, std::unordered_set<node_t> probSet) {
    std::stringstream nodes;
    std::stringstream edges;
    std::unordered_set<node_t> visited;
    std::queue<node_t> q;
    q.push(begin_node);
    while (!q.empty()) {
        node_t current = q.front();
        q.pop();
        if (visited.count(current)) {
            continue;
        }
        visited.insert(current);
        std::string current_id = getNodeId(current);
        const bool isBranch = current->next.size() > 1;
        std::stringstream label_ss;
        label_ss << escapeDotLabel(current->label);
        if (!current->ptrActions.empty()) {
            label_ss << "\\n---\\n";
            for (const auto &pa : current->ptrActions) {
                label_ss << cfg::getPtrActLabel(pa) << "\\n";
            }
        }
        nodes << "  " << current_id << " [label=\"" << label_ss.str() << "\"";
        if (isBranch)
            nodes << ", shape=oval";
        if (probSet.contains(current))
            nodes << ", color=red];\n";
        else
            nodes << "];\n";
        int edgeIdx = 0;
        for (const auto &succ : current->next) {
            if (succ) {
                std::string succ_id = getNodeId(succ);
                edges << "  " << current_id << " -> " << succ_id;
                if (isBranch)
                    edges << " [label=\"" << (edgeIdx == 0 ? "T" : "F") << "\"];\n";
                else
                    edges << ";\n";
                if (visited.find(succ) == visited.end())
                    q.push(succ);
            }
            edgeIdx++;
        }
        // for (const auto &pred : current->prev) {
        //     if (pred) {
        //         std::string pred_id = getNodeId(pred);
        //         edges << "  " << current_id << " -> " << pred_id << " [style=\"dashed\"]";
        //         if (visited.find(pred) == visited.end())
        //             q.push(pred);
        //     }
        // }
    }

    std::stringstream final_dot;
    final_dot << "digraph CFG {\n";
    final_dot << "  node [shape=box, fontname=\"Helvetica\"];\n";
    final_dot << "  edge [fontname=\"Helvetica\"];\n";
    final_dot << "  // Nodes\n";
    final_dot << nodes.rdbuf();
    final_dot << "\n  // Edges\n";
    final_dot << edges.rdbuf();
    final_dot << "}\n";
    return final_dot.str();
}

} // namespace dot