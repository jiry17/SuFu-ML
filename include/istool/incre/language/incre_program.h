//
// Created by pro on 2023/12/8.
//

#ifndef ISTOOL_INCRE_H
#define ISTOOL_INCRE_H

#include "incre_semantics.h"
#include "incre_rewriter.h"
#include "incre_types.h"

namespace incre {
    enum class CommandType {
        BIND_TERM, DEF_IND, DECLARE, EVAL
    };

    enum class CommandDecorate {
        INPUT, START, SYN_EXTRACT, SYN_COMBINE, SYN_COMPRESS, SYN_EXCLUDE, SYN_NO_PARTIAL, TERM_NUM
    };

    typedef std::unordered_set<CommandDecorate> DecorateSet;

    class CommandData {
    public:
        std::string name;
        CommandType type;
        DecorateSet decos;
        std::string source;
        CommandData(const CommandType& _type, const std::string& _name, const DecorateSet& decos, const std::string& source);
        bool isDecrorateWith(CommandDecorate deco) const;
        CommandType getType() const;
        virtual ~CommandData() = default;
    };
    typedef std::shared_ptr<CommandData> Command;
    typedef std::vector<Command> CommandList;

    class CommandBindTerm: public CommandData {
    public:
        syntax::Term term;
        bool is_rec;
        CommandBindTerm(const std::string& _name, bool _is_func, const syntax::Term& _term, const DecorateSet& decos, const std::string& _source);
    };

    typedef std::vector<std::pair<std::string, syntax::Ty>> IndConstructorInfo;
    class CommandDef: public CommandData {
    public:
        int param;
        IndConstructorInfo cons_list;
        CommandDef(const std::string& _name, int _param, const IndConstructorInfo& _cons_list, const DecorateSet& decos, const std::string& _source);
    };

    class CommandDeclare: public CommandData {
    public:
        syntax::Ty type;
        CommandDeclare(const std::string& _name, const syntax::Ty& _type, const DecorateSet& decos, const std::string& _source);
    };

    class CommandEval: public CommandData {
    public:
        syntax::Term term;
        CommandEval(const std::string& _name, const syntax::Term& _term, const DecorateSet& decos, const std::string& _source);
    };

    enum class IncreConfig {
        COMPOSE_NUM, /*Max components in align, default 3*/
        VERIFY_BASE, /*Base number of examples in verification, default 1000*/
        SAMPLE_SIZE, /*Max size of random data structures, default 10*/
        SAMPLE_INT_MAX, /*Int Max of Sample, Default 5*/
        SAMPLE_INT_MIN, /*Int Min of Sample, Default -5*/
        NON_LINEAR, /*Whether consider * in synthesis, default false*/
        ENABLE_FOLD, /*Whether consider `fold` operator on data structures in synthesis, default false*/
        TERM_NUM, /*Number of terms considered by PolyGen*/
        CLAUSE_NUM, /*Number of terms considered by PolyGen*/
        PRINT_ALIGN, /*Whether print align results to the result*/
        THREAD_NUM, /*The number of threads available for collecting examples*/
        SLOW_COMBINE /*Whether to ignore the O(1) time limit of sketch holes*/
    };

    typedef std::unordered_map<IncreConfig, Data> IncreConfigMap;

    class IncreProgramData {
    public:
        IncreConfigMap config_map;
        CommandList commands;
        IncreProgramData(const CommandList& _commands, const IncreConfigMap& _config_map);
    };
    typedef std::shared_ptr<IncreProgramData> IncreProgram;

    namespace config {
        extern const std::string KComposeNumName;
        extern const std::string KMaxTermNumName;
        extern const std::string KMaxClauseNumName;
        extern const std::string KIsNonLinearName;
        extern const std::string KVerifyBaseName;
        extern const std::string KDataSizeLimitName;
        extern const std::string KIsEnableFoldName;
        extern const std::string KSampleIntMinName;
        extern const std::string KSampleIntMaxName;
        extern const std::string KPrintAlignName;
        extern const std::string KThreadNumName;
        extern const std::string KSlowCombineName;

        IncreConfigMap buildDefaultConfigMap();
        void applyConfig(IncreProgramData* program, Env* env);
    }

    class IncreProgramWalker {
    protected:
        virtual void visit(CommandDef* command) = 0;
        virtual void visit(CommandBindTerm* command) = 0;
        virtual void visit(CommandDeclare* command) = 0;
        virtual void visit(CommandEval* command) = 0;
        virtual void initialize(IncreProgramData* program) {}
        virtual void preProcess(CommandData* program) {}
        virtual void postProcess(CommandData* program) {}
    public:
        void walkThrough(IncreProgramData* program);
        virtual ~IncreProgramWalker() = default;
    };

    class DefaultContextBuilder: public IncreProgramWalker {
    public:
        IncreContext ctx;
        std::unordered_map<std::string, EnvAddress*> address_map;
        DefaultContextBuilder(incre::semantics::IncreEvaluator* _evaluator, incre::types::IncreTypeChecker* _checker);
        virtual ~DefaultContextBuilder() = default;
    protected:
        incre::semantics::IncreEvaluator* evaluator;
        incre::types::IncreTypeChecker* checker;
        virtual void visit(CommandBindTerm* command);
        virtual void visit(CommandEval* command);
        virtual void visit(CommandDef* command);
        virtual void visit(CommandDeclare* command);
    };

    class ProgramRunner: public DefaultContextBuilder {
    public:
        CommandList commands;
        ProgramRunner(incre::semantics::IncreEvaluator* _evaluator, incre::types::IncreTypeChecker* _checker);
        virtual ~ProgramRunner() = default;
    protected:
        virtual void postProcess(CommandData* program);
    };

    IncreFullContext buildContext(IncreProgramData* program, semantics::IncreEvaluator* evaluator, types::IncreTypeChecker* checker);

    IncreProgram runProgram(IncreProgramData* program, semantics::IncreEvaluator* evaluator, types::IncreTypeChecker* checker);

    IncreFullContext buildContext(IncreProgramData* program,
                              const semantics::IncreEvaluatorGenerator& = [](){return nullptr;},
                              const types::IncreTypeCheckerGenerator& = [](){return nullptr;});
}

namespace incre::syntax {
    class IncreProgramRewriter: public IncreProgramWalker {
    protected:
        IncreTypeRewriter* type_rewriter;
        IncreTermRewriter* term_rewriter;
        virtual void visit(CommandDef* command);
        virtual void visit(CommandBindTerm* command);
        virtual void visit(CommandDeclare* command);
        virtual void visit(CommandEval* command);
        virtual void initialize(IncreProgramData* program);
    public:
        IncreProgram res;
        IncreProgramRewriter(IncreTypeRewriter* _type_rewriter, IncreTermRewriter* _term_rewriter);
    };

    IncreProgram rewriteProgram(IncreProgramData* program, const IncreTypeRewriterGenerator& type_gen, const IncreTermRewriterGenerator& term_gen);
}

#endif //ISTOOL_INCRE_H
