//
// Created by pro on 2023/1/22.
//

#include "istool/incre/autolabel/incre_autolabel.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::autolabel;

WrappedType::WrappedType(const z3::expr &_compress_label, const syntax::Ty &_content):
    TypeData(TypeType::COMPRESS), compress_label(_compress_label.simplify()), content(_content) {
}

std::string WrappedType::toString() const {
    return "[" + compress_label.to_string() + "](" + content->toString() + ")";
}

Z3LabeledVarInfo::Z3LabeledVarInfo(int _index, int _level, const z3::expr &_scalar_cond, const z3::expr &_base_cond):
    index(_index), level(_level), scalar_cond(_scalar_cond.simplify()), base_cond(_base_cond.simplify()) {
}

Z3TyVar::Z3TyVar(const FullZ3LabeledContent &_type): syntax::TypeData(TypeType::VAR), content(_type) {
}

std::string Z3TyVar::toString() const {
    if (isBounded()) return "bind(" + std::get<Ty>(content)->toString() + ")";
    auto info = std::get<Z3LabeledVarInfo>(content);
    return "?x" + std::to_string(info.index) + "[" + info.scalar_cond.to_string() + "," + info.base_cond.to_string() + "]";
}

Z3LabeledVarInfo &Z3TyVar::getVarInfo() {
    return std::get<Z3LabeledVarInfo>(content);
}

Ty &Z3TyVar::getBindType() {
    return std::get<Ty>(content);
}

bool Z3TyVar::isBounded() const {
    return content.index() == 0;
}

Z3Context::Z3Context(): cons_list(ctx), KTrue(ctx.bool_val(true)), KFalse(ctx.bool_val(false)) {}

void Z3Context::addCons(const z3::expr &expr) {
    // LOG(INFO) << "add cons " << expr.to_string();
    cons_list.push_back(expr.simplify());
}

z3::expr Z3Context::newVar() {
    auto var_name = "?b" + std::to_string(var_index++);
    return ctx.bool_const(var_name.c_str());
}

void SymbolicIncreTypeChecker::pushLevel() {
    ++_level;
}

void SymbolicIncreTypeChecker::popLevel() {
    --_level;
}

SymbolicIncreTypeChecker::SymbolicIncreTypeChecker(Z3Context *_z3_ctx): z3_ctx(_z3_ctx) {
}

std::shared_ptr<Z3TyVar> SymbolicIncreTypeChecker::getTmpVar(syntax::VarRange range) {
    auto info = Z3LabeledVarInfo(tmp_var_id++, _level, z3_ctx->KFalse, z3_ctx->KFalse);
    switch (range) {
        case ANY: break;
        case BASE: {
            info.base_cond = z3_ctx->KTrue; break;
        }
        case SCALAR: {
            info.scalar_cond = z3_ctx->KTrue; info.base_cond = z3_ctx->KTrue; break;
        }
    }
    return std::make_shared<Z3TyVar>(info);
}

WrappedTy SymbolicIncreTypeChecker::getTmpWrappedVar(syntax::VarRange range) {
    auto compress_var = z3_ctx->newVar();
    auto content = getTmpVar(range);
    auto& var_info = content->getVarInfo();
    var_info.base_cond = var_info.base_cond | compress_var;
    return std::make_shared<WrappedType>(compress_var, getTmpVar(range));
}

namespace {
    class _Z3LabeledTypeVarRewriter: public IncreTypeRewriter {
    public:
        std::unordered_map<int, WrappedTy> replace_map;
        Z3Context* ctx;
        _Z3LabeledTypeVarRewriter(const std::unordered_map<int, WrappedTy>& _replace_map, Z3Context* _ctx):
            replace_map(_replace_map), ctx(_ctx) {}
    protected:
        Ty _rewrite(TyVar* _, const Ty& _type) override {
            auto* type = dynamic_cast<Z3TyVar*>(_type.get()); assert(type);
            if (type->isBounded()) return rewrite(type->getBindType()); else return _type;
        }

        Ty _rewrite(TyCompress* _, const Ty& _type) override {
            auto* type = dynamic_cast<WrappedType*>(_type.get()); assert(type);
            if (type->content->getType() == TypeType::VAR) {
                auto* tv = dynamic_cast<Z3TyVar*>(type->content.get()); assert(tv);
                if (!tv->isBounded()) {
                    auto pre_info = tv->getVarInfo();
                    auto it = replace_map.find(pre_info.index);
                    if (it != replace_map.end()) {
                        auto replace_type = it->second;
                        ctx->addCons(!replace_type->compress_label | !type->compress_label);

                        auto* rv = dynamic_cast<Z3TyVar*>(replace_type->content.get());
                        assert(rv && !rv->isBounded());
                        auto& new_info = rv->getVarInfo();
                        new_info.scalar_cond = new_info.scalar_cond | pre_info.scalar_cond;
                        new_info.base_cond = new_info.base_cond | pre_info.base_cond;

                        return std::make_shared<WrappedType>(replace_type->compress_label | type->compress_label, replace_type->content);
                    }
                } else {
                    return rewrite(std::make_shared<WrappedType>(type->compress_label, tv->getBindType()));
                }
            }
            return std::make_shared<WrappedType>(type->compress_label, rewrite(type->content));
        }
    };
}

WrappedTy SymbolicIncreTypeChecker::instantiate(const WrappedTy &x) {
    if (x->content->getType() == TypeType::POLY) {
        auto* xp = dynamic_cast<TyPoly*>(x->content.get());
        std::unordered_map<int, WrappedTy> replace_map;
        for (auto index: xp->var_list) {
            replace_map[index] = getTmpWrappedVar(ANY);
        }
        auto* rewriter = new _Z3LabeledTypeVarRewriter(replace_map, z3_ctx);
        // LOG(INFO) << xp->body->toString();
        auto res = std::static_pointer_cast<WrappedType>(rewriter->rewrite(xp->body));
        delete rewriter; return res;
    } else return x;
}

namespace {
    TyList _getSubTypes(TypeData* x) {
        if (x->getType() == TypeType::VAR) {
            auto* var = dynamic_cast<Z3TyVar*>(x); assert(var);
            if (var->isBounded()) return {var->getBindType()}; else return {};
        }
        if (x->getType() == TypeType::COMPRESS) {
            auto* compress = dynamic_cast<WrappedType*>(x); assert(compress);
            return {compress->content};
        }
        return getSubTypes(x);
    }

    void _collectLocalVars(TypeData* x, std::unordered_set<int>& local_vars, int level) {
        if (x->getType() == TypeType::VAR) {
            auto* var = dynamic_cast<Z3TyVar*>(x); assert(var);
            if (!var->isBounded()) {
                auto info = var->getVarInfo();
                if (info.level > level) local_vars.insert(info.index);
                return;
            }
        }
        for (auto& sub_type: _getSubTypes(x)) {
            _collectLocalVars(sub_type.get(), local_vars, level);
        }
    }
}

WrappedTy SymbolicIncreTypeChecker::defaultWrap(const syntax::Ty &type) {
    auto* wt = dynamic_cast<WrappedType*>(type.get()); assert(!wt);
    return std::make_shared<WrappedType>(z3_ctx->KFalse, type);
}

WrappedTy SymbolicIncreTypeChecker::generalize(const WrappedTy &x) {
    std::unordered_set<int> local_vars;
    _collectLocalVars(x.get(), local_vars, _level);
    std::vector<int> vars(local_vars.begin(), local_vars.end());
    std::sort(vars.begin(), vars.end());
    if (vars.empty()) return x; else {
        auto content = std::make_shared<TyPoly>(vars, x);
        return defaultWrap(content);
    }
}

#define ToWrap(pointer) std::static_pointer_cast<WrappedType>(pointer)
#define MakeWrap(label, content) std::make_shared<WrappedType>(label, content)

void SymbolicIncreTypeChecker::updateTypeBeforeUnification(syntax::TypeData *x, const Z3LabeledVarInfo& info) {
    // LOG(INFO) << "update " << x->toString() << " " << info.scalar_cond;
    if (x->getType() == TypeType::VAR) {
        auto* var = dynamic_cast<Z3TyVar*>(x); assert(var);
        if (var->isBounded()) return updateTypeBeforeUnification(var->getBindType().get(), info);
        auto& var_info = var->getVarInfo();
        if (var_info.index == info.index) throw types::IncreTypingError("Infinite unification");
        var_info.level = std::min(var_info.level, info.level);
        var_info.scalar_cond = (var_info.scalar_cond | info.scalar_cond).simplify();
        var_info.base_cond = (var_info.base_cond | info.base_cond).simplify();
        return;
    }
    auto new_info = info;
    if (x->getType() == TypeType::COMPRESS) {
        auto* wt = dynamic_cast<WrappedType*>(x); assert(wt);
        new_info.scalar_cond = (new_info.scalar_cond & !wt->compress_label).simplify();
        new_info.base_cond = (new_info.base_cond & !wt->compress_label).simplify();
    } else if (x->getType() == TypeType::ARR) {
        auto* ta = dynamic_cast<TyArr*>(x); assert(ta);
        z3_ctx->addCons(!info.base_cond);
        new_info.scalar_cond = z3_ctx->KFalse;
        new_info.base_cond = z3_ctx->KFalse;
    } else if (x->getType() == TypeType::IND) {
        auto* ti = dynamic_cast<TyInd*>(x); assert(ti);
        z3_ctx->addCons(!info.scalar_cond);
        new_info.scalar_cond = z3_ctx->KFalse;
    }
    for (auto& sub_type: _getSubTypes(x)) {
        updateTypeBeforeUnification(sub_type.get(), new_info);
    }
}

#define UnifyCase(name) case TypeType::TYPE_TOKEN_ ## name: return _unify(dynamic_cast<Ty ## name*>(x->content.get()), dynamic_cast<Ty ## name*>(y->content.get()), x, y);

void SymbolicIncreTypeChecker::_unify(syntax::TyArr *x, syntax::TyArr *y, const WrappedTy &_x, const WrappedTy &_y) {
    unify(ToWrap(x->inp), ToWrap(y->inp));
    unify(ToWrap(x->oup), ToWrap(y->oup));
}

void SymbolicIncreTypeChecker::_unify(syntax::TyInd *x, syntax::TyInd *y, const WrappedTy &_x, const WrappedTy &_y) {
    z3_ctx->addCons(_x->compress_label == _y->compress_label);
    if (x->name != y->name || x->param_list.size() != y->param_list.size()) {
        throw types::IncreTypingError(x->toString() + " cannot unify with " + y->toString());
    }
    for (int i = 0; i < x->param_list.size(); ++i) {
        unify(ToWrap(x->param_list[i]), ToWrap(y->param_list[i]));
    }
}

void
SymbolicIncreTypeChecker::_unify(syntax::TyTuple *x, syntax::TyTuple *y, const WrappedTy &_x, const WrappedTy &_y) {
    z3_ctx->addCons(_x->compress_label == _y->compress_label);
    if (x->fields.size() != y->fields.size()) {
        throw types::IncreTypingError(x->toString() + " cannot unify with " + y->toString());
    }
    for (int i = 0; i < x->fields.size(); ++i) {
        unify(ToWrap(x->fields[i]), ToWrap(y->fields[i]));
    }
}

void SymbolicIncreTypeChecker::_unify(syntax::TyInt *x, syntax::TyInt *y, const WrappedTy &_x, const WrappedTy &_y) {}
void SymbolicIncreTypeChecker::_unify(syntax::TyBool *x, syntax::TyBool *y, const WrappedTy &_x, const WrappedTy &_y) {}
void SymbolicIncreTypeChecker::_unify(syntax::TyUnit *x, syntax::TyUnit *y, const WrappedTy &_x, const WrappedTy &_y) {}
void SymbolicIncreTypeChecker::_unify(syntax::TyPoly *x, syntax::TyPoly *y, const WrappedTy &_x, const WrappedTy &_y) {
    throw types::IncreTypingError("Unexpected unification for Poly type " + x->toString());
}
void SymbolicIncreTypeChecker::_unify(syntax::TyCompress *x, syntax::TyCompress *y, const WrappedTy &_x,
                                      const WrappedTy &_y) {
    throw types::IncreTypingError("Unexpected unification for Compress type " + _x->toString());
}

void SymbolicIncreTypeChecker::_unify(syntax::TyVar *__x, syntax::TyVar *__y, const WrappedTy &_x, const WrappedTy &_y) {
    auto* x = dynamic_cast<Z3TyVar*>(_x->content.get()), *y = dynamic_cast<Z3TyVar*>(_y->content.get());
    assert(x);
    if (x->isBounded()) return unify(std::make_shared<WrappedType>(_x->compress_label, x->getBindType()), _y);
    if (y && y->isBounded()) return unify(_x, std::make_shared<WrappedType>(_y->compress_label, y->getBindType()));
    z3_ctx->addCons(_x->compress_label == _y->compress_label);

    if (y && !y->isBounded() && y->getVarInfo().index == x->getVarInfo().index) return;
    updateTypeBeforeUnification(_y->content.get(), x->getVarInfo());
    x->content = _y->content;
}

void SymbolicIncreTypeChecker::unify(const WrappedTy &x, const WrappedTy &y) {
    auto x_type = x->content->getType(), y_type = y->content->getType();
    // LOG(INFO) << "unify " << x->toString() << " " << y->toString() << " " << (x_type == TypeType::VAR) << " " << (y_type == TypeType::VAR);
    if (x_type != TypeType::VAR && y_type == TypeType::VAR) return unify(y, x);
    if (x_type != TypeType::VAR && x_type != y_type) {
        throw types::IncreTypingError(x->toString() + " cannot unify with " + y->toString());
    }
    switch (x_type) {
        TYPE_CASE_ANALYSIS(UnifyCase);
    }
}

#define GetType(name, ctx) typing(term->name.get(), ctx)
#define GetTypeAssign(name, ctx) auto name = typing(term->name.get(), ctx)

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmIf *term, const IncreContext &ctx) {
    GetTypeAssign(c, ctx); unify(c, defaultWrap(std::make_shared<TyBool>()));
    GetTypeAssign(t, ctx); GetTypeAssign(f, ctx);
    unify(t, f); return t;
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmApp *term, const IncreContext &ctx) {
    GetTypeAssign(func, ctx); GetTypeAssign(param, ctx);
    auto res = getTmpWrappedVar(ANY);
    unify(func, defaultWrap(std::make_shared<TyArr>(param, res)));
    return res;
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmLet *term, const IncreContext &ctx) {
    if (term->is_rec) {
        pushLevel();
        auto def_var = getTmpWrappedVar(ANY);
        auto new_ctx = ctx.insert(term->name, Binding(def_var));
        unify(def_var, GetType(def, new_ctx));
        popLevel();
        new_ctx = ctx.insert(term->name, Binding(generalize(def_var)));
        return GetType(body, new_ctx);
    } else {
       pushLevel();
       GetTypeAssign(def, ctx);
       popLevel();
       def = generalize(def);
       auto new_ctx = ctx.insert(term->name, Binding(def));
       return GetType(body, new_ctx);
    }
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmVar *term, const IncreContext &ctx) {
    return instantiate(ToWrap(ctx.getRawType(term->name)));
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmCons *term, const IncreContext &ctx) {
    auto cons_type = instantiate(ToWrap(ctx.getRawType(term->cons_name)));
    GetTypeAssign(body, ctx); auto res = getTmpWrappedVar(ANY);
    // LOG(INFO) << "try unify " << cons_type->toString();
    // LOG(INFO) << body->toString() << "  ;;;;  " << res->toString();
    unify(cons_type, defaultWrap(std::make_shared<TyArr>(body, res)));
    return res;
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmFunc *term, const IncreContext &ctx) {
    auto inp_type = getTmpWrappedVar(ANY);
    auto new_ctx = ctx.insert(term->name, Binding(inp_type));
    return defaultWrap(std::make_shared<TyArr>(inp_type, GetType(body, new_ctx)));
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmProj *term, const IncreContext &ctx) {
    GetTypeAssign(body, ctx); TyList fields(term->size);
    for (int i = 0; i < term->size; ++i) fields[i] = getTmpVar(ANY);
    unify(body, defaultWrap(std::make_shared<TyTuple>(fields)));
    return ToWrap(fields[term->id - 1]);
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmLabel *term, const IncreContext &ctx) {
    LOG(FATAL) << "Unexpected TmLabel in the input";
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmTuple *term, const IncreContext &ctx) {
    TyList fields;
    for (auto& field: term->fields) fields.push_back(typing(field.get(), ctx));
    return defaultWrap(std::make_shared<TyTuple>(fields));
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmUnlabel *term, const IncreContext &ctx) {
    LOG(FATAL) << "Unexpected TmUnLabel in the input";
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmRewrite *term, const IncreContext &ctx) {
    LOG(FATAL) << "Unexpected TmRewrite in the input";
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmValue *term, const IncreContext &ctx) {
    return util::liftNormalType(types::getSyntaxValueType(term->v), z3_ctx);
}

std::pair<WrappedTy, IncreContext>
SymbolicIncreTypeChecker::processPattern(syntax::PatternData *pattern, const IncreContext &ctx) {
    switch (pattern->getType()) {
        case PatternType::UNDERSCORE: return {getTmpWrappedVar(ANY), ctx};
        case PatternType::VAR: {
            auto* pv = dynamic_cast<PtVar*>(pattern);
            if (pv->body) {
                auto [sub_ty, new_ctx] = processPattern(pv->body.get(), ctx);
                new_ctx = new_ctx.insert(pv->name, Binding(sub_ty));
                return {sub_ty, new_ctx};
            } else {
                auto new_var = getTmpWrappedVar(ANY);
                auto new_ctx = ctx.insert(pv->name, Binding(new_var));
                return {new_var, new_ctx};
            }
        }
        case PatternType::TUPLE: {
            auto* pt = dynamic_cast<PtTuple*>(pattern);
            TyList fields; auto current_ctx = ctx;
            for (auto& sub_pt: pt->fields) {
                auto [sub_type, new_ctx] = processPattern(sub_pt.get(), current_ctx);
                current_ctx = new_ctx; fields.push_back(sub_type);
            }
            return {defaultWrap(std::make_shared<TyTuple>(fields)), current_ctx};
        }
        case PatternType::CONS: {
            auto* pc = dynamic_cast<PtCons*>(pattern);
            auto cons_type = instantiate(ToWrap(ctx.getRawType(pc->name)));
            auto [body_type, new_ctx] = processPattern(pc->body.get(), ctx);
            auto res_type = getTmpWrappedVar(ANY);
            unify(defaultWrap(std::make_shared<TyArr>(body_type, res_type)), cons_type);
            return {res_type, new_ctx};
        }
    }
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmMatch *term, const IncreContext &ctx) {
    GetTypeAssign(def, ctx); auto res_type = getTmpWrappedVar(ANY);
    for (auto& [pt, case_body]: term->cases) {
        auto [inp_type, new_ctx] = processPattern(pt.get(), ctx);
        auto body_type = typing(case_body.get(), new_ctx);
        unify(def, inp_type); unify(res_type, body_type);
    }
    return res_type;
}

WrappedTy SymbolicIncreTypeChecker::_typing(syntax::TmPrimary *term, const IncreContext &ctx) {
    auto op_type = util::liftNormalType(types::getPrimaryType(term->op_name), z3_ctx);
    op_type = instantiate(op_type);
    auto oup_ty = getTmpWrappedVar(ANY); auto full_ty = oup_ty;
    for (int i = int(term->params.size()) - 1; i >= 0; --i) {
        full_ty = defaultWrap(std::make_shared<TyArr>(typing(term->params[i].get(), ctx), full_ty));
    }
    unify(op_type, full_ty); return oup_ty;
}

namespace {
    bool _isPossibleBase(TypeData* type) {
        if (type->getType() == TypeType::ARR) return false;
        for (auto& sub_type: _getSubTypes(type)) {
            if (!_isPossibleBase(sub_type.get())) return false;
        }
        return true;
    }
    bool _isIncludeInd(TypeData* type) {
        if (type->getType() == TypeType::VAR) {
            auto* tx = dynamic_cast<Z3TyVar*>(type); assert(tx);
            if (tx->isBounded()) return _isIncludeInd(tx->getBindType().get()); else return true;
        }
        if (type->getType() == TypeType::IND) return true;
        for (auto& sub_type: _getSubTypes(type)) {
            if (_isIncludeInd(sub_type.get())) return true;
        }
        return false;
    }
}

#define TypingCase(name) case TermType::TERM_TOKEN_ ## name: {res = _typing(dynamic_cast<Tm ## name*>(term), ctx); break;}
WrappedTy SymbolicIncreTypeChecker::typing(syntax::TermData *term, const IncreContext &ctx) {
    WrappedTy res;
    // LOG(INFO) << "typing " << term->toString();
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(TypingCase)
    }
    // LOG(INFO) << "raw type " << term->toString() << " " << res->toString();
    // LOG(INFO) << "normalized " << res->toString();
    if (_isPossibleBase(res.get())) {
        if (_isIncludeInd(res.get())) {
            auto flip_var = z3_ctx->newVar();
            z3_ctx->flip_map.insert({term, flip_var});
            Z3LabeledVarInfo base_info(-1, 1e9, z3_ctx->KFalse, flip_var);
            updateTypeBeforeUnification(res->content.get(), base_info);
            res = std::make_shared<WrappedType>(res->compress_label ^ flip_var, res->content);
        }
        auto rewrite_var = z3_ctx->newVar();
        z3_ctx->rewrite_map.insert({term, rewrite_var});
        Z3LabeledVarInfo rewrite_info(-1, 1e9, rewrite_var, z3_ctx->KFalse);
        updateTypeBeforeUnification(res.get(), rewrite_info);
    }
    // LOG(INFO) << term->toString() << " " << res->toString();
    return z3_ctx->type_map[term] = res;
}



