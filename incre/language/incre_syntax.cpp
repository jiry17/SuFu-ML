//
// Created by pro on 2023/12/4.
//
#include "istool/incre/language/incre_syntax.h"
#include "glog/logging.h"

using namespace incre::syntax;

TermData::TermData(const TermType &_term_type): term_type(_term_type) {}
TermType TermData::getType() const {return term_type;}

TmValue::TmValue(const Data &_v): TermData(TermType::VALUE), v(_v) {}
std::string TmValue::toString() const {return v.toString();}
TmApp::TmApp(const Term &_func, const Term &_param): TermData(TermType::APP), func(_func), param(_param) {}
std::string TmApp::toString() const {return func->toString() + " " + param->toString();}
TmCons::TmCons(const std::string &_cons_name, const Term &_body): TermData(TermType::CONS), cons_name(_cons_name), body(_body) {}
std::string TmCons::toString() const {return cons_name + " " + body->toString();}
TmFunc::TmFunc(const std::string &_name, const Term &_body): TermData(TermType::FUNC), name(_name), body(_body) {}
std::string TmFunc::toString() const {return "fun " + name + " -> " + body->toString();}
TmIf::TmIf(const Term &_c, const Term &_t, const Term &_f): TermData(TermType::IF), c(_c), t(_t), f(_f) {}
std::string TmIf::toString() const {return "if " + c->toString() + " then " + t->toString() + " else " + f->toString();}
TmLabel::TmLabel(const Term &_body): TermData(TermType::LABEL), body(_body) {}
std::string TmLabel::toString() const {return "label " + body->toString();}
TmUnlabel::TmUnlabel(const Term &_body): TermData(TermType::UNLABEL), body(_body) {}
std::string TmUnlabel::toString() const {return "unlabel " + body->toString();}
TmRewrite::TmRewrite(const Term &_body): TermData(TermType::REWRITE), body(_body) {}
std::string TmRewrite::toString() const {return "rewrite " + body->toString();}
TmLet::TmLet(const std::string &_name, bool _is_rec, const Term &_def, const Term &_body):
    TermData(TermType::LET), name(_name), is_rec(_is_rec), def(_def), body(_body) {}
std::string TmLet::toString() const {
    std::string res = is_rec ? "let rec" : "let";
    return res + " " + name + " = " + def->toString() + " in " + body->toString();
}
TmPrimary::TmPrimary(const std::string &_op_name, const TermList &_params): TermData(TermType::PRIMARY), op_name(_op_name), params(_params) {}
std::string TmPrimary::toString() const {
    if (params.size() == 1) return op_name + " " + params[0]->toString();
    if (params.size() == 2) return params[0]->toString() + " " + op_name + " " + params[1]->toString();
    LOG(FATAL) << "Unexpected op name " << op_name;
}
TmMatch::TmMatch(const Term &_def, const MatchCaseList &_cases): TermData(TermType::MATCH), def(_def), cases(_cases) {}
std::string TmMatch::toString() const {
    std::string res = "match " + def->toString() + " with ";
    for (int i = 0; i < cases.size(); ++i) {
        if (i) res += " | ";
        auto& [pt, case_term] = cases[i];
        res += pt->toString() + " -> " + case_term->toString();
    }
    return res;
}
TmVar::TmVar(const std::string &_name): TermData(TermType::VAR), name(_name) {}
std::string TmVar::toString() const {return name;}
TmTuple::TmTuple(const TermList &_fields): TermData(TermType::TUPLE), fields(_fields) {}
std::string TmTuple::toString() const {
    std::string res = "{";
    for (int i = 0; i < fields.size(); ++i) {
        if (i) res += ",";
        res += fields[i]->toString();
    }
    return res + "}";
}
TmProj::TmProj(const Term &_body, int _id, int _size):
    TermData(TermType::PROJ), body(_body), id(_id), size(_size) {}
std::string TmProj::toString() const {
    return body->toString() + "." + std::to_string(id) + "/" + std::to_string(size);
}

std::string incre::syntax::termType2String(TermType type) {
    switch (type) {
        case TermType::VALUE : return "VALUE";
        case TermType::IF : return "IF";
        case TermType::VAR : return "VAR";
        case TermType::PRIMARY : return "IF";
        case TermType::APP : return "APP";
        case TermType::TUPLE : return "IF";
        case TermType::PROJ : return "PROJ";
        case TermType::FUNC : return "FUNC";
        case TermType::LET : return "LET";
        case TermType::MATCH : return "MATCH";
        case TermType::CONS : return "CONS";
        case TermType::LABEL : return "LABEL";
        case TermType::UNLABEL : return "UNLABEL";
        case TermType::REWRITE : return "REWRITE";
        default : return "unknown TermType";
    }
}

#define SubTermHead(name) TermList _getSubTerms(Tm ## name* term)

namespace {
    SubTermHead(Value) {return {};}
    SubTermHead(App) {return {term->func, term->param};}
    SubTermHead(Cons) {return {term->body};}
    SubTermHead(Func) {return {term->body};}
    SubTermHead(If) {return {term->c, term->t, term->f};}
    SubTermHead(Label) {return {term->body};}
    SubTermHead(Unlabel) {return {term->body};}
    SubTermHead(Rewrite) {return {term->body};}
    SubTermHead(Let) {return {term->def, term->body};}
    SubTermHead(Primary) {return term->params;}
    SubTermHead(Match) {
        TermList res = {term->def};
        for (auto& [_, c]: term->cases) res.push_back(c);
        return res;
    }
    SubTermHead(Var) {return {};}
    SubTermHead(Tuple) {return term->fields;}
    SubTermHead(Proj) {return {term->body};}
}

#define RegisterSubTermCase(name) case TermType::TERM_TOKEN_##name: return _getSubTerms(dynamic_cast<Tm ## name*>(term));
TermList incre::syntax::getSubTerms(TermData *term) {
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(RegisterSubTermCase);
    }
}

std::string incre::syntax::patternType2String(PatternType t) {
    switch(t) {
        case PatternType::CONS : return "CONS";
        case PatternType::TUPLE : return "TUPLE";
        case PatternType::UNDERSCORE : return "UNDERSCORE";
        case PatternType::VAR : return "VAR";
        default : return "unknown pattern type";
    }
}

PatternData::PatternData(const PatternType &_type): type(_type) {}

PatternType PatternData::getType() const {return type;}

PtUnderScore::PtUnderScore(): PatternData(PatternType::UNDERSCORE) {}
std::string PtUnderScore::toString() const {return "_";}
PtVar::PtVar(const std::string &_name, const Pattern &_body): PatternData(PatternType::VAR), name(_name), body(_body) {}
std::string PtVar::toString() const {
    if (body) return "(" + body->toString() + ")@" + name;
    return name;
}
PtCons::PtCons(const std::string &_name, const Pattern &_body): PatternData(PatternType::CONS), name(_name), body(_body) {}
std::string PtCons::toString() const {
    return name + " " + body->toString();
}
PtTuple::PtTuple(const PatternList &_fields): PatternData(PatternType::TUPLE), fields(_fields) {}
std::string PtTuple::toString() const {
    std::string res = "{";
    for (int i = 0; i < fields.size(); ++i) {
        if (i) res += ",";
        res += fields[i]->toString();
    }
    return res + "}";
}

TypeData::TypeData(const TypeType &_type): type(_type) {}

TypeType TypeData::getType() const {return type;}

namespace {
    std::string _wrap(const std::string& s) {
        bool has_space = false;
        for (auto& c: s) if (c == ' ') has_space = true;
        if (!has_space) return s;
        int n = s.length(); if (s[0] != '(' || s[n - 1] != ')') return "(" + s + ")";
        int num = 1;
        for (int i = 1; i < n - 1; ++i) {
            if (s[i] == '(') ++num; else if (s[i] == ')') --num;
            if (num == 0) return "(" + s + ")";
        }
        return s;
    }
}

TyBool::TyBool(): TypeData(TypeType::BOOL) {}
std::string TyBool::toString() const {return "Bool";}
TyInt::TyInt(): TypeData(TypeType::INT) {}
std::string TyInt::toString() const {return "Int";}
TyUnit::TyUnit(): TypeData(TypeType::UNIT) {}
std::string TyUnit::toString() const {return "Unit";}
TyVar::TyVar(const TypeVarInfo &_info): info(_info), TypeData(TypeType::VAR) {}
std::string TyVar::toString() const {
    if (is_bounded()) return get_bound_type()->toString();
    auto [index, level, range] = get_var_info();
    std::string name = "?" + std::to_string(index);
    switch (range) {
        case ANY: return name;
        case SCALAR: return name + "[Scalar]";
        case BASE: return name + "[Base]";
    }
}
bool TyVar::is_bounded() const {return std::holds_alternative<Ty>(info);}
std::tuple<int, int, VarRange> TyVar::get_var_info() const {return std::get<std::tuple<int, int, VarRange>>(info);}

void TyVar::intersectWith(const VarRange &range) {
    assert(!is_bounded());
    auto [index, level, pre_range] = get_var_info();
    info = std::make_tuple(index, level, std::min(range, pre_range));
}
Ty TyVar::get_bound_type() const {return std::get<Ty>(info);}
TyPoly::TyPoly(const std::vector<int> &_var_list, const Ty &_body): TypeData(TypeType::POLY), var_list(_var_list), body(_body) {}
std::string TyPoly::toString() const {
    std::string res = "\\[";
    for (int i = 0; i < var_list.size(); ++i) {
        if (i) res += ",";
        res += std::to_string(var_list[i]);
    }
    return res + "] " + body->toString();
}
TyPolyWithName::TyPolyWithName(const std::vector<std::string> &_name_list, const std::vector<int> &indices,
                               const incre::syntax::Ty &_body):
                               TyPoly(indices, _body), name_list(_name_list) {
}
TyArr::TyArr(const Ty &_inp, const Ty &_oup): inp(_inp), oup(_oup), TypeData(TypeType::ARR) {}
std::string TyArr::toString() const {
    return _wrap(inp->toString()) + " -> " + oup->toString();
}
TyTuple::TyTuple(const TyList &_fields): TypeData(TypeType::TUPLE), fields(_fields) {}
std::string TyTuple::toString() const {
    std::string res;
    for (int i = 0; i < fields.size(); ++i) {
        if (i) res += " * ";
        res += fields[i]->toString();
    }
    return _wrap(res);
}
TyInd::TyInd(const std::string &_name, const TyList &_param_list): name(_name), param_list(_param_list), TypeData(TypeType::IND) {
    // LOG(INFO) << "Ind Type " << name;
}
std::string TyInd::toString() const {
    std::string res = name;
    for (auto& param: param_list) res += " " + param->toString();
    return res;
}
TyCompress::TyCompress(const Ty &_body): TypeData(TypeType::COMPRESS), body(_body) {}
std::string TyCompress::toString() const {
    return "Packed " + body->toString();
}

namespace {
#define SubTypeCase(name) case TypeType::TYPE_TOKEN_## name: return _getSubTypes(dynamic_cast<Ty ## name*>(x));
#define SubTypeHead(name) TyList _getSubTypes(Ty ## name* x)
#define EmptySubTypeCase(name) SubTypeHead(name) {return {};}

    EmptySubTypeCase(Int);
    EmptySubTypeCase(Bool);
    EmptySubTypeCase(Unit);
    SubTypeHead(Var) {
        if (x->is_bounded()) {
            return {x->get_bound_type()};
        }
        return {};
    }
    SubTypeHead(Tuple) {return x->fields;}
    SubTypeHead(Ind) {return x->param_list;}
    SubTypeHead(Arr) {return {x->inp, x->oup};}
    SubTypeHead(Poly) {return {x->body};}
    SubTypeHead(Compress) {return {x->body};}
}

TyList incre::syntax::getSubTypes(TypeData *x) {
    switch (x->getType()) {
        TYPE_CASE_ANALYSIS(SubTypeCase);
    }
}

Binding::Binding(bool _is_cons, const Ty &_type, const Data &_data): is_cons(_is_cons), type(_type), data(_data) {}
Binding::Binding(const Ty &_type): is_cons(false), type(_type), data() {}
Binding::Binding(const Data& _data): is_cons(false), data(_data), type() {}

Data Binding::getData() const {
    if (data.isNull()) LOG(FATAL) << "Term is not bound";
    return data;
}

Ty Binding::getType() const {
    if (!type) LOG(FATAL) << "Ty is not bound";
    return type;
}

namespace {
    void _getVarsInPattern(PatternData* pattern, std::vector<std::string>& names) {
        switch (pattern->getType()) {
            case PatternType::TUPLE: {
                auto* pt = dynamic_cast<PtTuple*>(pattern);
                for (auto& field: pt->fields) _getVarsInPattern(field.get(), names);
                return;
            }
            case PatternType::VAR: {
                auto* pv = dynamic_cast<PtVar*>(pattern);
                names.push_back(pv->name);
                if (pv->body) _getVarsInPattern(pv->body.get(), names);
                return;
            }
            case PatternType::UNDERSCORE: return;
            case PatternType::CONS: {
                auto* pc = dynamic_cast<PtCons*>(pattern);
                _getVarsInPattern(pc->body.get(), names);
                return;
            }
        }
    }
}

std::vector<std::string> incre::syntax::getVarsInPattern(PatternData *pattern) {
    std::vector<std::string> names;
    _getVarsInPattern(pattern, names);
    return names;
}