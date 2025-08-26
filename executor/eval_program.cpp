//
// Created by pro on 2025/8/25.
//

#include "istool/incre/io/incre_json.h"
#include "istool/incre/language/incre_syntax.h"
#include "istool/incre/language/incre_semantics.h"
#include "istool/incre/language/incre_rewriter.h"
#include "istool/incre/io/incre_printer.h"
#include "glog/logging.h"
#include <fstream>
#include <iostream>

using namespace incre;
using namespace incre::syntax;
using namespace incre::types;
using namespace incre::semantics;

class TypeLabelRemover: public IncreTypeRewriter {
public:
    virtual Ty _rewrite(TyCompress *type, const Ty &_type) {
        return rewrite(type->body);
    }
};

class TermLabelRemover: public IncreTermRewriter {
public:

    virtual Term _rewrite(TmLabel* term, const Term& _term) {
        return rewrite(term->body);
    }

    virtual Term _rewrite(TmUnlabel* term, const Term& _term) {
        return rewrite(term->body);
    }

    virtual Term _rewrite(TmRewrite* term, const Term& _term) {
        return rewrite(term->body);
    }
};

DEFINE_string(benchmark, "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/res.f", "The absolute path of the benchmark file (.sl)");
DEFINE_string(output, "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/eval.f", "The absolute path the output file");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    std::string path = FLAGS_benchmark, target = FLAGS_output;
    IncreProgram prog = io::parseFromF(path);

    prog = rewriteProgram(prog.get(), [](){return new TypeLabelRemover();}, [](){return new TermLabelRemover();});
    prog = runProgram(prog.get(), new DefaultEvaluator(), new DefaultIncreTypeChecker());

    incre::io::printProgram2F(target, prog.get());
}