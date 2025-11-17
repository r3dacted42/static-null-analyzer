#pragma once

#include "cfg.hpp"
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

namespace cfg {

class DotExporter {
  public:
    std::string toDot(node_ptr begin_node);

  private:
    std::string getNodeId(node_ptr node);
    std::string escapeDotLabel(const std::string &s);

    std::map<node_ptr, std::string> node_ids;
    int node_counter = 0;
};

} // namespace cfg