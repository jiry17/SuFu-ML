//
// Created by pro on 2022/9/23.
//

#ifndef ISTOOL_INCRE_INSTRU_RUNTIME_H
#define ISTOOL_INCRE_INSTRU_RUNTIME_H

#include "istool/incre/language/incre_program.h"
#include "istool/basic/example_sampler.h"
#include "incre_instru_types.h"
#include <random>
#include <unordered_set>

namespace incre::example {
    struct IncreExampleData {
        int rewrite_id;
        DataList local_inputs, global_inputs;
        Data oup;
        IncreExampleData(int _rewrite_id, const DataList& _local, const DataList& _global, const Data& _oup);
        std::string toString() const;
        virtual ~IncreExampleData() = default;
    };

    syntax::Ty getContentTypeForGen(syntax::TypeData* res, syntax::TypeData* cons_ty);

    typedef std::shared_ptr<IncreExampleData> IncreExample;
    typedef std::vector<IncreExample> IncreExampleList;

    class IncreDataGenerator {
    public:
        Env* env;
        int KSizeLimit, KIntMin, KIntMax;
        std::unordered_map<std::string, CommandDef*> cons_map;
        Data getRandomInt();
        Data getRandomBool();
        IncreDataGenerator(Env* _env, const std::unordered_map<std::string, CommandDef*>& _cons_map);
        virtual Data getRandomData(const syntax::Ty& type) = 0;
        virtual ~IncreDataGenerator() = default;
    };

    std::unordered_map<std::string, CommandDef*> extractConsMap(IncreProgramData* program);

    typedef std::variant<std::pair<std::string, syntax::Ty>, std::vector<int>> SizeSplitScheme;
    typedef std::vector<SizeSplitScheme> SizeSplitList;

    class SizeSafeValueGenerator: public IncreDataGenerator {
    public:
        std::unordered_map<std::string, SizeSplitList*> split_map;
        SizeSplitList* getPossibleSplit(syntax::TypeData* type, int size);
        SizeSafeValueGenerator(Env* _env, const std::unordered_map<std::string, CommandDef*>& _ind_cons_map);
        virtual Data getRandomData(const syntax::Ty& type);
        virtual ~SizeSafeValueGenerator();
    };

    class IncreExampleCollector;

    class IncreExampleCollectionEvaluator: public incre::semantics::IncreLabeledEvaluator {
    protected:
        RegisterEvaluateCase(Rewrite);
    public:
        IncreExampleCollector* collector;
        IncreExampleCollectionEvaluator(IncreExampleCollector* _collector);
    };

    class IncreExampleCollector {
    public:
        std::vector<IncreExampleList> example_pool;
        std::vector<std::vector<std::string>> cared_vars;
        std::vector<std::string> global_name;
        DataList current_global;
        IncreFullContext ctx;
        IncreExampleCollectionEvaluator* eval;
        std::unordered_map<std::string, EnvAddress*> global_address_map;

        IncreExampleCollector(IncreProgramData* program, const std::vector<std::vector<std::string>>& cared_vars,
                              const std::vector<std::string>& _global_name);
        void add(int rewrite_id, const DataList& local_inp, const Data& oup);
        virtual void collect(const syntax::Term& start, const DataList& global);
        void clear();
        virtual ~IncreExampleCollector();
    };

    class IncreExamplePool {
    private:
        IncreProgram program;
        std::vector<std::vector<std::string>> cared_vars;
        IncreDataGenerator* generator;
        std::vector<bool> is_finished;
        int thread_num;

        std::vector<std::pair<std::string, syntax::TyList>> start_list;
        std::vector<std::unordered_set<std::string>> existing_example_set;
    public:
        std::vector<std::string> global_name_list;
        syntax::TyList global_type_list;
        std::vector<IncreExampleList> example_pool;

        std::pair<syntax::Term, DataList> generateStart();
        IncreExamplePool(const IncreProgram& _program, const std::vector<std::vector<std::string>>& _cared_vars, IncreDataGenerator* _g);
        ~IncreExamplePool();
        void merge(int rewrite_id, IncreExampleCollector* collector, TimeGuard* guard);
        void generateSingleExample();
        void generateBatchedExample(int rewrite_id, int target_num, TimeGuard* guard);
    };
}

#endif //ISTOOL_INCRE_INSTRU_RUNTIME_H
