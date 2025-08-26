//
// Created by pro on 2023/12/16.
//

#include "istool/incre/grammar/incre_grammar_builder.h"
#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "istool/ext/deepcoder/data_type.h"
#include "istool/incre/trans/incre_trans.h"

using namespace incre;
using namespace incre::grammar;
using namespace incre::syntax;


SymbolInfo::SymbolInfo(const SymbolContext &_context, const PType &_type, NonTerminal *_symbol):
        context(_context), type(_type), symbol(_symbol) {
}

GrammarBuilder::GrammarBuilder(const SymbolContext &init_context): contexts({init_context}) {
}

std::vector<SymbolInfo> GrammarBuilder::getSymbols(const std::function<bool(const SymbolInfo &)> &filter) const {
    std::vector<SymbolInfo> res;
    for (auto& info: info_list) {
        if (filter(info)) res.push_back(info);
    }
    return res;
}

namespace {
    std::string _context2String(const SymbolContext& context) {
        std::vector<std::string> name_list;
        for (auto& sem: context) {
            name_list.push_back(sem->getName());
        }
        std::sort(name_list.begin(), name_list.end());
        std::string res = "[";
        for (int i = 0; i < name_list.size(); ++i) {
            if (i) res += ",";
            res += name_list[i];
        }
        return res + "]";
    }
}

std::vector<SymbolInfo> GrammarBuilder::getSymbols(const PType &type) const {
    auto filter = [type](const SymbolInfo& info) {
        return type->getName() == info.type->getName();
    };
    auto res = getSymbols(filter);
    return res;
}

NonTerminal *GrammarBuilder::getSymbol(const SymbolContext &context, const PType &type) const {
    auto feature = _context2String(context) + "@" + type->getName();
    auto it = info_map.find(feature);
    if (it == info_map.end()) return nullptr;
    return info_list[it->second].symbol;
}

void GrammarBuilder::insertContext(const SymbolContext &context) {
    auto feature = _context2String(context);
    for (auto& existing: contexts) {
        if (_context2String(existing) == feature) return;
    }
    contexts.push_back(context);
}

void GrammarBuilder::insertInfo(const SymbolContext &context, const PType &type) {
    auto feature = _context2String(context) + "@" + type->getName();
    if (info_map.find(feature) == info_map.end()) {
        auto* symbol = new NonTerminal(feature, type);
        info_map[feature] = info_list.size();
        info_list.emplace_back(context, type, symbol);
    }
}

void GrammarBuilder::insertTypeForAllContext(const PType &type) {
    for (auto& context: contexts) {
        insertInfo(context, type);
    }
}


namespace {
    Grammar* _buildGrammar(const TypeList& inp_list, const SynthesisComponentList& component_list, const std::function<bool(Type*)>& is_oup, const PType& single_oup) {
        SymbolContext init_context;
        for (int i = 0; i < inp_list.size(); ++i) {
            init_context.push_back(::semantics::buildParamSemantics(i, inp_list[i]));
        }
        GrammarBuilder builder(init_context);

        while (true) {
            int pre_size = builder.contexts.size();
            for (auto& component: component_list) component->extendContext(builder);
            if (builder.contexts.size() == pre_size) break;
        }

        for (auto& context: builder.contexts) {
            for (auto& sem: context) {
                auto* ts = dynamic_cast<TypedSemantics*>(sem.get());
                assert(ts);
                builder.insertInfo(context, ts->oup_type);
                auto* symbol = builder.getSymbol(context, ts->oup_type);
                symbol->rule_list.push_back(new ConcreteRule(sem, {}));
            }
        }

        int pre_size;
        if (single_oup) builder.insertInfo(init_context, single_oup);
        do {
            pre_size = builder.info_list.size();
            for (auto& component: component_list) {
                component->extendNTMap(builder);
            }
        } while (pre_size < builder.info_list.size());

        for (auto& component: component_list) {
            component->insertComponent(builder);
        }

        auto init_symbols = builder.getSymbols([&](const SymbolInfo& info) {
            return _context2String(info.context) == _context2String(init_context);
        });
        NTList start_list, symbol_list;
        for (auto& info: builder.info_list) symbol_list.push_back(info.symbol);
        for (auto& info: init_symbols) {
            if (is_oup(info.type.get())) start_list.push_back(info.symbol);
        }
        if (start_list.empty()) {
            auto* dummy_symbol = new NonTerminal("start", type::getTBool());
            return new Grammar(dummy_symbol, {dummy_symbol});
        }
        if (single_oup) {
            assert(start_list.size() == 1);
            auto* grammar = new Grammar(start_list[0], symbol_list, true);
            return grammar;
        }
        auto* start_symbol = new NonTerminal("start", type::getTVarA());
        symbol_list.push_back(start_symbol);
        for (auto* possible: start_list) {
            auto sem = std::make_shared<incre::semantics::TypeLabeledDirectSemantics>(possible->type);
            start_symbol->rule_list.push_back(new ConcreteRule(sem, {possible}));
        }
        auto* grammar = new Grammar(start_symbol, symbol_list, true);
        return grammar;
    }
    bool _isPrimaryType(Type* type) {
        return dynamic_cast<TBool*>(type) || dynamic_cast<TInt*>(type);
    }
    bool _isCompressType(Type* type) {
        return dynamic_cast<trans::TCompress*>(type);
    }
    bool _isNonFunctionalType(Type* type) {
        if (dynamic_cast<TArrow*>(type)) return false;
        auto* tt = dynamic_cast<TProduct*>(type);
        if (tt) {
            for (auto& sub_type: tt->sub_types) {
                if (_isNonFunctionalType(sub_type.get())) return false;
            }
        }
        return true;
    }
    bool _isGeneralCompressType(Type* type) {
        return _isNonFunctionalType(type) && !dynamic_cast<TProduct*>(type);
    }
    bool _isCompressOrPrimaryType(Type* type) {
        return _isPrimaryType(type) || _isCompressType(type);
    }
}


Grammar *ComponentPool::buildCompGrammar(const TypeList &inp_list, bool is_only_prime) {
    if (is_only_prime) {
        return _buildGrammar(inp_list, comp_list, _isPrimaryType, nullptr);
    } else {
        return _buildGrammar(inp_list, comp_list, _isNonFunctionalType, nullptr);
    }
}

Grammar *ComponentPool::buildExtractGrammar(const TypeList &inp_list, int command_id) {
    SynthesisComponentList component_list;
    for (auto& component: extract_list) {
        if (component->command_id < command_id) {
            component_list.push_back(component);
        }
    }
    return _buildGrammar(inp_list, component_list, _isCompressOrPrimaryType, nullptr);
}

Grammar *ComponentPool::buildCombGrammar(const TypeList &inp_list, const PType &oup_type, int command_id) {
    SynthesisComponentList component_list;
    for (auto& component: comb_list) {
        if (component->command_id < command_id) {
            component_list.push_back(component);
        }
    }
    return _buildGrammar(inp_list, component_list, [&](Type* type){return type::equal(type, oup_type.get());}, oup_type);
}


Grammar *incre::grammar::builder::buildGrammar(const TypeList &inp_list, const SynthesisComponentList &component_list,
                                               const PType &oup) {
    auto func = [&](Type* type) {return type::equal(type, oup.get());};
    return _buildGrammar(inp_list, component_list, func, oup);
}
