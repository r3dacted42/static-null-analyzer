#include "dot.hpp"
#include <sstream>

namespace cfg {

std::string DotExporter::getNodeId(node_ptr node) {
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

std::string DotExporter::toDot(node_ptr begin_node) {
    std::stringstream nodes;
    std::stringstream edges;
    std::set<node_ptr> visited;
    std::queue<node_ptr> q;
    q.push(begin_node);
    while (!q.empty()) {
        node_ptr current = q.front();
        q.pop();
        if (visited.count(current)) {
            continue;
        }
        visited.insert(current);
        std::string current_id = getNodeId(current);
        const bool isBranch = current->next.size() > 1;
        std::stringstream label_ss;
        label_ss << escapeDotLabel(current->label);
        // if (!current->ptrData.empty()) {
        //     label_ss << "\\n---"; // Newline in DOT
        //     for (const auto &pd : current->ptrData) {
        //         // You can customize this to show more PtrData info
        //         label_ss << "\\n"
        //                  << pd.name << " (ref: " << pd.refId << ")";
        //     }
        // }
        nodes << "  " << current_id << " [label=\"" << label_ss.str() << "\"";
        if (isBranch)
            nodes << ", shape=oval];\n";
        else
            nodes << "];\n";
        int edgeIdx = 0;
        for (const auto &successor : current->next) {
            if (successor) {
                std::string successor_id = getNodeId(successor);
                edges << "  " << current_id << " -> " << successor_id;
                if (isBranch)
                    edges << " [label=\"" << (edgeIdx == 0 ? "T" : "F") << "\"];\n";
                else
                    edges << ";\n";
                if (visited.find(successor) == visited.end())
                    q.push(successor);
            }
            edgeIdx++;
        }
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

} // namespace cfg