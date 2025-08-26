//
// Created by pro on 2023/12/15.
//

#ifndef ISTOOL_INCRE_GRAMMAR_SEMANTICS_H
#define ISTOOL_INCRE_GRAMMAR_SEMANTICS_H

#include "istool/basic/semantics.h"
#include "istool/incre/language/incre_semantics.h"

namespace incre::semantics {
    class IncreGloablExternalEvaluator: public semantics::DefaultEvaluator {
    protected:
        RegisterEvaluateCase(Var);
        std::unordered_map<std::string, Data> global_input;
    public:
        IncreGloablExternalEvaluator(const std::unordered_map<std::string, Data>& _global_input);
        virtual ~IncreGloablExternalEvaluator() = default;
    };

    class IncreExecutionInfo: public ExecuteInfo {
    public:
        IncreGloablExternalEvaluator* eval;
        IncreExecutionInfo(const DataList& param_list, const std::unordered_map<std::string, Data>& global_input);
        virtual ~IncreExecutionInfo();
    };

    class IncreExecutionInfoBuilder: public ExecuteInfoBuilder {
    public:
        std::vector<std::string> global_name;
        bool is_enable;
        IncreExecutionInfoBuilder(const std::vector<std::string>& _global_name);
        virtual ExecuteInfo* buildInfo(const DataList& _param_value, const FunctionContext& ctx);
    };

    class IncreOperatorSemantics: public FullExecutedSemantics {
    public:
        Data func;
        IncreOperatorSemantics(const std::string& name, const Data& _func);
        virtual Data run(DataList&& inp_list, ExecuteInfo* info);
    };

    struct TypeLabeledDirectSemantics: public NormalSemantics {
    public:
        PType type;
        TypeLabeledDirectSemantics(const PType& _type);
        virtual Data run(DataList&& inp_list, ExecuteInfo* info);
        virtual ~TypeLabeledDirectSemantics() = default;
    };

    Data invokeApp(const Data& func, const DataList& param_list, IncreExecutionInfo* info);
    void registerIncreExecutionInfo(Env* env, const std::vector<std::string>& global_names);
    void isConsiderGlobalInputs(Env* env, bool new_flag);
}

#endif //ISTOOL_INCRE_GRAMMAR_SEMANTICS_H
