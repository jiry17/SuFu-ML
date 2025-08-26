//
// Created by pro on 2024/3/29.
//

#include "istool/incre/autolabel/incre_autolabel.h"
#include "istool/incre/io/incre_printer.h"
#include "istool/incre/language/incre_util.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::autolabel;

IncreProgram autolabel::labelProgram(const IncreProgram &program) {
    auto* z3_ctx = new Z3Context();
    auto* checker = new SymbolicIncreTypeChecker(z3_ctx);

    util::collectHardConstraints(program.get(), checker);
    auto obj = util::collectSoftConstraints(program.get(), z3_ctx);

    z3::optimize solver(z3_ctx->ctx);
    solver.add(z3_ctx->cons_list);

    // Minimize the number of covered AST nodes
    solver.push();
    solver.minimize(obj);
    if (solver.check() != z3::sat) {
        LOG(FATAL) << "Autolabel failed: Cannot find any valid elaborations";
    }
    auto first_staged_model = solver.get_model();
    solver.pop();
    auto min_obj = first_staged_model.eval(obj).get_numeral_int();
    solver.add(obj == min_obj);

    // Minimize the number of inserted operators
    z3::expr op_num = z3_ctx->ctx.int_val(0);
    for (auto& [_, var]: z3_ctx->flip_map) {
        op_num = op_num + z3::ite(var, z3_ctx->ctx.int_val(1), z3_ctx->ctx.int_val(0));
    }
    for (auto& [_, var]: z3_ctx->rewrite_map) {
        op_num = op_num + z3::ite(var, z3_ctx->ctx.int_val(1), z3_ctx->ctx.int_val(0));
    }
    solver.minimize(op_num);
    assert(solver.check() == z3::sat);

    auto res = util::rewriteUseResult(program.get(), z3_ctx, solver.get_model());
    res = ::incre::util::extractIrrelevantSubTermsForProgram(res.get());
    return res;
}