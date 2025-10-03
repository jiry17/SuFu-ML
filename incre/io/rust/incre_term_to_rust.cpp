//
// Created by pro on 2025/9/11.
//

#include "istool/incre/io/incre_to_rust.h"
#include "istool/incre/language/incre_util.h"
#include <ranges>
#include <set>
#include <format>
#include "glog/logging.h"

using namespace incre::types;
using namespace incre::syntax;
using namespace incre::rust;

namespace {
    template<typename T, typename F>
    std::string printList(const std::vector<T>& data, const F& func, const std::string& start, const std::string& sep, const std::string last, bool is_ignore_empty) {
        if (data.empty() && is_ignore_empty) return "";
        std::string result = start;
        for (int i = 0; i < data.size(); ++i) {
            if (i) result += sep;
            result += func(data[i]);
        }
        return result + last;
    }
}

std::string
util::funcSignature2String(const incre::rust::util::TypeSignature &signature, const std::string &func_name) {
    std::ostringstream out;
    out << "fn " << func_name;
    out << printList(signature.params, [](const std::pair<std::string, std::string>& info) {return info.first;}, "<", ", ", ">", true);
    out << printList(signature.inp_list, [](const std::pair<std::string, std::string>& info) {return info.first + ": " + info.second;}, "(", ", ", ")", false);
    out << " -> " << signature.oup;
    out << printList(signature.params, [](const std::pair<std::string, std::string>& info) {return info.first + ": " + info.second;}, "\n  where ", ", ", "", true);
    return out.str();
}

namespace {
    std::string _removeBrackets(const std::string& expr) {
        int left = 0, right = expr.length();
        while (left < right && expr[left] == '(' && expr[right - 1] == ')') {
            ++left; --right;
        }
        return expr.substr(left, right - left);
    }

    bool _isAtomic(const std::string& s) {
        for (char c: " (),*") {
            if (s.find(c) != std::string::npos) return false;
        }
        return true;
    }

    std::string _extractOperand(const std::string& expr) {
        auto new_expr = _removeBrackets(expr);
        if (new_expr.starts_with("Rc::new")) {
            auto value = new_expr.substr(7);
            if (_isAtomic(value)) {
                return _removeBrackets(value);
            } else {
                return value;
            }
        } else {
            return "*" + expr;
        }
    }

    std::string _extractLetBody(const std::string& expr) {
        if (!expr.empty() && expr[0] == '{' && expr[expr.length() - 1] == '}') {
            return expr.substr(1, expr.length() - 2);
        } else {
            return expr;
        }
    }

    std::string _extractReference(const std::string& expr) {
        auto new_expr = _removeBrackets(expr);
        if (new_expr.ends_with("clone()")) {
            return new_expr.substr(0, new_expr.length() - 7) + "as_ref()";
        } else {
            return expr + ".as_ref()";
        }
    }
    std::string _wrapWith(char l, const std::string& inner, char r) {
        if (_isAtomic(inner)) return inner;
        if (inner.starts_with(l) && inner.ends_with(r)) {
            int counter = 0;
            for (int i = 1; i + 1 < inner.length(); ++i) {
                if (inner[i] == l) counter++;
                if (inner[i] == r) counter--;
                if (counter < 0) return std::format("{}{}{}", l, inner, r);
            }
            return inner;
        }
        return std::format("{}{}{}", l, inner, r);
    }

    std::string _getOpName(const std::string& name) {
        if (name == "and") return "&&";
        if (name == "or") return "||";
        if (name == "not") return "!";
        return name;
    }

    Term _processLefDef(const std::string& current_name, const Term& term) {
        if (term->getType() != TermType::FUNC) return term;
        auto name = util::getAuxFuncName(false);
        return incre::util::renameVariable(term, current_name, name);
    }

    class _Term2RustWalker {
    public:
        std::vector<std::string> context_functions;
        incre::IncreContext global;
        util::RustContext global_rust_context;
        Term full_term;
        IncreTypeChecker* type_checker;

        _Term2RustWalker(const incre::IncreContext& _context, IncreTypeChecker* _checker, const util::RustContext& global, const Term& _full_term):
        global(_context), type_checker(_checker), global_rust_context(global), full_term(_full_term) {
        }

#define Term2RustHead(Name) std::string _rewrite ## Name (Tm ## Name* term, const Term& _term, const util::RustContext& ctx)
#define Term2RustCase(Name) case TermType::TERM_TOKEN_ ## Name: return _rewrite ## Name (dynamic_cast<Tm ## Name*>(term.get()), term, ctx)
#define Term2RustUnexpected(Name) Term2RustHead(Name) { \
    throw TranslationError("unexpected term", term->toString()); \
}

        Term2RustHead(Value) {
            auto data = term->v;
            {
                auto* di = dynamic_cast<incre::semantics::VInt*>(data.get());
                if (di) return std::format("Rc::new({})", di->w);
            }
            {
                auto* db = dynamic_cast<incre::semantics::VBool*>(data.get());
                if (db) return std::format("Rc::new({})", db->w ? "true" : "false");
            }
            {
                auto* du = dynamic_cast<incre::semantics::VUnit*>(data.get());
                if (du) return std::format("Rc::new(())");
            }
            throw TranslationError(std::format("unexpected value {}", data.toString()), full_term->toString());
        }

        Term2RustHead(Var) {
            auto res = ctx.lookup(term->name, true);
            return res->expr;
        }

        Term2RustHead(Cons) {
            auto result = rewrite(term->body, ctx);
            return std::format("Rc::new({}({}))", ctx.lookup(term->cons_name)->expr, result);
        }

        Term2RustHead(Func) {
            std::string func_name = util::getAuxFuncName();
            auto [new_context, related_functions] = util::function2Rust(_term, global, func_name, global_rust_context, type_checker);
            for (auto& function: related_functions) {
                context_functions.push_back(function);
            }
            auto* func_info = new_context.lookup(func_name);
            return func_info->expr;
        }

        Term2RustHead(Proj) {
            return std::format("{}.{}", rewrite(term->body, ctx), term->id - 1);
        }

        Term2RustHead(If) {
            return std::format("(if {} {} else {})", _extractOperand(rewrite(term->c, ctx)), _wrapWith('{', rewrite(term->t, ctx), '}'), _wrapWith('{', rewrite(term->f, ctx), '}'));
        }

        Term2RustHead(Tuple) {
            return printList(
                    term->fields,
                    [&](const Term& field) {return this->rewrite(field, ctx);},
                    "Rc::new((", ", ", "))", false);
        }

        Term2RustHead(Primary) {
            if (term->params.size() == 1) {
                auto content = rewrite(term->params[0], ctx);
                return std::format("Rc::new({}{})", _getOpName(term->op_name), _extractOperand(content));
            }
            if (term->params.size() == 2) {
                auto x = _extractOperand(rewrite(term->params[0], ctx));
                auto y = _extractOperand(rewrite(term->params[1], ctx));
                return std::format("Rc::new({}{}{})", x, _getOpName(term->op_name), y);
            }
            throw TranslationError("unexpected operator " + term->op_name, full_term->toString());
        }

        Term2RustHead(App) {
            std::vector<std::string> inputs;
            auto func = _term;
            while (func->getType() == TermType::APP) {
                auto* ta = dynamic_cast<TmApp*>(func.get());
                func = ta->func;
                inputs.push_back(rewrite(ta->param, ctx));
            }
            std::reverse(inputs.begin(), inputs.end());
            auto params = printList(inputs, [](const std::string& name) {return name;}, "(", ", ", ")", false);
            auto raw_func_name = rewrite(func, ctx);
            auto func_name = _extractOperand(raw_func_name);
            // LOG(INFO) << "func " << func_name << " " << raw_func_name;
            return std::format("({}{})", _wrapWith('(', func_name, ')'), params);
        }

        Term2RustHead(Let) {
            auto def = rewrite(_processLefDef(term->name, term->def), ctx);
            auto first_line = std::format("let {} = {};", term->name, def);
            auto new_ctx = ctx.insert(term->name, std::format("{}.clone()", term->name));
            auto second_line = _extractLetBody(rewrite(term->body, new_ctx));
            return std::format("{{{}\n{}}}", first_line, second_line);
        }

        util::RustContext insertPatternBind(PatternData* pattern, const util::RustContext& ctx) {
            if (pattern->getType() == PatternType::UNDERSCORE) return ctx;
            if (pattern->getType() == PatternType::VAR) {
                auto* pv = dynamic_cast<PtVar*>(pattern);
                if (!pv->body) return ctx.insert(pv->name, std::format("{}.clone()", pv->name));
            }
            throw TranslationError("unexpected pattern " + pattern->toString(), full_term->toString());
        }

        std::string processCase(PatternData* pattern, const Term& term, const util::RustContext& ctx)  {
            switch (pattern->getType()) {
                case PatternType::TUPLE: {
                    auto* pt = dynamic_cast<PtTuple*>(pattern);
                    auto new_ctx = ctx;
                    for (auto& field: pt->fields) {
                        new_ctx = insertPatternBind(field.get(), new_ctx);
                    }
                    auto pattern_str = printList(pt->fields, [](const Pattern& pattern) {return pattern->toString();}, "(", ", ", ")", false);
                    return std::format("{} => {}", pattern_str, rewrite(term, new_ctx));
                }
                case PatternType::CONS: {
                    auto* pc = dynamic_cast<PtCons*>(pattern);
                    auto new_ctx = insertPatternBind(pc->body.get(), ctx);
                    auto pattern_str = std::format("{}({})", ctx.lookup(pc->name)->expr, pc->body->toString());
                    return std::format("{} => {}", pattern_str, rewrite(term, new_ctx));
                }
                default: {
                    auto new_ctx = insertPatternBind(pattern, ctx);
                    return std::format("{} => {}", pattern->toString(), rewrite(term, new_ctx));
                }
            }
        }

        bool isCaseComplete(TmMatch* term, const util::RustContext& ctx) {
            std::string structure_name; int cons_num = 0;
            for (auto& [pattern, _]: term->cases) {
                auto* pc = dynamic_cast<PtCons*>(pattern.get());
                if (!pc) return true;
                cons_num++;
                auto cons_name = ctx.lookup(pc->name)->expr; auto index = cons_name.find("::");
                if (index == std::string::npos) throw TranslationError("unexpected cons name " + pc->name, full_term->toString());
                structure_name = cons_name.substr(0, index + 2);
            }

            int expected_num = 0;
            for (auto entry = ctx.start; entry; entry = entry->next_entry) {
                if (entry->expr.starts_with(structure_name)) ++expected_num;
            }
            // LOG(INFO) << structure_name << " " << cons_num << " " << expected_num << std::endl;
            return expected_num == cons_num;
        }

        Term2RustHead(Match) {
            auto def = rewrite(term->def, ctx);
            std::string result = std::format("match {} {{\n", _extractReference(def));
            for (auto& [pattern, sub_term]: term->cases) {
                result += processCase(pattern.get(), sub_term, ctx) + ",\n";
            }
            if (!isCaseComplete(term, ctx)) {
                result += "_ => panic!(\"unexpected case\"),\n";
            }
            return result + "}";
        }

        Term2RustUnexpected(Label)
        Term2RustUnexpected(Unlabel)
        Term2RustUnexpected(Rewrite)



        std::string rewrite(const Term& term, const util::RustContext& ctx) {
            switch (term->getType()) {
                TERM_CASE_ANALYSIS(Term2RustCase)
            }
        }
    };
}

std::pair<util::RustContext, std::vector<std::string>>
util::function2Rust(const syntax::Term &term, const incre::IncreContext &global,
                    const std::string &func_name, const incre::rust::util::RustContext &rust_ctx,
                    IncreTypeChecker* type_checker) {
    // check free variables
    {
        auto free_variables = incre::util::getFreeVariables(term.get());
        for (auto& name: free_variables) {
            if (!global.isContain(name) && name != func_name) {
                throw TranslationError("found free variable " + name, term->toString());
            }
        }
    }

    // get function type
    type_checker->pushLevel();
    auto func_type_var = type_checker->getTmpVar(ANY);
    auto inner_ctx = global.insert(func_name, func_type_var);
    type_checker->unify(func_type_var, type_checker->typing(term.get(), inner_ctx));
    type_checker->popLevel();
    auto generalized_type = type_checker->generalize(func_type_var, term.get());
    inner_ctx = global.insert(func_name, generalized_type);
    auto func_type = util::unfoldBoundVariable(func_type_var);

    // get input variables and filter out curried functions
    std::vector<std::string> inp_names; syntax::Term body(term); syntax::Ty body_type(func_type);
    while (body->getType() == TermType::FUNC) {
        auto* tf = dynamic_cast<TmFunc*>(body.get());
        assert(body_type->getType() == TypeType::ARR);
        inp_names.push_back(tf->name);
        body = tf->body;
        auto* ra = dynamic_cast<TyArr*>(body_type.get());
        body_type = ra->oup;
    }
    if (body_type->getType() == TypeType::ARR) {
        LOG(FATAL) << "The current translator cannot support curried function " << func_name;
    }
    auto signature = buildTypeSignature(inp_names, func_type.get());

    // build context
    auto new_rust_ctx = rust_ctx.insert(func_name, std::format("Rc::new({})", func_name));
    std::ostringstream result;
    result << funcSignature2String(signature, func_name) << "{\n";

    // build body
    _Term2RustWalker walker(inner_ctx, type_checker, new_rust_ctx, term);
    auto local_rust_ctx = new_rust_ctx;
    for (auto& inp_name: inp_names) {
        local_rust_ctx = local_rust_ctx.insert(inp_name, std::format("{}.clone()", inp_name));
    }
    result << indent(1) << walker.rewrite(body, local_rust_ctx) << "\n}\n";

    auto final_result = walker.context_functions;
    final_result.push_back(result.str());
    return {new_rust_ctx, final_result};
}