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
    void printList(std::ostream& stream, const std::vector<T>& data, const F& func, const std::string& start, const std::string& sep, const std::string last, bool is_ignore_empty) {
        if (data.empty() && !is_ignore_empty) return;
        stream << start;
        for (int i = 0; i < data.size(); ++i) {
            if (i) stream << sep;
            stream << func(data[i]);
        }
        stream << last;
    }
}

std::string
util::funcSignature2String(const incre::rust::util::TypeSignature &signature, const std::string &func_name) {
    std::ostringstream out;
    out << "fn " << func_name;
    printList(out, signature.params, [](const std::pair<std::string, std::string>& info) {return info.first;}, "<", ", ", ">", true);
    printList(out, signature.inp_list, [](const std::pair<std::string, std::string>& info) {return info.first + ": " + info.second;}, "(", ", ", ")", false);
    out << " -> " << signature.oup;
    printList(out, signature.params, [](const std::pair<std::string, std::string>& info) {return info.first + ": " + info.second;}, "\n  where ", ", ", "", true);
    return out.str();
}

util::RecursiveFunctionInfo::RecursiveFunctionInfo(const std::string &_name,
                                                   const incre::rust::util::TypeSignature &info): name(_name) {
    for (auto& [input_name, _]: info.inp_list) {
        if (!global_params.empty()) global_params += ", ";
        global_params += input_name + ".clone()";
    }
}

std::string util::RecursiveFunctionInfo::buildFunctionCall(const std::string &input) {
    std::string result = name + "(" + global_params;
    if (!global_params.empty()) result += ", ";
    return result + input + ")";
}

namespace {

    using RecursiveInfoMap = std::unordered_map<std::string, util::RecursiveFunctionInfo>;

    class _Term2RustWalker {
    public:
        RecursiveInfoMap recursive_info;
        std::vector<std::string> context_functions;
        incre::IncreContext global_context;

        _Term2RustWalker(const RecursiveInfoMap& _recursive_info, const incre::IncreContext& _context):
            recursive_info(std::move(_recursive_info)), global_context(_context) {
        }

#define Term2RustHead(Name) std::string _rewrite ## Name (Tm ## Name* term, const Term& _term)
#define Term2RustCase(Name) case TermType::TERM_TOKEN_ ## Name: return _rewrite ## Name (dynamic_cast<Tm ## Name*>(term), term)

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
            LOG(FATAL) << "unexpected value " << data.toString() << " during the translation to rust";
        }

        Term2RustHead(Var) {
            return std::format("{}.clone()", term->name);
        }

        Term2RustHead(Cons) {
            auto result = rewrite(term->body.get());
            return std::format("Rc::new({}({}))", term->cons_name, result);
        }

        Term2RustHead(Func) {
            auto [func_info, related_functions] = util::function2Rust(_term, global_context, std::nullopt, recursive_info);
            for (auto& func: related_functions) context_functions.push_back(func);
            return std::format("Rc::new({})", func_info. )
        }

        std::string rewrite(TermData* term) {
            switch (term->getType()) {
                TERM_CASE_ANALYSIS(Term2RustCase)
            }
        }
    };
}

std::pair<util::RecursiveFunctionInfo, std::vector<std::string>>
util::function2Rust(const syntax::Term &term, const incre::IncreContext &ctx, const std::optional<std::string> &name,
                    const std::unordered_map<std::string, RecursiveFunctionInfo> &rec_infos) {
    auto func_name = name.has_value() ? name.value() : getAuxFuncName();
    DefaultIncreTypeChecker checker;
    auto func_var = checker.getTmpVar(ANY);
    auto func_ctx = ctx.insert(func_name, func_var);

    auto free_variables = incre::util::getFreeVariables(term.get());
    int remain_index = 0;
    for (auto& variable: free_variables) {
        if (!func_ctx.isContain(variable)) {
            free_variables[remain_index++] = variable;
        }
    }
    free_variables.resize(remain_index);

    std::ostringstream result;

    // get full type
    auto func_term = term;
    for (auto& free_variable: free_variables | std::views::reverse) {
        func_term = std::make_shared<TmFunc>(free_variable, func_term);
    }
    auto func_type = checker.typing(func_term.get(), func_ctx);
    checker.unify(func_var, func_type);
    func_type = util::unfoldBoundVariable(func_type);

    // build signature
    LOG(INFO) << func_type->toString();
    std::vector<std::string> input_names;
    auto current_term = func_term;
    for (int i = 0; i <= free_variables.size(); ++i) {
        if (current_term->getType() == TermType::FUNC) {
            auto tf = dynamic_cast<TmFunc*>(current_term.get());
            input_names.push_back(tf->name);
            current_term = tf->body;
        } else {
            assert(i == free_variables.size());
        }
    }
    auto type_signature = buildTypeSignature(input_names, func_type.get());
    result << funcSignature2String(type_signature, func_name) << "{\n";
    RecursiveFunctionInfo info(func_name, type_signature);
    rec_infos.insert({func_name, info});

    _Term2RustWalker term_rewriter(rec_infos, ctx);
    result << term_rewriter.rewrite(term.get()) << "\n}\n";

    auto result_list = term_rewriter.context_functions;
    result_list.push_back(result.str());
    return {info, result_list};
}