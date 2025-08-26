//
// Created by pro on 2023/12/7.
//

#include "istool/incre/language/incre_rewriter.h"

using namespace incre;
using namespace incre::syntax;

#define TypeRewriteCase(name) case TypeType::TYPE_TOKEN_ ## name: return _rewrite(dynamic_cast<Ty ## name*>(type.get()), type);
Ty syntax::IncreTypeRewriter::rewrite(const Ty &type) {
    switch (type->getType()) {
        TYPE_CASE_ANALYSIS(TypeRewriteCase);
    }
}

#define TypeRewrite(name) rewrite(type->name)
Ty IncreTypeRewriter::_rewrite(TyArr *type, const Ty &_type) {
    return std::make_shared<TyArr>(TypeRewrite(inp), TypeRewrite(oup));
}

Ty IncreTypeRewriter::_rewrite(TyInd *type, const Ty &_type) {
    TyList param_list;
    for (auto& param: type->param_list) param_list.push_back(rewrite(param));
    return std::make_shared<TyInd>(type->name, param_list);
}

Ty IncreTypeRewriter::_rewrite(TyInt *type, const Ty &_type) {
    return _type;
}

Ty IncreTypeRewriter::_rewrite(TyVar *type, const Ty &_type) {
    if (type->is_bounded()) {
        return rewrite(type->get_bound_type());
    }
    return _type;
}

Ty IncreTypeRewriter::_rewrite(TyBool *type, const Ty &_type) {
    return _type;
}

Ty IncreTypeRewriter::_rewrite(TyPoly *type, const Ty &_type) {
    return std::make_shared<TyPoly>(type->var_list, TypeRewrite(body));
}

Ty IncreTypeRewriter::_rewrite(TyUnit *type, const Ty &_type) {
    return _type;
}

Ty IncreTypeRewriter::_rewrite(TyCompress *type, const Ty &_type) {
    return std::make_shared<TyCompress>(TypeRewrite(body));
}

Ty IncreTypeRewriter::_rewrite(TyTuple *type, const Ty &_type) {
    TyList fields;
    for (auto& field: type->fields) fields.push_back(rewrite(field));
    return std::make_shared<TyTuple>(fields);
}

#define TermRewriteCase(name) case TermType::TERM_TOKEN_##name: {res = _rewrite(dynamic_cast<Tm ## name*>(term.get()), term); break;}

Term IncreTermRewriter::rewrite(const Term &term) {
    Term res;
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(TermRewriteCase);
    }
    return postProcess(term, res);
}

Term IncreTermRewriter::postProcess(const Term &original_term, const Term &res) {
    return res;
}

#define TermRewrite(name) rewrite(term->name)
Term IncreTermRewriter::_rewrite(TmIf *term, const Term &_term) {
    return std::make_shared<TmIf>(TermRewrite(c), TermRewrite(t), TermRewrite(f));
}

Term IncreTermRewriter::_rewrite(TmApp *term, const Term &_term) {
    return std::make_shared<TmApp>(TermRewrite(func), TermRewrite(param));
}

Term IncreTermRewriter::_rewrite(TmLet *term, const Term &_term) {
    return std::make_shared<TmLet>(term->name, term->is_rec, TermRewrite(def), TermRewrite(body));
}

Term IncreTermRewriter::_rewrite(TmVar *term, const Term &_term) {
    return _term;
}

Term IncreTermRewriter::_rewrite(TmCons *term, const Term &_term) {
    return std::make_shared<TmCons>(term->cons_name, TermRewrite(body));
}

Term IncreTermRewriter::_rewrite(TmFunc *term, const Term &_term) {
    return std::make_shared<TmFunc>(term->name, TermRewrite(body));
}

Term IncreTermRewriter::_rewrite(TmProj *term, const Term &_term) {
    return std::make_shared<TmProj>(TermRewrite(body), term->id, term->size);
}

Term IncreTermRewriter::_rewrite(TmLabel *term, const Term &_term) {
    return std::make_shared<TmLabel>(TermRewrite(body));
}

Term IncreTermRewriter::_rewrite(TmMatch *term, const Term &_term) {
    MatchCaseList cases;
    for (auto& [pt, sub_case]: term->cases) {
        cases.emplace_back(pt, rewrite(sub_case));
    }
    return std::make_shared<TmMatch>(TermRewrite(def), cases);
}

Term IncreTermRewriter::_rewrite(TmTuple *term, const Term &_term) {
    TermList fields;
    for (auto& field: term->fields) fields.push_back(rewrite(field));
    return std::make_shared<TmTuple>(fields);
}

Term IncreTermRewriter::_rewrite(TmValue *term, const Term &_term) {
    return _term;
}

Term IncreTermRewriter::_rewrite(TmPrimary *term, const Term &_term) {
    TermList params;
    for (auto& field: term->params) params.push_back(rewrite(field));
    return std::make_shared<TmPrimary>(term->op_name, params);
}

Term IncreTermRewriter::_rewrite(TmRewrite *term, const Term &_term) {
    return std::make_shared<TmRewrite>(TermRewrite(body));
}

Term IncreTermRewriter::_rewrite(TmUnlabel *term, const Term &_term) {
    return std::make_shared<TmUnlabel>(TermRewrite(body));
}

