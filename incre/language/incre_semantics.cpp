//
// Created by pro on 2023/12/5.
//

#include "istool/incre/language/incre_semantics.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::semantics;
using namespace incre::syntax;

IncreSemanticsError::IncreSemanticsError(const std::string _message): message(_message) {}

const char *IncreSemanticsError::what() const noexcept {return message.c_str();}

std::string VUnit::toString() const {return "unit";}

bool VUnit::equal(Value *value) const {
    return dynamic_cast<VUnit*>(value);
}
VClosure::VClosure(const IncreContext &_context, const std::string &_name, const syntax::Term &_body):
    context(_context), name(_name), body(_body) {
}
VClosure::VClosure(const std::tuple<IncreContext, std::string, syntax::Term> &_content):
    context(std::get<0>(_content)), name(std::get<1>(_content)), body(std::get<2>(_content)) {
}

VCompress::VCompress(const Data &_body): body(_body) {}

bool VCompress::equal(Value *value) const {
    auto* vc = dynamic_cast<VCompress*>(value);
    return vc && body == vc->body;
}
std::string VCompress::toString() const {
    return "compress " + body.toString();
}

std::string VClosure::toString() const {
    return "func " + name;
    //todo: write term to string
}
bool VClosure::equal(Value *value) const {
    return false;
}

VInd::VInd(const std::string &_name, const Data &_body): name(_name), body(_body) {
}
VInd::VInd(const std::pair<std::string, Data> &_content): name(_content.first), body(_content.second) {
}

std::string VInd::toString() const {
    return name + " " + body.toString();
}

bool VInd::equal(Value *value) const {
    auto* vi = dynamic_cast<VInd*>(value);
    return name == vi->name && body == vi->body;
}

#define INT_BINARY(sop, op, oup) if (name == sop) return BuildData(oup, theory::clia::getIntValue(params[0]) op theory::clia::getIntValue(params[1]))
#define BOOL_BINARY(sop, op) if (name == sop) return BuildData(Bool, params[0].isTrue() op params[1].isTrue())


Data incre::semantics::invokePrimary(const std::string &name, const DataList &params) {
    INT_BINARY("+", +, Int); INT_BINARY("-", -, Int);
    INT_BINARY("*", *, Int); INT_BINARY("/", /, Int);
    if (name == "==") return BuildData(Bool, params[0] == params[1]);
    INT_BINARY("<", <, Bool); INT_BINARY("<=", <=, Bool);
    INT_BINARY(">", >, Bool); INT_BINARY(">=", >=, Bool);
    if (name == "neg") return BuildData(Int, -theory::clia::getIntValue(params[0]));
    BOOL_BINARY("and", &&); BOOL_BINARY("or", ||);
    if (name == "not") return BuildData(Bool, !params[0].isTrue());
    LOG(FATAL) << "unknown primary operator " << name;
}

#define EvalCase(name) \
  case TermType:: TERM_TOKEN_ ## name: {res = _evaluate(dynamic_cast<Tm ## name*>(term), ctx); break;}

Data incre::semantics::IncreEvaluator::evaluate(syntax::TermData *term, const IncreContext &ctx) {
    preProcess(term, ctx); Data res;
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(EvalCase);
    }
    postProcess(term, ctx, res);
    return res;
}

void DefaultEvaluator::preProcess(syntax::TermData *term, const IncreContext &ctx) {}
void DefaultEvaluator::postProcess(syntax::TermData *term, const IncreContext &ctx, const Data& res) {}

#define Eval(name, ctx_name) evaluate(term->name.get(), ctx_name)
#define EvalAssign(name, ctx_name) auto name = Eval(name, ctx_name)
Data DefaultEvaluator::_evaluate(syntax::TmIf *term, const IncreContext &ctx) {
    EvalAssign(c, ctx); if (c.isTrue()) return Eval(t, ctx); else return Eval(f, ctx);
}

Data DefaultEvaluator::_evaluate(syntax::TmApp *term, const IncreContext &ctx) {
    EvalAssign(func, ctx); EvalAssign(param, ctx);
    auto* vc = dynamic_cast<VClosure*>(func.get());
    if (!vc) throw IncreSemanticsError("the evaluation result of TmApp func should be a closure, but got " + func.toString());
    auto new_ctx = vc->context.insert(vc->name, param);
    return evaluate(vc->body.get(), new_ctx);
}

Data DefaultEvaluator::_evaluate(syntax::TmLet *term, const IncreContext &ctx) {
    if (term->is_rec) {
        auto new_ctx = ctx.insert(term->name, Data());
        new_ctx.start->bind.data = Eval(def, new_ctx);
        return Eval(body, new_ctx);
    } else {
        EvalAssign(def, ctx);
        auto new_ctx = ctx.insert(term->name, def);
        return Eval(body, new_ctx);
    }
}

Data DefaultEvaluator::_evaluate(syntax::TmVar *term, const IncreContext &ctx) {
    return ctx.getData(term->name);
}

Data DefaultEvaluator::_evaluate(syntax::TmCons *term, const IncreContext &ctx) {
    EvalAssign(body, ctx);
    return Data(std::make_shared<VInd>(term->cons_name, body));
}

Data DefaultEvaluator::_evaluate(syntax::TmFunc *term, const IncreContext &ctx) {
    return Data(std::make_shared<VClosure>(ctx, term->name, term->body));
}

Data DefaultEvaluator::_evaluate(syntax::TmProj *term, const IncreContext &ctx) {
    EvalAssign(body, ctx);
    auto* vt = dynamic_cast<VTuple*>(body.get());
    if (!vt) throw IncreSemanticsError("the evaluation result of TmProj body should be a tuple, but got " + body.toString());
    if (vt->elements.size() != term->size || vt->elements.size() < term->id) {
        throw IncreSemanticsError("Incorrect tuple size, expected a type of size " + std::to_string(term->size) + ", but got " + body.toString());
    }
    return vt->elements[term->id - 1];
}

Data DefaultEvaluator::_evaluate(syntax::TmLabel *term, const IncreContext &ctx) {
    return Data(std::make_shared<VCompress>(Eval(body, ctx)));
}

bool incre::semantics::isValueMatchPattern(PatternData *pattern, const Data &data) {
    switch (pattern->getType()) {
        case PatternType::UNDERSCORE: return true;
        case PatternType::VAR: {
            auto* pt = dynamic_cast<PtVar*>(pattern);
            if (pt->body) return isValueMatchPattern(pt->body.get(), data);
            return true;
        }
        case PatternType::TUPLE: {
            auto* pt = dynamic_cast<PtTuple*>(pattern);
            auto* vt = dynamic_cast<VTuple*>(data.get());
            if (!vt || vt->elements.size() != pt->fields.size()) return false;
            for (int i = 0; i < pt->fields.size(); ++i) {
                if (!isValueMatchPattern(pt->fields[i].get(), vt->elements[i])) return false;
            }
            return true;
        }
        case PatternType::CONS: {
            auto* pt = dynamic_cast<PtCons*>(pattern);
            auto* vi = dynamic_cast<VInd*>(data.get());
            if (!vi || vi->name != pt->name) return false;
            return isValueMatchPattern(pt->body.get(), vi->body);
        }
    }
}

IncreContext incre::semantics::bindValueWithPattern(syntax::PatternData *pattern, const Data &data, const IncreContext &ctx) {
    switch (pattern->getType()) {
        case PatternType::UNDERSCORE: return ctx;
        case PatternType::VAR: {
            auto* pv = dynamic_cast<PtVar*>(pattern);
            auto new_ctx = ctx.insert(pv->name, data);
            if (pv->body) {
                return bindValueWithPattern(pv->body.get(), data, new_ctx);
            }
            return new_ctx;
        }
        case PatternType::TUPLE: {
            auto* pt = dynamic_cast<PtTuple*>(pattern);
            auto* vt = dynamic_cast<VTuple*>(data.get());
            if (!vt || vt->elements.size() != pt->fields.size()) {
                throw IncreSemanticsError("value " + data.toString() + "does not match pattern");
            }
            auto res = ctx;
            for (int i = 0; i < pt->fields.size(); ++i) {
                res = bindValueWithPattern(pt->fields[i].get(), vt->elements[i], res);
            }
            return res;
        }
        case PatternType::CONS: {
            auto* pc = dynamic_cast<PtCons*>(pattern);
            auto* vi = dynamic_cast<VInd*>(data.get());
            if (!vi || vi->name != pc->name) {
                throw IncreSemanticsError("value " + data.toString() + "does not match pattern");
            }
            return bindValueWithPattern(pc->body.get(), vi->body, ctx);
        }
    }
}

Data DefaultEvaluator::_evaluate(syntax::TmMatch *term, const IncreContext &ctx) {
    EvalAssign(def, ctx);
    for (auto& [pt, tm]: term->cases) {
        if (isValueMatchPattern(pt.get(), def)) {
            auto new_ctx = bindValueWithPattern(pt.get(), def, ctx);
            return evaluate(tm.get(), new_ctx);
        }
    }
    throw IncreSemanticsError("cannot match " + def.toString() + " with " + term->toString());
}

Data DefaultEvaluator::_evaluate(syntax::TmTuple *term, const IncreContext &ctx) {
    DataList elements;
    for (auto& sub_term: term->fields) {
        elements.push_back(evaluate(sub_term.get(), ctx));
    }
    return BuildData(Product, elements);
}

Data DefaultEvaluator::_evaluate(syntax::TmValue *term, const IncreContext &ctx) {
    return term->v;
}

Data DefaultEvaluator::_evaluate(syntax::TmPrimary *term, const IncreContext &ctx) {
    DataList param_list;
    for (auto& param: term->params) param_list.push_back(evaluate(param.get(), ctx));
    return semantics::invokePrimary(term->op_name, param_list);
}

Data DefaultEvaluator::_evaluate(syntax::TmRewrite *term, const IncreContext &ctx) {
    return Eval(body, ctx);
}

Data DefaultEvaluator::_evaluate(syntax::TmUnlabel *term, const IncreContext &ctx) {
    EvalAssign(body, ctx);
    auto* vc = dynamic_cast<VCompress*>(body.get());
    if (!vc) throw IncreSemanticsError("the body of TmUnlabel should be a compress value, but got " + body.toString());
    return vc->body;
}