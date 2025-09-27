//
// Created by pro on 2025/9/9.
//

#include "istool/incre/io/incre_to_rust.h"
#include "istool/incre/language/incre_types.h"
#include "glog/logging.h"

using namespace incre::syntax;
using namespace incre::rust;
using namespace incre::types;

namespace {
    void processCommand(incre::IncreProgramWalker* walker, const incre::Command& command) {
        auto program = new incre::IncreProgramData({command}, {});
        walker->walkThrough(program);
        delete program;
    }
}

Ty util::unfoldBoundVariable(const syntax::Ty &raw_type) {
    IncreTypeRewriter rewriter;
    return rewriter.rewrite(raw_type);
}

std::string util::getAuxFuncName() {
    static int index = 0;
    return "aux" + std::to_string(index);
}

void incre::rust::program2Rust(std::ostream &out, incre::IncreProgramData *program) {
    auto* walker = new DefaultContextBuilder(nullptr, new DefaultIncreTypeChecker());

    for (auto& command: program->commands) {
        switch (command->getType()) {
            case CommandType::EVAL: break;
            case CommandType::DEF_IND: {
                auto* ci = dynamic_cast<CommandDef*>(command.get());
                util::indDef2Rust(out, ci);
                break;
            }
            case CommandType::DECLARE: break;
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                std::unordered_map<std::string, util::RecursiveFunctionInfo> info_map;
                auto result = util::function2Rust(cb->term, walker->ctx, command->name, info_map);

                for (auto& function_result: result) {
                    out << function_result << "\n";
                }
            }
        }

        processCommand(walker, command);
    }
}