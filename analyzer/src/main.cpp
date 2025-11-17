#include <iostream>
#include <fstream>
#include "json.hpp"
#include "cfg.hpp"
#include "dot.hpp"

using json = nlohmann::json;

int main(int argc, const char **argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <file.json>\n";
        return 1;
    }

    std::string filename(argv[1]);
    std::ifstream json_file(filename);
    if (!json_file.is_open()) {
        std::cerr << "error: could not open file " << filename << "\n";
        return 1;
    }

    json ast;
    try {
        ast = json::parse(json_file);
    } catch (json::parse_error &e) {
        std::cerr << "error: failed to parse JSON file " << filename << "\n";
        return 1;
    }

    cfg::CFG cfg(ast);
    cfg::DotExporter exporter;

    std::string dot_output = exporter.toDot(cfg.getBeginNode());
    std::cout << dot_output;
    // std::ofstream out_file("graph.dot");
    // out_file << dot_output;

    return 0;
}
