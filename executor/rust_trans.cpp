//
// Created by pro on 2025/9/9.
//

#include "istool/basic/config.h"
#include "istool/incre/io/incre_json.h"
#include "istool/incre/io/incre_to_rust.h"
#include <iostream>
#include <fstream>
#include "glog/logging.h"

using namespace incre;

DEFINE_string(input, "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/mts-res.f", "The absolute path of the benchmark file (.f)");
DEFINE_string(output, "//Users/pro/Desktop/work/2025A/rust_test/src/main.rs", "The absolute path of the output rust file (.rs)");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    std::string path = FLAGS_input;
    IncreProgram prog = io::parseFromF(path);

    try {
        std::ofstream out(FLAGS_output);
        incre::rust::program2Rust(out, prog.get());
        out.close();
        auto command = std::format("rustfmt {}", FLAGS_output);
        std::system(command.c_str());
        std::cout << command << std::endl;
    } catch (const incre::rust::TranslationError& e) {
        std::cout << e.message << std::endl;
    }

}