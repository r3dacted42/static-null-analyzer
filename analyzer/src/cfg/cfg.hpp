#pragma once

#include "json.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cfg {

using json = nlohmann::json;

struct CFGNode;
using node_t = CFGNode *;

enum class NodeKind {
    Other,
    BranchStmt, // if, for, while
    FunctionDecl,
    ParmVarDecl,
    CompoundStmt,
    VarDecl,
    BinaryOperator,
    UnaryOperator,
    ImplicitCastExpr,
    CallExpr,
    ReturnStmt,
    CXXDeleteExpr,
    BreakStmt,
    MemberExpr,
};

struct Metadata {
    std::string id;
    uint offsetBegin;
    uint offsetEnd;
    NodeKind kind = NodeKind::Other;
    bool isPtr = false;
    std::string qualType;
};

namespace PtrActionTypes {

struct None {};

struct Declare {
    std::string refId;
    std::string name;
};

struct AssignConst {
    enum class AssignType {
        NullPtr,
        Reference,
        Other,
    } assignType;
    std::string refId;
    std::string name;
};

struct AssignPtr {
    std::string refIdL;
    std::string nameL;
    std::string refIdR;
    std::string nameR;
};

struct MemMgmt {
    enum class MemType {
        Alloc,
        Dealloc,
    } memType;
    std::string refId;
    std::string name;
};

struct Deref {
    std::string refId;
    std::string name;
};

struct Branch {
    enum class BranchType {
        NotNull,
        Null,
    } branchType;
    std::string refId;
    std::string name;
};

} // namespace PtrActionTypes

using PtrAction = std::variant<PtrActionTypes::None,
                               PtrActionTypes::Declare,
                               PtrActionTypes::AssignConst,
                               PtrActionTypes::AssignPtr,
                               PtrActionTypes::MemMgmt,
                               PtrActionTypes::Deref,
                               PtrActionTypes::Branch>;

std::string getPtrActLabel(const PtrAction &);

struct CFGNode {
    Metadata metadata;
    std::string label;
    std::vector<node_t> next;
    std::vector<PtrAction> ptrActions;
};

enum class RValKind {
    Other,
    UnaryOperator,
    BinaryOperator,
    DeclRefExpr,
    CallExpr,
    CXXNewExpr,
    CXXConstructExpr,
    ImplicitCastExpr,
};

namespace RValTypes {

struct Other {
    std::string label;
};

struct VarRef {
    std::string label;
};

struct CallExpr {
    std::string label;
    bool isAlloc;
    std::string id;
    std::string name;
};

struct OtherPtr {
    std::string id;
    std::string name;
};

struct PtrDeref {
    std::string id;
    std::string name;
};

struct NullPtr {};

} // namespace RValTypes

using RValue = std::variant<RValTypes::Other,
                            RValTypes::VarRef,
                            RValTypes::CallExpr,
                            RValTypes::OtherPtr,
                            RValTypes::PtrDeref,
                            RValTypes::NullPtr>;

std::string getRValLabel(const RValue &);

class CFG {
  public:
    CFG(const json &);
    node_t getBeginNode() { return begin; }
    node_t getEndNode() { return end; }

  private:
    node_t make_node(const std::string &);
    node_t make_node(const std::string &, const json &);
    void erase_node(const std::string &);

    RValue handleRValue(const json &);
    void populatePool(const json &);
    node_t linkNodes(const json &, const node_t &prev);

    node_t begin, end;
    std::unordered_map<std::string, std::unique_ptr<CFGNode>> pool;
};

} // namespace cfg
