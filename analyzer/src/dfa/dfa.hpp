#pragma once

#include "cfg.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dfa {

using node_t = cfg::node_t;

enum class PtrState : short {
    K_Unknown,
    N_NotNull,
    B_NotNull,
    Z_Null,
    B_Null,
    T_MaybeNull,
};

using PtrTable = std::unordered_map<std::string, PtrState>;

bool operator==(const PtrTable &, const PtrTable &);
bool operator!=(const PtrTable &, const PtrTable &);

class NullPtrAnalyzer {
  public:
    std::unordered_set<node_t> analyze(node_t);

  private:
    PtrState join(const PtrState &, const PtrState &);
    PtrTable join(const PtrTable &, const PtrTable &, uint);
    PtrTable flow(const node_t &, const PtrTable &);
};

} // namespace dfa
