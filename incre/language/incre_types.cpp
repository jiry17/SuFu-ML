//
// Created by pro on 2023/12/7.
//

#include "istool/incre/language/incre_types.h"
#include "istool/incre/language/incre_rewriter.h"
#include "istool/incre/language/incre_semantics.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::types;

IncreTypingError::IncreTypingError(const std::string _message): message(_message) {
}

#define TINT std::make_shared<TyInt>()
#define TBOOL std::make_shared<TyBool>()
#define TARR(inp, oup) std::make_shared<TyArr>(inp, oup)

syntax::Ty types::getSyntaxValueType(const Data &data) {
    if (dynamic_cast<semantics::VInt*>(data.get())) return TINT;
    if (dynamic_cast<semantics::VBool*>(data.get())) return TBOOL;
    if (dynamic_cast<semantics::VUnit*>(data.get())) return std::make_shared<TyUnit>();
    throw IncreTypingError("value " + data.toString() + "should not occur in the input program");
}

namespace {
    const std::string KIntBinaryOp = "+-*/";
    const std::string KIntCompareOp = ">=<=";

    std::string _getRangeName(syntax::VarRange x) {
        switch (x) {
            case ANY: return "Any";
            case SCALAR: return "Scalar";
            case BASE: return "Base";
        }
    }

    void rangeMismatchError(syntax::VarRange x, syntax::TypeData* y) {
        throw IncreTypingError("Type " + y->toString() + " does not belong to range " + _getRangeName(x));
    }
}

void types::checkAndUpdate(syntax::VarRange x, syntax::TypeData* type) {
    if (type->getType() == TypeType::COMPRESS) return;
    if (type->getType() == TypeType::ARR && x != ANY) {
        rangeMismatchError(x, type);
    }
    if (type->getType() == TypeType::IND && x == SCALAR) {
        rangeMismatchError(x, type);
    }
    if (type->getType() == TypeType::VAR) {
        auto* tv = dynamic_cast<TyVar*>(type);
        if (!tv->is_bounded()) tv->intersectWith(x);
    }
    for (auto& sub_type: syntax::getSubTypes(type)) {
        checkAndUpdate(x, sub_type.get());
    }
}


syntax::Ty types::getPrimaryType(const std::string &name) {
    if (KIntBinaryOp.find(name) != std::string::npos) return TARR(TINT, TARR(TINT, TINT));
    if (name == "==") {
        std::tuple<int, int, VarRange> var_info(0, 0, BASE);
        auto var = std::make_shared<TyVar>(var_info);
        auto type = TARR(var, TARR(var, TBOOL));
        return std::make_shared<TyPoly>((std::vector<int>){0}, type);
    }
    if (KIntCompareOp.find(name) != std::string::npos) return TARR(TINT, TARR(TINT, TBOOL));
    if (name == "and" || name == "or") return TARR(TBOOL, TARR(TBOOL, TBOOL));
    if (name == "neg") return TARR(TINT, TINT);
    if (name == "not") return TARR(TBOOL, TBOOL);
    throw IncreTypingError("Unknown primary operator " + name);
}

const char *IncreTypingError::what() const noexcept {
    return message.c_str();
}

void types::IncreTypeChecker::pushLevel() {
    ++_level;
}

void types::IncreTypeChecker::popLevel() {
    --_level;
}

syntax::Ty types::IncreTypeChecker::getTmpVar(VarRange range) {
    TypeVarInfo info(std::make_tuple(tmp_var_id++, _level, range));
    return std::make_shared<TyVar>(info);
}

namespace {
    class _TypeVarRewriter: public IncreTypeRewriter {
    public:
        std::unordered_map<int, Ty> replace_map;
        _TypeVarRewriter(const std::unordered_map<int, Ty> _replace_map): replace_map(_replace_map) {}
    protected:
        Ty _rewrite(TyVar* type, const Ty& _type) override {
            if (type->is_bounded()) {
                return rewrite(type->get_bound_type());
            }
            auto [index, _level, _info] = type->get_var_info();
            auto it = replace_map.find(index);
            if (it == replace_map.end()) return _type;
            return it->second;
        }
    };

    void _collectRangeMap(TypeData* type, std::unordered_map<int, VarRange>& range_map) {
        if (type->getType() == TypeType::VAR) {
            auto* tv = dynamic_cast<TyVar*>(type);
            if (!tv->is_bounded()) {
                auto [index, _, info] = tv->get_var_info();
                range_map[index] = info;
            }
        }
        for (auto& sub_type: syntax::getSubTypes(type)) {
            _collectRangeMap(sub_type.get(), range_map);
        }
    }
}

syntax::Ty DefaultIncreTypeChecker::instantiate(const syntax::Ty &x) {
    if (x->getType() == TypeType::POLY) {
        auto* xp = dynamic_cast<TyPoly*>(x.get());
        std::unordered_map<int, Ty> replace_map;
        std::unordered_map<int, VarRange> range_map;
        _collectRangeMap(xp->body.get(), range_map);
        for (auto index: xp->var_list) {
            auto it = range_map.find(index); assert(it != range_map.end());
            replace_map[index] = getTmpVar(it->second);
        }
        auto* rewriter = new _TypeVarRewriter(replace_map);
        auto res = rewriter->rewrite(xp->body);
        delete rewriter; return res;
    } else return x;
}

namespace {
    void _collectLocalVars(TypeData* x, std::unordered_set<int>& local_vars, int level) {
        if (x->getType() == TypeType::VAR) {
            auto* tv = dynamic_cast<TyVar*>(x);
            if (!tv->is_bounded()) {
                auto [var_index, var_level, info] = tv->get_var_info();
                if (var_level > level) local_vars.insert(var_index);
                return;
            }
        }
        for (auto& subtype: getSubTypes(x)) {
            _collectLocalVars(subtype.get(), local_vars, level);
        }
    }

    bool _isInvolveRewrite(TermData* term) {
        if (term->getType() == TermType::REWRITE) return true;
        for (auto& sub: syntax::getSubTerms(term)) {
            if (_isInvolveRewrite(sub.get())) return true;
        }
        return false;
    }
}

syntax::Ty DefaultIncreTypeChecker::generalize(const syntax::Ty &x, TermData* term) {
    if (_isInvolveRewrite(term)) return x;
    std::unordered_set<int> local_vars;
    _collectLocalVars(x.get(), local_vars, _level);
    std::vector<int> vars;
    for (auto index: local_vars) vars.push_back(index);
    std::sort(vars.begin(), vars.end());
    if (vars.empty()) return x; else return std::make_shared<TyPoly>(vars, x);
}

void DefaultIncreTypeChecker::updateLevelBeforeUnification(syntax::TypeData *x, int index, int level) {
    if (x->getType() == TypeType::VAR) {
        auto* tv = dynamic_cast<TyVar*>(x);
        if (!tv->is_bounded()) {
            auto [var_index, var_level, info] = tv->get_var_info();
            if (var_index == index) throw IncreTypingError("infinite unification");
            tv->info = std::make_tuple(var_index, std::min(var_level, level), info);
            return;
        }
    }
    for (auto& subtype: getSubTypes(x)) {
        updateLevelBeforeUnification(subtype.get(), index, level);
    }
}

#define UnifyCase(name) case TypeType::TYPE_TOKEN_ ## name: return _unify(dynamic_cast<Ty ## name*>(x.get()), dynamic_cast<Ty ## name*>(y.get()), x, y);
void IncreTypeChecker::unify(const syntax::Ty& x, const syntax::Ty& y) {
    // LOG(INFO) << "unify " << x->toString() << " | " << y->toString();
    if (x->getType() != TypeType::VAR && y->getType() == TypeType::VAR) return unify(y, x);
    switch (x->getType()) {
        TYPE_CASE_ANALYSIS(UnifyCase);
    }
}

#define TypeCheckCase(name) case TermType::TERM_TOKEN_## name: {res = _typing(dynamic_cast<Tm ## name*>(term), ctx); break;}

syntax::Ty IncreTypeChecker::typing(syntax::TermData *term, const IncreContext &ctx) {
    preProcess(term, ctx); Ty res;
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(TypeCheckCase);
    }
    return postProcess(term, ctx, normalize(res));
}

#define TrivialUnificationCase(name) void DefaultIncreTypeChecker::_unify(syntax::Ty ##name *x, syntax::Ty## name *y, const syntax::Ty &_x, const syntax::Ty &_y) { \
    if (!y) throw IncreTypingError("unification failed: typetype dismatch.");                                                                                         \
    auto x_sub = getSubTypes(x), y_sub = getSubTypes(y);                                                                                                            \
    if (x_sub.size() != y_sub.size()) throw IncreTypingError("unification failed: different number of parameters.");                                                  \
    for (int i = 0; i < x_sub.size(); ++i) unify(x_sub[i], y_sub[i]);                                                                                   \
}

TrivialUnificationCase(Int);
TrivialUnificationCase(Bool);
TrivialUnificationCase(Unit);
TrivialUnificationCase(Arr);
TrivialUnificationCase(Tuple);
TrivialUnificationCase(Compress);

void DefaultIncreTypeChecker::_unify(syntax::TyPoly *x, syntax::TyPoly *y, const syntax::Ty &_x, const syntax::Ty &_y) {
    throw IncreTypingError("PolyType should not occur when unification");
}

void DefaultIncreTypeChecker::_unify(syntax::TyInd *x, syntax::TyInd *y, const syntax::Ty &_x, const syntax::Ty &_y) {
    if (!y) throw IncreTypingError("unification failed: typetype dismatch");
    if (x->name != y->name || x->param_list.size() != y->param_list.size()) {
        throw IncreTypingError("unification failed: inductive constructor dismatch");
    }
    for (int i = 0; i < x->param_list.size(); ++i) {
        unify(x->param_list[i], y->param_list[i]);
    }
}

void DefaultIncreTypeChecker::_unify(syntax::TyVar *x, syntax::TyVar *y, const syntax::Ty &_x, const syntax::Ty &_y) {
    if (x->is_bounded()) return unify(x->get_bound_type(), _y);
    if (y && y->is_bounded()) return unify(_x, y->get_bound_type());
    auto [x_index, x_level, x_range] = x->get_var_info();
    if (y && std::get<0>(y->get_var_info()) == x_index) return;
    updateLevelBeforeUnification(_y.get(), x_index, x_level);
    checkAndUpdate(x_range, _y.get());
    x->info = _y;
}

void DefaultIncreTypeChecker::preProcess(syntax::TermData *term, const IncreContext &ctx) {}
Ty DefaultIncreTypeChecker::postProcess(syntax::TermData *term, const IncreContext &ctx, const syntax::Ty &res) {return res;}

#define GetType(name, ctx) typing(term->name.get(), ctx)
#define GetTypeAssign(name, ctx) auto name = typing(term->name.get(), ctx)
syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmIf *term, const IncreContext &ctx) {
    GetTypeAssign(c, ctx); unify(c, std::make_shared<TyBool>());
    GetTypeAssign(t, ctx); GetTypeAssign(f, ctx);
    unify(t, f); return t;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmApp *term, const IncreContext &ctx) {
    GetTypeAssign(func, ctx); GetTypeAssign(param, ctx);
    auto res = getTmpVar(ANY);
    unify(func, std::make_shared<TyArr>(param, res));
    return res;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmLet *term, const IncreContext &ctx) {
    if (term->is_rec) {
        pushLevel();
        auto def_var = getTmpVar(ANY);
        auto new_ctx = ctx.insert(term->name, def_var);
        unify(def_var, GetType(def, new_ctx));
        popLevel();
        new_ctx = ctx.insert(term->name, generalize(def_var, term->def.get()));
        return GetType(body, new_ctx);
    } else {
        pushLevel();
        GetTypeAssign(def, ctx);
        popLevel();
        def = generalize(def, term->def.get());
        auto new_ctx = ctx.insert(term->name, def);
        return GetType(body, new_ctx);
    }
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmVar *term, const IncreContext &ctx) {
    auto var_type = ctx.getRawType(term->name);
    return instantiate(var_type);
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmCons *term, const IncreContext &ctx) {
    auto cons_type = instantiate(ctx.getRawType(term->cons_name));
    GetTypeAssign(body, ctx); auto res = getTmpVar(ANY);
    unify(cons_type, std::make_shared<TyArr>(body, res));
    return res;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmFunc *term, const IncreContext &ctx) {
    auto inp_type = getTmpVar(ANY);
    auto new_ctx = ctx.insert(term->name, inp_type);
    return std::make_shared<TyArr>(inp_type, GetType(body, new_ctx));
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmProj *term, const IncreContext &ctx) {
    GetTypeAssign(body, ctx);
    TyList fields(term->size);
    for (int i = 0; i < term->size; ++i) fields[i] = getTmpVar(ANY);
    unify(body, std::make_shared<TyTuple>(fields));
    return fields[term->id - 1];
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmLabel *term, const IncreContext &ctx) {
    GetTypeAssign(body, ctx);
    checkAndUpdate(BASE, body.get());
    return std::make_shared<TyCompress>(GetType(body, ctx));
}

namespace {
    bool _isDuplicatedCompress(TypeData* type) {
        if (type->getType() == TypeType::COMPRESS) {
            auto* tc = dynamic_cast<TyCompress*>(type);
            if (tc->body->getType() == TypeType::COMPRESS) return true;
        }
        for (auto& sub_type: getSubTypes(type)) {
            if (_isDuplicatedCompress(sub_type.get())) return true;
        }
        return false;
    }
}

syntax::Ty DefaultIncreTypeChecker::normalize(const syntax::Ty &type) {
    if (_isDuplicatedCompress(type.get())) {
        throw IncreTypingError("Duplicated type annotation found on " + type->toString());
    }
    return type;
}

std::pair<syntax::Ty, IncreContext>
DefaultIncreTypeChecker::processPattern(syntax::PatternData *pattern, const IncreContext& ctx) {
    switch (pattern->getType()) {
        case PatternType::UNDERSCORE: return {getTmpVar(ANY), ctx};
        case PatternType::VAR: {
            auto* pv = dynamic_cast<PtVar*>(pattern);
            if (pv->body) {
                auto [sub_ty, new_ctx] = processPattern(pv->body.get(), ctx);
                new_ctx = new_ctx.insert(pv->name, sub_ty);
                return {sub_ty, new_ctx};
            } else {
                auto new_var = getTmpVar(ANY);
                auto new_ctx = ctx.insert(pv->name, new_var);
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
            return {std::make_shared<TyTuple>(fields), current_ctx};
        }
        case PatternType::CONS: {
            auto* pc = dynamic_cast<PtCons*>(pattern);
            auto cons_type = instantiate(ctx.getRawType(pc->name));
            auto [body_type, new_ctx] = processPattern(pc->body.get(), ctx);
            auto res_type = getTmpVar(ANY);
            unify(std::make_shared<TyArr>(body_type, res_type), cons_type);
            return {res_type, new_ctx};
        }
    }
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmMatch *term, const IncreContext &ctx) {
    GetTypeAssign(def, ctx); auto res_type = getTmpVar(ANY);
    for (auto& [pt, case_body]: term->cases) {
        auto [inp_type, new_ctx] = processPattern(pt.get(), ctx);
        auto body_type = typing(case_body.get(), new_ctx);
        unify(def, inp_type); unify(res_type, body_type);
    }
    return res_type;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmTuple *term, const IncreContext &ctx) {
    TyList fields;
    for (auto& field: term->fields) fields.push_back(typing(field.get(), ctx));
    return std::make_shared<TyTuple>(fields);
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmValue *term, const IncreContext &ctx) {
    return getSyntaxValueType(term->v);
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmPrimary *term, const IncreContext &ctx) {
    auto op_type = getPrimaryType(term->op_name);
    op_type = instantiate(op_type);
    auto oup_ty = getTmpVar(ANY); auto full_ty = oup_ty;
    for (int i = int(term->params.size()) - 1; i >= 0; --i) {
        full_ty = std::make_shared<TyArr>(typing(term->params[i].get(), ctx), full_ty);
    }
    unify(op_type, full_ty); return oup_ty;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmRewrite *term, const IncreContext &ctx) {
    GetTypeAssign(body, ctx);
    checkAndUpdate(SCALAR, body.get());
    return body;
}

syntax::Ty DefaultIncreTypeChecker::_typing(syntax::TmUnlabel *term, const IncreContext &ctx) {
    GetTypeAssign(body, ctx); auto res = getTmpVar(BASE);
    unify(body, std::make_shared<TyCompress>(res));
    return res;
}