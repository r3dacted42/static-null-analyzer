#include "cfg.hpp"
#include "dfa.hpp"
#include "dot.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

json collectIssues(std::unordered_set<cfg::node_t> nodes) {
    json res = json::array();
    for (const auto &n : nodes) {
        for (const auto &pa : n->ptrActions)
            if (std::holds_alternative<cfg::PtrActionTypes::Deref>(pa)) {
                const auto &deref = std::get<cfg::PtrActionTypes::Deref>(pa);
                res.push_back(json{
                    {"start_offset", n->metadata.offsetBegin},
                    {"end_offset", n->metadata.offsetEnd},
                    {"message", "potential nullptr dereference of " + deref.name}});
            }
    }
    return res;
}

int main(int argc, const char **argv) {
    if (argc < 4) {
        std::cerr << "usage: " << argv[0] << " <ast_path> <dot_path> <issues_path>\n";
        return 1;
    }

    std::string ast_path(argv[1]), dot_path(argv[2]), issues_path(argv[3]);
    std::ifstream json_file(ast_path);
    if (!json_file.is_open()) {
        std::cerr << "error: could not open file " << ast_path << "\n";
        return 1;
    }

    json ast;
    try {
        ast = json::parse(json_file);
    } catch (json::parse_error &e) {
        std::cerr << "error: failed to parse AST JSON file " << ast_path << "\n"
            << "function name may be missing or wrong\n";
        return 1;
    }

    if (!ast.is_object() || !ast.contains("id")) {
        std::cerr << "error: AST JSON is not a valid object\n";
        return 1;
    }

    cfg::CFG cfg(ast);

    dfa::NullPtrAnalyzer nullAnalyzer;
    const auto res = nullAnalyzer.analyze(cfg.getBeginNode());

    dot::DotExporter exporter;
    std::string dot_output = exporter.toDot(cfg.getBeginNode(), res);

    std::ofstream dot_file(dot_path);
    dot_file << dot_output;

    std::ofstream issues_file(issues_path);
    issues_file << collectIssues(res).dump(4);

    return 0;
}
