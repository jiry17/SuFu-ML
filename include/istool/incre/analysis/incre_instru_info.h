//
// Created by pro on 2022/9/23.
//

#ifndef ISTOOL_INCRE_INFO_H
#define ISTOOL_INCRE_INFO_H

#include "istool/incre/language/incre_program.h"
#include "istool/incre/grammar/incre_grammar_builder.h"
#include "incre_instru_types.h"
#include "incre_instru_runtime.h"

namespace incre::analysis {
    class IncreInfoCollector: public types::IncreLabeledTypeChecker {
    protected:
        RegisterUnifyRule(Compress);
        RegisterTypingRule(Label);
        RegisterTypingRule(Unlabel);
        virtual syntax::Ty postProcess(syntax::TermData* term, const IncreContext& ctx, const syntax::Ty& res);
    public:
        std::unordered_map<syntax::TermData*, syntax::Ty> term_type_map;
        std::unordered_map<syntax::TermData*, IncreContext> term_context_map;
        std::vector<int> final_index;
        std::vector<int> father;
        int getNewCompressLabel();
        int getReprCompressLabel(int k);
        void unionCompressLabel(int x, int y);
        void constructFinalInfo();
        int getFinalCompressLabel(int k);
    };

    typedef std::vector<std::pair<std::string, syntax::Ty>> IncreInputInfo;

    class RewriteTypeInfo {
    public:
        int index;
        IncreInputInfo inp_types;
        syntax::Ty oup_type;
        int command_id;
        RewriteTypeInfo(int _index, const IncreInputInfo& _inp_types, const syntax::Ty& _oup_type, int _command_id);
    };

    class IncreInfoData {
    public:
        IncreProgram program;
        std::vector<RewriteTypeInfo> rewrite_info_list;
        example::IncreExamplePool* example_pool;
        grammar::ComponentPool component_pool;
        IncreInfoData(const IncreProgram& _program, const std::vector<RewriteTypeInfo>& _rewrite_info_list,
                      example::IncreExamplePool* example_pool, const grammar::ComponentPool& component_pool);
        ~IncreInfoData();
    };

    typedef std::shared_ptr<IncreInfoData> IncreInfo;

    IncreInfo buildIncreInfo(IncreProgramData* program, Env* env);

    namespace input_filter {
        bool isValidInputType(const syntax::Ty& type);
        IncreInputInfo buildInputInfo(const IncreContext& local_ctx, const IncreContext& global_ctx);
    }
}


#endif //ISTOOL_INCRE_INFO_H
