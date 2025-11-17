#pragma once

#include "cfg.hpp" // Your existing CFG header
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

namespace cfg {

/**
 * @class DotExporter
 * @brief Traverses a built CFG and exports it to the Graphviz DOT language.
 */
class DotExporter {
  public:
    /**
     * @brief Exports the CFG starting from the given node.
     * @param begin_node The "BEGIN" node of your CFG.
     * @return A std::string containing the complete .dot file contents.
     */
    std::string to_dot(node_ptr begin_node);

  private:
    // Map to get a unique string ID (e.g., "Node0", "Node1") for each node pointer
    std::map<node_ptr, std::string> node_ids;
    int node_counter = 0;

    // Helper to get or create a unique ID for a node
    std::string get_node_id(node_ptr node);

    // Helper to escape characters that are special in DOT labels (like " and \)
    std::string escape_dot(const std::string &s);
};

} // namespace cfg