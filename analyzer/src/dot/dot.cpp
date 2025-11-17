#include "dot.hpp"
#include <sstream> // Using sstream for C++17/20 compatible string formatting

namespace cfg {

/**
 * @brief Gets a unique DOT-language ID for a node (e.g., "Node0", "Node1").
 */
std::string DotExporter::get_node_id(node_ptr node) {
    // We use the pointer's memory address as the key.
    if (node_ids.find(node) == node_ids.end()) {
        node_ids[node] = "Node" + std::to_string(node_counter++);
    }
    return node_ids[node];
}

/**
 * @brief Escapes special characters for a DOT label string.
 */
std::string DotExporter::escape_dot(const std::string &s) {
    std::string result;
    result.reserve(s.length()); // Reserve space for efficiency

    for (const char c : s) {
        if (c == '"' || c == '\\') {
            result.push_back('\\');
        }
        result.push_back(c);
    }
    return result;
}

/**
 * @brief Performs a graph traversal (BFS) to build the DOT string.
 */
std::string DotExporter::to_dot(node_ptr begin_node) {
    std::stringstream nodes; // Holds all node declarations (e.g., Node0 [label="..."];)
    std::stringstream edges; // Holds all edge declarations (e.g., Node0 -> Node1;)

    std::set<node_ptr> visited;    // To avoid re-processing and infinite loops
    std::queue<node_ptr> worklist; // Standard BFS work queue

    worklist.push(begin_node);

    while (!worklist.empty()) {
        node_ptr current = worklist.front();
        worklist.pop();

        // Check if we've already processed this node
        if (visited.count(current)) {
            continue;
        }
        visited.insert(current);

        std::string current_id = get_node_id(current);

        // --- 1. Declare the Node ---
        std::stringstream label_ss;
        // Add line number and escaped label
        label_ss << "L" << current->metadata.line << ": " << escape_dot(current->label);

        // Optionally, add pointer data to the label
        if (!current->ptrData.empty()) {
            label_ss << "\\n---"; // Newline in DOT
            for (const auto &pd : current->ptrData) {
                // You can customize this to show more PtrData info
                label_ss << "\\n"
                         << pd.name << " (ref: " << pd.refId << ")";
            }
        }

        nodes << "  " << current_id << " [label=\"" << label_ss.str() << "\"];\n";

        // --- 2. Declare its Edges ---
        for (const auto &successor : current->next) {
            if (successor) {
                std::string successor_id = get_node_id(successor);
                edges << "  " << current_id << " -> " << successor_id << ";\n";

                // Add the successor to the queue *if* we haven't seen it
                if (visited.find(successor) == visited.end()) {
                    worklist.push(successor);
                }
            }
        }
    }

    // --- 3. Assemble the final DOT string ---
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