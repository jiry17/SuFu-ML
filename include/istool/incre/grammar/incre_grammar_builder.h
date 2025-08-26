//
// Created by pro on 2023/4/5.
//

#ifndef ISTOOL_INCRE_GRAMMAR_BUILDER_H
#define ISTOOL_INCRE_GRAMMAR_BUILDER_H

#include "istool/basic/grammar.h"
#include "istool/incre/language/incre_syntax.h"
#include "istool/incre/language/incre_program.h"

namespace incre::grammar {
    typedef std::vector<PSemantics> SymbolContext;


    class SymbolInfo {
    public:
        SymbolContext context;
        PType type;
        NonTerminal* symbol;
        SymbolInfo(const SymbolContext& _context, const PType& _type, NonTerminal* _symbol);
        ~SymbolInfo() = default;
    };

    class GrammarBuilder {
    public:
        std::unordered_map<std::string, int> info_map;
        std::vector<SymbolInfo> info_list;
        std::vector<SymbolContext> contexts;
        void insertContext(const SymbolContext& context);
        void insertInfo(const SymbolContext& context, const PType& type);
        void insertTypeForAllContext(const PType& type);
        std::vector<SymbolInfo> getSymbols(const std::function<bool(const SymbolInfo&)>& filter) const;
        std::vector<SymbolInfo> getSymbols(const PType& type) const;
        NonTerminal* getSymbol(const SymbolContext& context, const PType& type) const;
        GrammarBuilder(const SymbolContext& init_context);
    };

    class SynthesisComponent {
    public:
        int command_id;
        std::string name;
        SynthesisComponent(int command_id, const std::string& _name);
        virtual void extendContext(GrammarBuilder& builder) = 0;
        virtual void insertComponent(const GrammarBuilder& builder) = 0;
        virtual void extendNTMap(GrammarBuilder& builder) = 0;
        virtual syntax::Term tryBuildTerm(const PSemantics& sem, const syntax::TermList& term_list) = 0;
        virtual ~SynthesisComponent() = default;
    };
    typedef std::shared_ptr<SynthesisComponent> PSynthesisComponent;
    typedef std::vector<PSynthesisComponent> SynthesisComponentList;

    class ContextFreeSynthesisComponent: public SynthesisComponent {
    public:
        ContextFreeSynthesisComponent(int command_id, const std::string& _name);
        virtual void extendContext(GrammarBuilder& builder);
        virtual ~ContextFreeSynthesisComponent() = default;
    };

    class IncreComponent: public ContextFreeSynthesisComponent {
    public:
        TypeList param_types;
        PType res_type;
        Data data;
        bool is_partial;
        IncreComponent(const std::string& _name, const PType& _type, const Data& _data, int command_id, bool _is_partial);
        virtual void insertComponent(const GrammarBuilder& symbol_map);
        virtual void extendNTMap(GrammarBuilder& symbol_map);
        virtual syntax::Term tryBuildTerm(const PSemantics& sem, const syntax::TermList& term_list);
        ~IncreComponent() = default;
    };

    class ConstComponent: public ContextFreeSynthesisComponent {
    public:
        PType type;
        DataList const_list;
        std::function<bool(Value*)> is_inside;
        ConstComponent(const PType& _type, const DataList& _const_list, const std::function<bool(Value*)>& _is_inside);
        virtual void insertComponent(const GrammarBuilder& symbol_map);
        virtual void extendNTMap(GrammarBuilder& symbol_map);
        virtual syntax::Term tryBuildTerm(const PSemantics& sem, const syntax::TermList& term_list);
        ~ConstComponent() = default;
    };

    class BasicOperatorComponent: public ContextFreeSynthesisComponent {
    public:
        TypedSemantics* sem;
        PSemantics _sem;
        BasicOperatorComponent(const std::string& _name, const PSemantics& __semantics);
        virtual syntax::Term tryBuildTerm(const PSemantics& sem, const syntax::TermList& term_list);
        virtual void insertComponent(const GrammarBuilder& symbol_map);
        virtual void extendNTMap(GrammarBuilder& symbol_map);
        ~BasicOperatorComponent() = default;
    };

#define LanguageComponentBody(name) \
        virtual void insertComponent(const GrammarBuilder& builder); \
        virtual void extendNTMap(GrammarBuilder& builder); \
        virtual syntax::Term tryBuildTerm(const PSemantics& sem, const syntax::TermList& term_list); \
        ~name ## Component() = default;
#define LanguageComponent(name)     \
    class name ## Component: public ContextFreeSynthesisComponent { \
    public:                         \
          name ## Component ();                           \
          LanguageComponentBody(name);                              \
          } \

    LanguageComponent(Ite);
    LanguageComponent(Tuple);

    class ProjComponent: public ContextFreeSynthesisComponent {
    public:
        int size;
        ProjComponent(int _size);
        LanguageComponentBody(Proj);
    };

    enum class GrammarType {
        EXTRACT, COMPRESS, COMBINE
    };

    class ComponentPool {
    public:
        SynthesisComponentList extract_list, comp_list, comb_list;
        void add(const PSynthesisComponent& component, GrammarType usable_types);
        ComponentPool();
        void print() const;

        Grammar* buildCompGrammar(const TypeList& inp_list, bool is_only_prime = true);
        Grammar* buildExtractGrammar(const TypeList& inp_list, int command_id);
        Grammar* buildCombGrammar(const TypeList& inp_list, const PType& oup_type, int command_id);

        ~ComponentPool() = default;
    };
    namespace builder {
        Grammar *buildGrammar(const TypeList &inp_list, const SynthesisComponentList &component_list, const PType& oup);
    }

    namespace collector {
        ComponentPool getBasicComponentPool(Env* env);
        ComponentPool collectComponent(Env* env, IncreProgramData* program);
    }
}

#endif //ISTOOL_INCRE_GRAMMAR_BUILDER_H
