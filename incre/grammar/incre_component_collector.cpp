//
// Created by pro on 2023/12/16.
//

#include "istool/incre/grammar/incre_grammar_builder.h"
#include "istool/incre/analysis/incre_instru_types.h"
#include "istool/incre/trans/incre_trans.h"
#include "glog/logging.h"

using namespace incre::grammar;
using namespace incre::syntax;
using namespace incre::semantics;
using namespace incre::types;

#define RegisterComponent(type, comp) basic.add(comp, GrammarType::type)
#define RegisterAll(component) RegisterComponent(EXTRACT, component), RegisterComponent(COMPRESS, component), RegisterComponent(COMBINE, component)

ComponentPool incre::grammar::collector::getBasicComponentPool(Env* env) {
    ComponentPool basic;
    // insert basic operator
    std::vector<std::string> op_list = {"+", "-", "=", "<", "<=", "and", "or", "!"};

    const std::unordered_set<std::string> all_used_op = {"+", "-"};

    for (auto& op_name: op_list) {
        auto sem = env->getSemantics(op_name);
        auto comp = std::make_shared<BasicOperatorComponent>(op_name, sem);
        if (all_used_op.find(op_name) != all_used_op.end()) RegisterAll(comp); else RegisterComponent(COMBINE, comp);
    }

    {
        auto time_sem = env->getSemantics("*");
        PSynthesisComponent comp = std::make_shared<BasicOperatorComponent>("*", time_sem);
        auto* is_use_time = env->getConstRef(incre::config::KIsNonLinearName, BuildData(Bool, false));
        if (!is_use_time->isTrue()) comp->command_id = 1e9;
        RegisterComponent(COMBINE, comp);
    }

    // insert const operator
    auto ic_align = std::make_shared<ConstComponent>(theory::clia::getTInt(), (DataList){BuildData(Int, 0)},
                                                     [](Value* value)->bool {return dynamic_cast<IntValue*>(value);});
    RegisterComponent(COMPRESS, ic_align);
    auto ic = std::make_shared<ConstComponent>(theory::clia::getTInt(), (DataList){BuildData(Int, 0), BuildData(Int, 1)},
                                               [](Value* value)->bool {return dynamic_cast<IntValue*>(value);});
    RegisterComponent(EXTRACT, ic);
    RegisterComponent(COMBINE, ic);
    auto ib = std::make_shared<ConstComponent>(type::getTBool(), (DataList){},
                                               [](Value* value) -> bool {return dynamic_cast<BoolValue*>(value);});
    RegisterAll(ib);

    // insert language constructs
    RegisterAll(std::make_shared<IteComponent>());
    for (int i = 2; i <= 4; ++i) {
        RegisterAll(std::make_shared<ProjComponent>(i));
    }
    RegisterAll(std::make_shared<TupleComponent>());
    return basic;
}

namespace {
    typedef std::tuple<int, std::string, Data, Ty, incre::CommandData*> ComponentInfo;

    class _ComponentCollectWalker: public incre::DefaultContextBuilder {
    protected:
        std::vector<ComponentInfo> component_info_list;
        std::unordered_map<std::string, int> name_last_id_map;
        int command_id = 0;
        virtual void postProcess(incre::CommandData* command) {
            command_id += 1;
        }
        virtual void visit(incre::CommandBindTerm* command) {
            incre::DefaultContextBuilder::visit(command);
            name_last_id_map[command->name] = command_id;
            auto address = ctx.start.get();
            component_info_list.emplace_back(command_id, command->name, address->bind.getData(), address->bind.getType(), command);
        }
        virtual void visit(incre::CommandDef* command) {
            incre::DefaultContextBuilder::visit(command);
            for (auto& [name, _]: command->cons_list) {
                name_last_id_map[name] = command_id;
                auto* address = ctx.getAddress(name);
                auto cons_term = std::make_shared<TmFunc>("x", std::make_shared<TmCons>(name, std::make_shared<TmVar>("x")));
                auto data = evaluator->evaluate(cons_term.get(), ctx);
                component_info_list.emplace_back(command_id, name, data, address->bind.getType(), command);
            }
        }
        virtual void visit(incre::CommandDeclare* command) {
            incre::DefaultContextBuilder::visit(command);
            name_last_id_map[command->name] = command_id;
        }
    public:
        _ComponentCollectWalker(): incre::DefaultContextBuilder(new IncreLabeledEvaluator(),
                                                                new IncreLabeledTypeChecker()) {
        }
        ~_ComponentCollectWalker() {
            delete evaluator; delete checker;
        }
        std::vector<ComponentInfo> getComponentInfo() {
            auto* rewriter = new incre::syntax::IncreLabeledTypeRewriter();
            for (auto& [id, name, v, type, info]: component_info_list) {
                type = rewriter->rewrite(type);
            }
            delete rewriter;
            return component_info_list;
        }
    };

#define DetectorCase(name) case TermType::TERM_TOKEN_ ## name: return _detectTerm(dynamic_cast<Tm ## name*>(term));
#define DetectorHead(name) bool _detectTerm(Tm ## name* term)
#define DetectorDefaultCase(name) DetectorHead(name) {\
    auto sub_list = getSubTerms(term);                \
    for (auto& sub: sub_list) if (!detectTerm(sub.get())) return false; \
    return true; \
}

    bool _getInfo(const std::string& name, const std::vector<std::pair<std::string, bool>>& var_infos) {
        for (int i = int(var_infos.size()) - 1; i >= 0; --i) {
            if (var_infos[i].first == name) return var_infos[i].second;
        }
        LOG(FATAL) << "Unknown variable " << name;
    }


    class _ValidTermDetectorWalker: public incre::IncreProgramWalker {
    public:
        bool is_allow_rec;
        std::vector<std::pair<std::string, bool>> var_infos;
        bool isValidComponent(const std::string& name) {
            return _getInfo(name, var_infos);
        }
        _ValidTermDetectorWalker(bool _is_allow_rec): is_allow_rec(_is_allow_rec) {}
    protected:
        virtual void visit(incre::CommandDef* command) {
            for (auto& [name, ty]: command->cons_list) {
                var_infos.emplace_back(name, true);
            }
        }
        virtual void visit(incre::CommandBindTerm* command) {
            bool info;
            if (command->is_rec) {
                var_infos.emplace_back(command->name, false);
                info = detectTerm(command->term.get());
                var_infos.pop_back();
            } else info = detectTerm(command->term.get());
            var_infos.emplace_back(command->name, info);
        }
        virtual void visit(incre::CommandEval* command) {
            return;
        }
        virtual void visit(incre::CommandDeclare* command) {
            var_infos.emplace_back(command->name, false);
        }

        DetectorDefaultCase(If); DetectorDefaultCase(Tuple); DetectorDefaultCase(Proj);
        DetectorDefaultCase(Primary); DetectorDefaultCase(App); DetectorDefaultCase(Value);
        DetectorDefaultCase(Label); DetectorDefaultCase(Unlabel); DetectorDefaultCase(Cons);

        DetectorHead(Var) {
            return _getInfo(term->name, var_infos);
        }

        DetectorHead(Let) {
            bool res = true;
            if (term->is_rec) {
                var_infos.emplace_back(term->name, is_allow_rec);
                res &= detectTerm(term->def.get());
            } else {
                res &= detectTerm(term->def.get());
                var_infos.emplace_back(term->name, true);
            }
            res &= detectTerm(term->body.get());
            var_infos.pop_back();
            return res;
        }

        DetectorHead(Match) {
            if (!detectTerm(term->def.get())) return false;
            int init_size = int(var_infos.size()); bool res = true;
            for (auto& [pt, case_term]: term->cases) {
                auto new_vars = incre::syntax::getVarsInPattern(pt.get());
                for (auto& name: new_vars) var_infos.emplace_back(name, true);
                res &= detectTerm(case_term.get());
                var_infos.resize(init_size);
            }
            return res;
        }

        DetectorHead(Rewrite) {return false;}

        DetectorHead(Func) {
            var_infos.emplace_back(term->name, true);
            auto res = detectTerm(term->body.get());
            var_infos.pop_back(); return res;
        }

        bool detectTerm(TermData* term) {
            switch (term->getType()) {
                TERM_CASE_ANALYSIS(DetectorCase);
            }
        }
    };

    bool _isBasicType(TypeData* type);

#define BasicTypeCase(name) case TypeType::TYPE_TOKEN_ ## name: return __isBasicType(dynamic_cast<Ty ## name*>(type));
#define BasicTypeHead(name) bool __isBasicType(Ty ## name* type)
#define DefaultBasicTypeCase(name, value) BasicTypeHead(name) {return value;}

    DefaultBasicTypeCase(Var, false); DefaultBasicTypeCase(Arr, false); DefaultBasicTypeCase(Poly, false);
    DefaultBasicTypeCase(Int, true); DefaultBasicTypeCase(Bool, true); DefaultBasicTypeCase(Unit, true);
    // DefaultBasicTypeCase(Tuple, false);
    DefaultBasicTypeCase(Compress, false);
    BasicTypeHead(Ind) {
        for (auto& param: type->param_list) if (!_isBasicType(param.get())) return false;
        return true;
    }
    BasicTypeHead(Tuple) {
        for (auto& param: type->fields) if (!_isBasicType(param.get())) return false;
        return true;
    }


    bool _isBasicType(TypeData* type) {
        switch (type->getType()) {
            TYPE_CASE_ANALYSIS(BasicTypeCase);
        }
    }

    bool _isUnboundedType(TypeData* type) {
        if (type->getType() == TypeType::VAR) {
            auto* tv = dynamic_cast<TyVar*>(type);
            return !tv->is_bounded();
        }
        for (auto& sub: incre::syntax::getSubTypes(type)) {
            if (_isUnboundedType(sub.get())) return true;
        }
        return false;
    }

    void _collectBasicType(const Ty& type, TyList& basic_types) {
        if (_isBasicType(type.get())) basic_types.push_back(type);
        for (auto& sub_type: getSubTypes(type.get())) {
            _collectBasicType(sub_type, basic_types);
        }
    }

    void _selectAmong(int pos, const TyList& pool, TyList& tmp, std::vector<TyList>& plan_list) {
        if (pos == tmp.size()) {
            plan_list.push_back(tmp); return;
        }
        for (auto& choice: pool) {
            tmp[pos] = choice;
            _selectAmong(pos + 1, pool, tmp, plan_list);
        }
    }

    class _TypeGrounder: public IncreTypeRewriter {
    public:
        std::unordered_map<int, Ty> var_map;
        void set(const std::vector<int>& var_id, const TyList& types) {
            var_map.size();
            for (int i = 0; i < var_id.size(); ++i) {
                var_map[var_id[i]] = types[i];
            }
        }
    protected:
        virtual Ty _rewrite(TyVar* type, const Ty& _type) {
            if (type->is_bounded()) {
                LOG(FATAL) << "Bounded vars should be removed before grounding";
            }
            auto [id, _index, _range] = type->get_var_info();
            auto it = var_map.find(id);
            if (it == var_map.end()) {
                LOG(FATAL) << "Unknown variable " << type->toString();
            }
            return it->second;
        }
    };

    TyList _groundTypes(const Ty& type, const TyList& basic_types) {
        if (type->getType() != TypeType::POLY) return {type};
        auto* pt = dynamic_cast<TyPoly*>(type.get());

        TyList tmp(pt->var_list.size());
        std::vector<TyList> plan_list;
        _selectAmong(0, basic_types, tmp, plan_list);
        auto* grounder = new _TypeGrounder();
        TyList res;
        for (auto& ground_plan: plan_list) {
            grounder->set(pt->var_list, ground_plan);
            res.push_back(grounder->rewrite(pt->body));
        }
        delete grounder;
        return res;
    }
}

#include <iostream>
ComponentPool incre::grammar::collector::collectComponent(Env *env, IncreProgramData *program) {
    ComponentPool pool = incre::grammar::collector::getBasicComponentPool(env);
    std::vector<ComponentInfo> component_infos;
    {
        auto* collector = new _ComponentCollectWalker();
        collector->walkThrough(program);
        component_infos = collector->getComponentInfo();
        delete collector;
    }

    std::vector<Ty> basic_types;
    {
        for (auto& component: component_infos) {
            auto type = std::get<3>(component);
            _collectBasicType(type, basic_types);
        }
        std::unordered_set<std::string> type_set;
        int now = 0;
        for (auto& basic_type: basic_types) {
            auto name = basic_type->toString();
            if (type_set.find(name) == type_set.end()) {
                basic_types[now++] = basic_type;
                type_set.insert(name);
            }
        }
        basic_types.resize(now);
    }

    auto* rec_d = env->getConstRef(config::KSlowCombineName);
    auto* detector = new _ValidTermDetectorWalker(rec_d->isTrue());
    detector->walkThrough(program);

    for (auto& [command_id, name, value, type, command]: component_infos) {
        if (command->isDecrorateWith(CommandDecorate::SYN_EXCLUDE)) continue;

        std::vector<GrammarType> usable_grammars;
        if (command->isDecrorateWith(CommandDecorate::SYN_COMPRESS)) usable_grammars.push_back(GrammarType::COMPRESS);
        if (command->isDecrorateWith(CommandDecorate::SYN_COMBINE)) usable_grammars.push_back(GrammarType::COMBINE);
        if (command->isDecrorateWith(CommandDecorate::SYN_EXTRACT)) usable_grammars.push_back(GrammarType::EXTRACT);
        if (usable_grammars.empty()) {
            usable_grammars.push_back(GrammarType::COMPRESS);
            if (detector->isValidComponent(name)) {
                usable_grammars.push_back(GrammarType::EXTRACT);
                usable_grammars.push_back(GrammarType::COMBINE);
            }
        }

        TyList possible_types = _groundTypes(type, basic_types);
        LOG(INFO) << "possible types for " << name;
        for (auto& possible_type: possible_types) LOG(INFO) << "  " << possible_type->toString();
        SynthesisComponentList components;
        auto is_partial = command->isDecrorateWith(CommandDecorate::SYN_NO_PARTIAL);
        for (auto& incre_type: possible_types) {
            if (_isUnboundedType(incre_type.get())) continue;
            components.push_back(std::make_shared<IncreComponent>(name, incre::trans::typeFromIncre(incre_type.get()), value, command_id, is_partial));
        }

        for (auto grammar_type: usable_grammars) {
            for (auto& component: components) pool.add(component, grammar_type);
        }
    }

    delete detector;
    return pool;
}