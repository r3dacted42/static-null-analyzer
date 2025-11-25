#pragma once

#include "cfg.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace dot {

using node_t = cfg::node_t;

class DotExporter {
  public:
    std::string toDot(node_t, std::unordered_set<node_t>);

  private:
    std::string getNodeId(node_t);
    std::string escapeDotLabel(const std::string &);

    std::unordered_map<node_t, std::string> node_ids;
    int node_counter = 0;
};

} // namespace dot