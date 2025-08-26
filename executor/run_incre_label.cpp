//
// Created by pro on 2022/9/18.
//

#include "istool/basic/config.h"
#include "istool/incre/io/incre_json.h"
#include "istool/incre/language/incre_util.h"
#include "istool/incre/io/incre_printer.h"
#include "istool/incre/language/incre_program.h"
#include "istool/incre/analysis/incre_instru_info.h"
#include "istool/incre/autolifter/incre_autolifter_solver.h"
#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "istool/incre/autolabel/incre_autolabel.h"
#include "istool/incre/io/incre_printer.h"
#include "istool/solver/polygen/lia_solver.h"
#include <iostream>
#include "glog/logging.h"

using namespace incre;

DEFINE_string(benchmark, "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/mts.f", "The absolute path of the benchmark file (.sl)");
DEFINE_string(output, "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/mts-res.f", "The absolute path of the output file");
DEFINE_bool(autolabel, true, "Whether automatically generate annotations");
DEFINE_bool(mark_rewrite, false, "Whether to mark the sketch holes.");
DEFINE_bool(scalar, true, "Whether consider only scalar expressions when filling sketch holes");
DEFINE_string(stage_output_file, "", "Only used in online demo");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    std::string path = FLAGS_benchmark, target = FLAGS_output;
    bool is_autolabel = FLAGS_autolabel;
    bool is_highlight_replace = FLAGS_mark_rewrite;
    global::KStageInfoPath = FLAGS_stage_output_file;

    IncreProgram prog = io::parseFromF(path);

    if (is_autolabel) {
        global::printStageResult("Start stage 0/2: generating annotations.");
        prog = incre::autolabel::labelProgram(prog);
        global::printStageResult("Stage 0/2 Finished.");
    }
    incre::io::printProgram(prog.get());

    auto env = std::make_shared<Env>();
    env->setConst(solver::lia::KIsGurobiName, BuildData(Bool, false));
    incre::config::applyConfig(prog.get(), env.get());

    auto ctx = buildContext(prog.get(), [](){return new incre::semantics::DefaultEvaluator();},
                            [](){return new types::DefaultIncreTypeChecker();});
    ctx->ctx.printTypes();
    auto incre_info = incre::analysis::buildIncreInfo(prog.get(), env.get());

    std::cout << std::endl << "Rewrite infos" << std::endl;
    for (auto& rewrite_info: incre_info->rewrite_info_list) {
        std::cout << rewrite_info.index << " ";
        std::cout << rewrite_info.command_id << " ";
        std::cout << "{"; bool is_start = true;
        for (auto& var_info: rewrite_info.inp_types) {
            if (!is_start) std::cout << ", ";
            std::cout << var_info.first + " @ " + var_info.second->toString();
            is_start = false;
        }
        std::cout << "} -> " << rewrite_info.oup_type->toString() << std::endl;
    }
    std::cout << std::endl;

    for (int i = 0; i < 10; ++i) {
        std::cout << "Start # " << i << ": " << incre_info->example_pool->generateStart().first->toString() << std::endl;
    }
    for (int i = 0; i < 10; ++i) {
        incre_info->example_pool->generateSingleExample();
    }
    for (int index = 0; index < incre_info->rewrite_info_list.size(); ++index) {
        std::cout << "examples collected for #" << index << std::endl;
        for (int i = 0; i < 5 && i < incre_info->example_pool->example_pool[index].size(); ++i) {
            auto& example = incre_info->example_pool->example_pool[index][i];
            std::cout << "  " << example->toString() << std::endl;
        }
    }

    std::cout << std::endl;

    incre_info->component_pool.print();

    auto* solver = new IncreAutoLifterSolver(incre_info, env);
    auto res = solver->solve();

    res.print();
    auto res_prog = rewriteWithIncreSolution(incre_info->program.get(), res, is_highlight_replace);

    res_prog = incre::util::removeTrivialLetForProgram(res_prog.get());

    incre::io::printProgram2F(target, res_prog.get());
}