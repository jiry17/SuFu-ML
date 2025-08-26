//
// Created by pro on 2023/4/5.
//

#include "istool/incre/grammar/incre_grammar_builder.h"
#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "istool/incre/trans/incre_trans.h"
#include "istool/sygus/theory/basic/clia/clia_semantics.h"
#include "istool/ext/deepcoder/data_type.h"
#include <iostream>
#include "glog/logging.h"

using namespace incre;
using namespace incre::grammar;
using namespace incre::syntax;

SynthesisComponent::SynthesisComponent(int _command_id, const std::string& _name): command_id(_command_id), name(_name){
}

ContextFreeSynthesisComponent::ContextFreeSynthesisComponent(int command_id, const std::string& _name): SynthesisComponent(command_id, _name) {
}
void ContextFreeSynthesisComponent::extendContext(GrammarBuilder &builder) {
    return;
}

IncreComponent::IncreComponent(const std::string &_name, const PType &_type, const Data &_data,
                               int _command_id, bool _is_partial):
                               ContextFreeSynthesisComponent(_command_id, _name), data(_data),
                               is_partial(_is_partial) {
    res_type = _type;
    while (1) {
        auto* ta = dynamic_cast<TArrow*>(res_type.get());
        if (!ta) break; assert(ta->inp_types.size() == 1);
        param_types.push_back(ta->inp_types[0]);
        res_type = ta->oup_type;
    }
}

void IncreComponent::extendNTMap(GrammarBuilder &builder) {
    builder.insertTypeForAllContext(res_type);
    if (is_partial) {
        auto partial_output = res_type;
        for (int i = int(param_types.size()) - 1; i >= 0; --i) {
            partial_output = std::make_shared<TArrow>((TypeList){param_types[i]}, partial_output);
            builder.insertTypeForAllContext(partial_output);
        }
    } else {
        auto full_type = res_type;
        for (int i = int(param_types.size()) - 1; i >= 0; --i) {
            full_type = std::make_shared<TArrow>((TypeList){param_types[i]}, full_type);
        }
        builder.insertTypeForAllContext(full_type);
    }
}

void IncreComponent::insertComponent(const GrammarBuilder &builder) {
    auto sem = std::make_shared<incre::semantics::IncreOperatorSemantics>(name, data);
    {
        auto full_type = res_type;
        for (int i = int(param_types.size()) - 1; i >= 0; --i) {
            full_type = std::make_shared<TArrow>((TypeList) {param_types[i]}, full_type);
        }
        auto info_list = builder.getSymbols(full_type);
        for (auto& info: info_list) {
            info.symbol->rule_list.push_back(new ConcreteRule(sem, {}));
        }
    }
    if (is_partial) {
        for (int i = 0; i < param_types.size(); ++i) {
            auto partial_res_type = res_type;
            for (int j = int(param_types.size()) - 1; j > i; --j) {
                partial_res_type = std::make_shared<TArrow>((TypeList){param_types[j]}, partial_res_type);
            }
            auto info_list = builder.getSymbols(partial_res_type);
            for (auto& info: info_list) {
                NTList param_list; bool is_valid = true;
                for (int j = 0; j <= i; ++j) {
                    auto* symbol = builder.getSymbol(info.context, param_types[j]);
                    if (!symbol) {
                        is_valid = false; break;
                    }
                    param_list.push_back(symbol);
                }
                if (is_valid) {
                    info.symbol->rule_list.push_back(new ConcreteRule(sem, param_list));
                }
            }
        }
    } else {
        if (param_types.empty()) return;
        auto info_list = builder.getSymbols(res_type);
        for (auto& info: info_list) {
            NTList param_list; bool is_valid = true;
            for (auto &param_type: param_types) {
                auto* param_symbol = builder.getSymbol(info.context, param_type);
                if (!param_symbol) {
                    is_valid = false; break;
                }
                param_list.push_back(param_symbol);
            }
            if (is_valid) {
                assert(param_list.size() == param_types.size());
                info.symbol->rule_list.push_back(new ConcreteRule(sem, param_list));
            }
        }
    }
}
Term IncreComponent::tryBuildTerm(const PSemantics &sem, const TermList &term_list) {
    auto* os = dynamic_cast<incre::semantics::IncreOperatorSemantics*>(sem.get());
    if (!os || sem->getName() != name) return nullptr;
    Term res = std::make_shared<TmVar>(name);
    for (auto& param: term_list) res = std::make_shared<TmApp>(res, param);
    return res;
}

ConstComponent::ConstComponent(const PType &_type, const DataList &_const_list,
                               const std::function<bool(Value *)> &_is_inside):
                               type(_type), const_list(_const_list), is_inside(_is_inside), ContextFreeSynthesisComponent(-1, _type->getName()) {
}

void ConstComponent::extendNTMap(GrammarBuilder &builder) {
    builder.insertTypeForAllContext(type);
}

void ConstComponent::insertComponent(const GrammarBuilder &builder) {
    for (auto& data: const_list) {
        auto sem = ::semantics::buildConstSemantics(data);
        auto info_list = builder.getSymbols(type);
        for (auto& info: info_list) {
            info.symbol->rule_list.push_back(new ConcreteRule(sem, {}));
        }
    }
}
Term ConstComponent::tryBuildTerm(const PSemantics &sem, const TermList &term_list) {
    auto* cs = dynamic_cast<ConstSemantics*>(sem.get());
    if (!cs || !is_inside(cs->w.get())) return nullptr;
    return std::make_shared<TmValue>(cs->w);
}

BasicOperatorComponent::BasicOperatorComponent(const std::string &_name, const PSemantics &__sem):
    ContextFreeSynthesisComponent(-1, _name), _sem(__sem){
    sem = dynamic_cast<TypedSemantics*>(_sem.get()); assert(sem);
}

namespace {
    PType _replace(const PType& type) {
        if (dynamic_cast<TVar*>(type.get())) return theory::clia::getTInt();
        return type;
    }
}

void BasicOperatorComponent::extendNTMap(GrammarBuilder &builder) {
    builder.insertTypeForAllContext(_replace(sem->oup_type));
}

void BasicOperatorComponent::insertComponent(const GrammarBuilder &builder) {
    auto info_list = builder.getSymbols(_replace(sem->oup_type));
    for (auto& info: info_list) {
        NTList param_list; bool is_valid = true;
        for (auto& inp_type: sem->inp_type_list) {
            auto* symbol = builder.getSymbol(info.context, _replace(inp_type));
            if (!symbol) {
                is_valid = false; break;
            }
            param_list.push_back(symbol);
        }
        info.symbol->rule_list.push_back(new ConcreteRule(_sem, param_list));
    }
}

Term BasicOperatorComponent::tryBuildTerm(const PSemantics &current_sem, const TermList &term_list) {
    if (current_sem->getName() != name && current_sem->getName() != _sem->getName()) return nullptr;
    auto op_name = trans::operatorNameToIncre(name);
    return std::make_shared<TmPrimary>(op_name, term_list);
}

IteComponent::IteComponent(): ContextFreeSynthesisComponent(-1, "ite") {
}

void IteComponent::extendNTMap(GrammarBuilder &builder) {
}
void IteComponent::insertComponent(const GrammarBuilder& builder) {
    auto ti = theory::clia::getTInt(), tb = type::getTBool();
    auto info_list = builder.getSymbols(ti);
    for (auto& info: info_list) {
        auto* cond = builder.getSymbol(info.context, tb);
        if (cond) info.symbol->rule_list.push_back(new ConcreteRule(std::make_shared<IteSemantics>(), {cond, info.symbol, info.symbol}));
    }
}
Term IteComponent::tryBuildTerm(const PSemantics& sem, const TermList &term_list) {
    if (!dynamic_cast<IteSemantics*>(sem.get())) return nullptr;
    assert(term_list.size() == 3);
    return std::make_shared<TmIf>(term_list[0], term_list[1], term_list[2]);
}

#include "istool/ext/deepcoder/deepcoder_semantics.h"
#include "istool/incre/trans/incre_trans.h"

TupleComponent::TupleComponent(): ContextFreeSynthesisComponent( -1, "prod") {
}

void TupleComponent::extendNTMap(GrammarBuilder &builder) {
    for (int i = 0; i < builder.info_list.size(); ++i) {
        auto* tp = dynamic_cast<TProduct*>(builder.info_list[i].type.get());
        if (tp) {
            for (auto& sub_type: tp->sub_types) {
                builder.insertInfo(builder.info_list[i].context, sub_type);
            }
        }
    }
}
void TupleComponent::insertComponent(const GrammarBuilder &builder) {
    auto sem = std::make_shared<ProductSemantics>();
    for (auto& info: builder.info_list) {
        auto* tp = dynamic_cast<TProduct*>(info.type.get());
        if (!tp) continue;
        NTList param_list;
        for (auto& sub_type: tp->sub_types) {
            param_list.push_back(builder.getSymbol(info.context, sub_type));
        }
        bool is_valid = true;
        for (auto* param: param_list) {
            if (!param) is_valid = false;
        }
        if (is_valid) info.symbol->rule_list.push_back(new ConcreteRule(sem, param_list));
    }
}
Term TupleComponent::tryBuildTerm(const PSemantics& sem, const TermList &term_list) {
    if (!dynamic_cast<ProductSemantics*>(sem.get())) return nullptr;
    return std::make_shared<TmTuple>(term_list);
}

ProjComponent::ProjComponent(int _size): ContextFreeSynthesisComponent(-1, "proj"), size(_size) {
}

void ProjComponent::extendNTMap(GrammarBuilder &builder) {
    for (int i = 0; i < builder.info_list.size(); ++i) {
        auto* tp = dynamic_cast<TProduct*>(builder.info_list[i].type.get());
        if (tp && tp->sub_types.size() == size) {
            for (auto& sub_type: tp->sub_types) {
                builder.insertInfo(builder.info_list[i].context, sub_type);
            }
        }
    }
}
void ProjComponent::insertComponent(const GrammarBuilder &builder) {
    for (auto& info: builder.info_list) {
        auto* tp = dynamic_cast<TProduct*>(info.type.get());
        if (!tp) continue;
        for (int i = 0; i < tp->sub_types.size(); ++i) {
            auto sem = std::make_shared<AccessSemantics>(i);
            auto* target = builder.getSymbol(info.context, tp->sub_types[i]);
            if (target) target->rule_list.push_back(new ConcreteRule(sem, {info.symbol}));
        }
    }
}
Term ProjComponent::tryBuildTerm(const PSemantics &sem, const TermList &term_list) {
    auto* as = dynamic_cast<AccessSemantics*>(sem.get());
    if (!as) return nullptr;
    assert(term_list.size() == 1);
    return std::make_shared<TmProj>(term_list[0], as->id + 1, size);
}

ComponentPool::ComponentPool() {
}

void ComponentPool::print() const {
    std::vector<std::pair<std::string, SynthesisComponentList>> all_components = {
            {"extract", extract_list}, {"compress", comp_list}, {"comb", comb_list}
    };
    for (auto& [name, comp_list]: all_components) {
        std::cout << "Components for " << name << ":" << std::endl;
        for (auto& comp: comp_list) {
            auto* uc = dynamic_cast<IncreComponent*>(comp.get());
            if (uc) std::cout << "  " << uc->name << " " << type::typeList2String(uc->param_types) << " -> " << uc->res_type->getName() << " " << uc->command_id << std::endl;
            auto* bc = dynamic_cast<BasicOperatorComponent*>(comp.get());
            if (bc) std::cout << "  " << bc->_sem->getName() << " " << type::typeList2String(bc->sem->inp_type_list) << " " << bc->sem->oup_type->getName() << std::endl;
        }
        std::cout << std::endl;
    }
}

void ComponentPool::add(const PSynthesisComponent &component, GrammarType type) {
    switch (type) {
        case GrammarType::COMBINE: {
            comb_list.push_back(component); break;
        }
        case GrammarType::COMPRESS: {
            comp_list.push_back(component); break;
        }
        case GrammarType::EXTRACT: {
            extract_list.push_back(component); break;
        }
    }
}