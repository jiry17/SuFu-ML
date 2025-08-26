//
// Created by pro on 2024/3/30.
//

#include "istool/incre/autolabel/incre_autolabel.h"
#include "istool/incre/language/incre_util.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::autolabel;

namespace {
    z3::expr _collectMovableCondition(TermData* term, Z3Context* ctx, bool is_consider_rewrite) {
        auto flip_it = ctx->flip_map.find(term);
        auto rewrite_it = ctx->rewrite_map.find(term);

        auto res = (flip_it == ctx->flip_map.end()) ? ctx->KTrue : !flip_it->second;
        for (auto& sub_term: getSubTerms(term)) res = res & _collectMovableCondition(sub_term.get(), ctx, true);
        if (rewrite_it != ctx->rewrite_map.end() && is_consider_rewrite) res = res | rewrite_it->second;

        return res;
    }

    void _collectSoftConstraint(TermData* term, Z3Context* ctx, const z3::expr& outer_cond, const z3::expr& inner_cond, std::vector<z3::expr>& res) {
        auto movable_cond = _collectMovableCondition(term, ctx, false);
        auto new_inner_cond = inner_cond;
        auto it = ctx->rewrite_map.find(term);
        if (it != ctx->rewrite_map.end()) new_inner_cond = new_inner_cond | it->second;
        // LOG(INFO) << "Collect for " << term->toString();
        // LOG(INFO) << "  " << outer_cond.to_string() << " " << inner_cond.to_string() << " " << movable_cond.to_string();
        res.push_back(outer_cond | (new_inner_cond & !movable_cond));

        for (auto& [local_vars, sub_term]: ::incre::util::getSubTermWithLocalVars(term)) {
            if (local_vars.empty()) {
                _collectSoftConstraint(sub_term.get(), ctx, outer_cond, new_inner_cond, res);
            } else {
                _collectSoftConstraint(sub_term.get(), ctx, outer_cond | new_inner_cond, ctx->KFalse, res);
            }
        }
    }
}

z3::expr incre::autolabel::util::collectSoftConstraints(IncreProgramData *program, Z3Context *ctx) {
    std::vector<z3::expr> res;
    for (auto& command: program->commands) {
        if (command->getType() == CommandType::BIND_TERM) {
            auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
            _collectSoftConstraint(cb->term.get(), ctx, ctx->KFalse, ctx->KFalse, res);
        }
    }
    z3::expr obj = ctx->ctx.int_val(0);
    for (auto& expr: res) obj = obj + z3::ite(expr, ctx->ctx.int_val(1), ctx->ctx.int_val(0));
    return obj;
}