#pragma once

#include "json.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cfg {

using json = nlohmann::json;

struct CFGNode;
using node_ptr = std::shared_ptr<CFGNode>;

struct Metadata {
    std::string id;
    std::string kind;
    uint line;

    Metadata() : line(-1) {}
};

enum NodePtrType {
    ANY,
    DECL,
    ASSIGN,
    DEREF,
    B_NULL,
    B_NOT_NULL
};

enum PtrState {
    UNKNOWN,
    NULLPTR,
    PTR_REF,
    REFERENCE,
    ALLOC,
    DEALLOC,
    DONTCARE
};

struct PtrData {
    NodePtrType n_type;
    std::string refId;
    std::string name;
    PtrState state;
};

struct CFGNode {
    std::vector<PtrData> ptrData;
    Metadata metadata;
    std::string label;
    std::vector<node_ptr> next;
    bool hasReturn = false;
};

struct RValue {
    std::vector<PtrData> ptrData;
    std::string label;
};

class CFG {
  public:
    CFG(const json &ast);
    node_ptr getBeginNode() { return begin; }
    node_ptr getEndNode() { return end; }

  private:
    RValue handleExpr(const json &);
    node_ptr recWalkAST(const json &, node_ptr = nullptr);

    node_ptr begin, end;
};

} // namespace cfg
