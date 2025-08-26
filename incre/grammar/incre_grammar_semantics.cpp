//
// Created by pro on 2023/12/16.
//

#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "glog/logging.h"

using namespace incre::semantics;
using namespace incre::syntax;

incre::semantics::IncreGloablExternalEvaluator::IncreGloablExternalEvaluator(
        const std::unordered_map<std::string, Data> &_global_input): global_input(_global_input) {
}

Data incre::semantics::IncreGloablExternalEvaluator::_evaluate(syntax::TmVar *term, const IncreContext &ctx) {
    for (auto pos = ctx.start; pos; pos = pos->next) {
        if (pos->name == term->name) {
            if (!pos->bind.data.isNull()) return pos->bind.data;
            auto it = global_input.find(term->name);
            if (it != global_input.end()) return it->second;
        }
    }
    throw incre::semantics::IncreSemanticsError("Unknown variable " + term->name);
}

incre::semantics::IncreExecutionInfo::IncreExecutionInfo(const DataList &_param_list,
                                                         const std::unordered_map<std::string, Data> &global_input):
                                                         ExecuteInfo(_param_list, {}) {
    eval = new IncreGloablExternalEvaluator(global_input);
}
incre::semantics::IncreExecutionInfo::~IncreExecutionInfo() noexcept {
    delete eval;
}

Data incre::semantics::invokeApp(const Data &func, const DataList &param_list, IncreExecutionInfo *info) {
    Term full_term = std::make_shared<TmValue>(func);
    for (auto& param: param_list) full_term = std::make_shared<TmApp>(full_term, std::make_shared<TmValue>(param));
    return info->eval->evaluate(full_term.get(), IncreContext(nullptr));
}

incre::semantics::IncreOperatorSemantics::IncreOperatorSemantics(const std::string &name, const Data &_func):
    FullExecutedSemantics(name), func(_func) {
}

Data incre::semantics::IncreOperatorSemantics::run(DataList &&inp_list, ExecuteInfo *info) {
    auto* incre_info = dynamic_cast<IncreExecutionInfo*>(info);
    if (!incre_info) LOG(FATAL) << "IncreOperatorSemantics can only be evaluated using IncreExecutionInfo";
    try {
        return incre::semantics::invokeApp(func, inp_list, incre_info);
    } catch (const IncreSemanticsError& e) {
        throw SemanticsError();
    }
}

incre::semantics::IncreExecutionInfoBuilder::IncreExecutionInfoBuilder(const std::vector<std::string> &_global_name):
    global_name(_global_name), is_enable(true) {
}

ExecuteInfo *
incre::semantics::IncreExecutionInfoBuilder::buildInfo(const DataList &_param_value, const FunctionContext &ctx) {
    if (!is_enable) {
        return new IncreExecutionInfo(_param_value, {});
    }
    std::unordered_map<std::string, Data> global_input;
    int start = int(_param_value.size()) - int(global_name.size());
    for (int i = 0; i < global_name.size(); ++i) {
        global_input[global_name[i]] = _param_value[start + i];
    }
    return new IncreExecutionInfo(_param_value, global_input);
}

void incre::semantics::registerIncreExecutionInfo(Env* env, const std::vector<std::string> &global_names) {
    env->setExecuteInfoBuilder(new IncreExecutionInfoBuilder(global_names));
}

void incre::semantics::isConsiderGlobalInputs(Env* env, bool new_flag) {
    auto* builder = dynamic_cast<IncreExecutionInfoBuilder*>(env->getExecuteInfoBuilder());
    if (!builder) LOG(FATAL) << "The currect builder is not IncreExecutionInfoBuilder";
    builder->is_enable = new_flag;
}

TypeLabeledDirectSemantics::TypeLabeledDirectSemantics(const PType &_type): NormalSemantics(_type->getName(), _type, {_type}), type(_type) {
}
Data TypeLabeledDirectSemantics::run(DataList &&inp_list, ExecuteInfo *info) {
    return inp_list[0];
}

