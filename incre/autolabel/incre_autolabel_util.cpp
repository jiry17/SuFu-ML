//
// Created by pro on 2024/3/29.
//

#include "istool/incre/autolabel/incre_autolabel.h"
#include "glog/logging.h"

using namespace incre::syntax;
using namespace incre::autolabel;
using namespace incre;

#define LIFT_CASE(name) case TypeType::TYPE_TOKEN_ ## name: return _lift(dynamic_cast<Ty ## name*>(raw_type.get()), raw_type, ctx)
#define LIFT_HEAD(name) WrappedTy _lift(Ty ## name* type, const Ty& _type, Z3Context* ctx)
#define Wrap(ty) std::make_shared<WrappedType>(ctx->KFalse, ty)
#define DEFAULT_LIFT(name) LIFT_HEAD(name) {return Wrap(_type);}

LIFT_HEAD(Var) {
    if (type->is_bounded()) LOG(FATAL) << "Type not normalized: " << _type->toString();
    auto [index, level, range] = type->get_var_info();
    Z3LabeledVarInfo info(index, level, ctx->KFalse, ctx->KFalse);
    if (range <= incre::syntax::BASE) info.base_cond = ctx->KTrue;
    if (range <= incre::syntax::SCALAR) info.base_cond = ctx->KTrue;
    return Wrap(std::make_shared<Z3TyVar>(info));
}

DEFAULT_LIFT(Int)
DEFAULT_LIFT(Bool)
DEFAULT_LIFT(Unit)

LIFT_HEAD(Arr) {
    auto inp = util::liftNormalType(type->inp, ctx);
    auto oup = util::liftNormalType(type->oup, ctx);
    return Wrap(std::make_shared<TyArr>(inp, oup));
}

LIFT_HEAD(Compress) {
    auto content = util::liftNormalType(type->body, ctx);
    content->compress_label = ctx->KTrue;
    return content;
}

LIFT_HEAD(Tuple) {
    TyList fields;
    for (auto& field: type->fields) fields.push_back(util::liftNormalType(field, ctx));
    return Wrap(std::make_shared<TyTuple>(fields));
}

LIFT_HEAD(Poly) {
    auto content = util::liftNormalType(type->body, ctx);
    return Wrap(std::make_shared<TyPoly>(type->var_list, content));
}

LIFT_HEAD(Ind) {
    TyList param_list;
    for (auto& param: type->param_list) param_list.push_back(util::liftNormalType(param, ctx));
    return Wrap(std::make_shared<TyInd>(type->name, param_list));
}

WrappedTy util::liftNormalType(const syntax::Ty &raw_type, Z3Context *ctx) {
    switch (raw_type->getType()) {
        TYPE_CASE_ANALYSIS(LIFT_CASE);
    }
}

namespace {
    class _LabelClearer: public IncreTypeRewriter {
    public:
        Z3Context* ctx;
        _LabelClearer(Z3Context* _ctx): ctx(_ctx) {}
        Ty _rewrite(TyVar* _, const Ty& _type) override {
            auto* type = dynamic_cast<Z3TyVar*>(_type.get()); assert(type);
            if (type->isBounded()) return rewrite(type->getBindType()); else return _type;
        }
        Ty _rewrite(TyCompress* _, const Ty& _type) override {
            auto* type = dynamic_cast<WrappedType*>(_type.get()); assert(type);
            ctx->addCons(!type->compress_label);
            return std::make_shared<WrappedType>(ctx->KFalse, rewrite(type->content));
        }
    };

    Ty clearLabel(const Ty& type, Z3Context* ctx) {
        auto* rewriter = new _LabelClearer(ctx);
        auto res = rewriter->rewrite(type);
        delete rewriter; return res;
    }

#define ToWrap(pointer) (std::static_pointer_cast<WrappedType>(pointer))

    void _collectCoverCond(TermData* term, const z3::expr& raw_rewrite_cond, Z3Context* ctx) {
        auto rewrite_cond(raw_rewrite_cond);
        auto rewrite_it = ctx->rewrite_map.find(term);
        if (rewrite_it != ctx->rewrite_map.end()) {
            rewrite_cond = rewrite_cond | rewrite_it->second;
        }
        auto flip_it = ctx->flip_map.find(term);
        if (flip_it != ctx->flip_map.end()) {
            ctx->addCons(z3::implies(flip_it->second, rewrite_cond));
        }
        for (auto& sub_term: syntax::getSubTerms(term)) {
            _collectCoverCond(sub_term.get(), rewrite_cond, ctx);
        }
    }

    void _collectFromCommand(CommandBindTerm* command, IncreContext& ctx, SymbolicIncreTypeChecker* checker) {
        if (ctx.isContain(command->name)) {
            auto* address = ctx.getAddress(command->name);
            if (!address->bind.data.isNull()) LOG(FATAL) << "Duplicated declaration on name " << command->name;
            assert(address->bind.type);
            checker->unify(ToWrap(address->bind.type), checker->typing(command->term.get(), ctx));
            address->bind.type = ToWrap(address->bind.type);
        } else if (command->is_rec) {
            ctx = ctx.insert(command->name, Binding(false, {}, {}));
            auto* address = ctx.getAddress(command->name);
            checker->pushLevel(); auto current_type = checker->getTmpWrappedVar(ANY);
            address->bind.type = current_type;
            checker->unify(current_type, checker->typing(command->term.get(), ctx));
            checker->popLevel();
            auto final_type = checker->generalize(current_type);
            address->bind.type = clearLabel(final_type, checker->z3_ctx);
        } else {
            checker->pushLevel(); auto type = checker->typing(command->term.get(), ctx);
            checker->popLevel(); type = checker->generalize(type);
            ctx = ctx.insert(command->name, Binding(clearLabel(type, checker->z3_ctx)));
        }
        //LOG(INFO) << "collect cover cond";
        _collectCoverCond(command->term.get(), checker->z3_ctx->KFalse, checker->z3_ctx);
    }

    void _collectFromCommand(CommandEval* command, IncreContext& ctx, SymbolicIncreTypeChecker* checker) {
        checker->pushLevel();
        checker->typing(command->term.get(), ctx);
        checker->popLevel();
    }

    void _collectFromCommand(CommandDef* command, IncreContext& ctx, SymbolicIncreTypeChecker* checker) {
        for (auto& [cons_name, cons_type]: command->cons_list) {
            ctx = ctx.insert(cons_name, Binding(true, util::liftNormalType(cons_type, checker->z3_ctx), {}));
        }
    }

    void _collectFromCommand(CommandDeclare* command, IncreContext& ctx, SymbolicIncreTypeChecker* checker) {
        if (ctx.isContain(command->name)) LOG(FATAL) << "Duplicated name " << command->name;
        ctx = ctx.insert(command->name, Binding(util::liftNormalType(command->type, checker->z3_ctx)));
    }
}

void util::collectHardConstraints(IncreProgramData *program, SymbolicIncreTypeChecker *checker) {
    IncreContext ctx(nullptr);

    for (auto& command: program->commands) {
        switch (command.get()->getType()) {
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                _collectFromCommand(cb, ctx, checker); break;
            }
            case CommandType::DEF_IND: {
                auto* cd = dynamic_cast<CommandDef*>(command.get());
                _collectFromCommand(cd, ctx, checker); break;
            }
            case CommandType::DECLARE: {
                auto* cd = dynamic_cast<CommandDeclare*>(command.get());
                _collectFromCommand(cd, ctx, checker); break;
            }
            case CommandType::EVAL: {
                auto* cd = dynamic_cast<CommandEval*>(command.get());
                _collectFromCommand(cd, ctx, checker); break;
            }
        }
    }
}

namespace {
    bool _extractBoolValue(const z3::expr& expr, const z3::model& model) {
        auto res = model.eval(expr);
        if (res.bool_value() == Z3_L_TRUE) return true;
        return false;
    }

    class _LabelConstructor: public IncreTermRewriter {
    protected:
        virtual Term postProcess(const Term &original_term, const Term &raw_res) override {
            auto flip_it = ctx->flip_map.find(original_term.get());
            auto type = ctx->type_map[original_term.get()]; assert(type);
            Term res = raw_res;
            if (flip_it != ctx->flip_map.end()) {
                auto is_flip = _extractBoolValue(flip_it->second, model);
                auto is_label = _extractBoolValue(type->compress_label, model);
                if (is_flip) {
                    if (is_label) res = std::make_shared<TmLabel>(res); else res = std::make_shared<TmUnlabel>(res);
                }
            }
            auto rewrite_it = ctx->rewrite_map.find(original_term.get());
            if (rewrite_it != ctx->rewrite_map.end()) {
                if (_extractBoolValue(rewrite_it->second, model)) {
                    res = std::make_shared<TmRewrite>(res);
                }
            }
            return res;
        }
    public:
        z3::model model;
        Z3Context* ctx;
        _LabelConstructor(const z3::model& _model, Z3Context* _ctx): model(_model), ctx(_ctx) {}
    };
}

IncreProgram util::rewriteUseResult(IncreProgramData *program, Z3Context *ctx, const z3::model &model) {
    CommandList commands;
    auto* rewriter = new _LabelConstructor(model, ctx);
    for (auto& command: program->commands) {
        switch (command->getType()) {
            case CommandType::DECLARE:
            case CommandType::DEF_IND: {
                commands.push_back(command);
                break;
            }
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                auto new_term = rewriter->rewrite(cb->term);
                auto new_command = std::make_shared<CommandBindTerm>(cb->name, cb->is_rec, new_term, cb->decos, cb->source);
                commands.push_back(new_command);
                break;
            }
            case CommandType::EVAL: {
                auto* ce = dynamic_cast<CommandEval*>(command.get());
                auto new_term = rewriter->rewrite(ce->term);
                auto new_command = std::make_shared<CommandEval>(ce->name, new_term, ce->decos, ce->source);
                commands.push_back(new_command);
                break;
            }
        }
    }
    return std::make_shared<IncreProgramData>(commands, program->config_map);
}

