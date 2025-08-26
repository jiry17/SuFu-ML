//
// Created by pro on 2023/12/10.
//

#include "istool/incre/analysis/incre_instru_runtime.h"
#include "istool/incre/language/incre_program.h"
#include "istool/incre/language/incre_rewriter.h"
#include "istool/incre/analysis/incre_instru_types.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::example;
using namespace incre::syntax;

IncreExampleData::IncreExampleData(int _rewrite_id, const DataList &_local, const DataList &_global, const Data &_oup):
    rewrite_id(_rewrite_id), local_inputs(_local), global_inputs(_global), oup(_oup) {
}

std::string IncreExampleData::toString() const {
    std::string res = "#" + std::to_string(rewrite_id) + ": ";
    res += "local_inputs: " + data::dataList2String(local_inputs);
    res += ", global_inputs: " + data::dataList2String(global_inputs);
    res += " -> " + oup.toString();
    return res;
}

IncreDataGenerator::IncreDataGenerator(Env *_env, const std::unordered_map<std::string, CommandDef *> &_cons_map):
    env(_env), cons_map(_cons_map) {
    auto* data = env->getConstRef(incre::config::KDataSizeLimitName);
    KSizeLimit = theory::clia::getIntValue(*data);
    data = env->getConstRef(incre::config::KSampleIntMinName);
    KIntMin = theory::clia::getIntValue(*data);
    data = env->getConstRef(incre::config::KSampleIntMaxName);
    KIntMax = theory::clia::getIntValue(*data);
}

Data IncreDataGenerator::getRandomBool() {
    auto bool_dist = std::bernoulli_distribution(0.5);
    return BuildData(Bool, bool_dist(env->random_engine));
}

Data IncreDataGenerator::getRandomInt() {
    auto int_dist = std::uniform_int_distribution<int>(KIntMin, KIntMax);
    return BuildData(Int, int_dist(env->random_engine));
}

SizeSafeValueGenerator::SizeSafeValueGenerator(Env *_env,
                                               const std::unordered_map<std::string, CommandDef *> &_cons_map):
        IncreDataGenerator(_env, _cons_map) {
}

std::unordered_map<std::string, CommandDef *> incre::example::extractConsMap(IncreProgramData *program) {
    std::unordered_map<std::string, CommandDef*> res;
    for (auto& command: program->commands) {
        if (command->getType() == CommandType::DEF_IND) {
            auto* cd = dynamic_cast<CommandDef*>(command.get());
            res[cd->name] = cd;
        }
    }
    return res;
}

namespace {
    class _PolyBodyRewriter: public IncreTypeRewriter {
    public:
        std::unordered_map<int, syntax::Ty> substitute_map;

        _PolyBodyRewriter(const std::vector<int>& var_list, const TyList& param_list) {
            for (int i = 0; i < var_list.size(); ++i) {
                substitute_map[var_list[i]] = param_list[i];
            }
        }
    protected:
        virtual Ty _rewrite(TyVar* type, const Ty& _type) {
            if (type->is_bounded()) {
                return rewrite(type->get_bound_type());
            }
            auto [index, _level, _info] = type->get_var_info();
            auto it = substitute_map.find(index);
            if (it != substitute_map.end()) return it->second;
            return _type;
        }
    };
}

syntax::Ty incre::example::getContentTypeForGen(syntax::TypeData *_res, syntax::TypeData *_cons_ty) {
    if (_res->getType() != TypeType::IND) {
        LOG(FATAL) << "Unexpected inductive type " << _res->toString();
    }
    auto* res = dynamic_cast<TyInd*>(_res);
    TyArr* cons_ty; std::vector<int> var_list;
    if (_cons_ty->getType() == TypeType::POLY) {
        auto* pt = dynamic_cast<TyPoly*>(_cons_ty);
        if (pt->body->getType() != TypeType::ARR) {
            LOG(FATAL) << "Unexpected constructor type " << _cons_ty->toString();
        }
        cons_ty = dynamic_cast<TyArr*>(pt->body.get());
        var_list = pt->var_list;
    } else if (_cons_ty->getType() == TypeType::ARR) {
        cons_ty = dynamic_cast<TyArr*>(_cons_ty);
    } else LOG(FATAL) << "Unexpected constructor type " << _cons_ty->toString();

    if (res->param_list.size() != var_list.size()) {
        LOG(FATAL) << "Unmatched data structure and constructor: " << res->toString() << " and " << cons_ty->toString();
    }
    auto* rewriter = new _PolyBodyRewriter(var_list, res->param_list);
    return rewriter->rewrite(cons_ty->inp);
}

#define SizeGenCase(name) case TypeType::TYPE_TOKEN_ ## name: {\
    auto res = _getPossibleSplit(dynamic_cast<Ty ## name*>(type), size, this); \
    return split_map[feature] = res;}
#define SizeGenHead(name) SizeSplitList* _getPossibleSplit(Ty ## name* type, int size, SizeSafeValueGenerator* gen)
namespace {
#define SizeGenErrorCase(name) SizeGenHead(name) { \
    LOG(FATAL) << "Unexpected type in generation: " << type->toString();}

    SizeGenErrorCase(Poly); SizeGenErrorCase(Arr); SizeGenErrorCase(Var);

#define SizeGenEndCase(name) SizeGenHead(name) { \
    auto* split_list = new SizeSplitList; \
    if (size == 0) split_list->push_back(std::vector<int>()); \
    return split_list;}

    SizeGenEndCase(Int); SizeGenEndCase(Bool); SizeGenEndCase(Unit);

    void _getAllCombine(int now, int tot, const std::vector<std::vector<int>>& choice_list, std::vector<int>& tmp, SizeSplitList* scheme_list) {
        if (now == choice_list.size()) {
            if (tot == 0) scheme_list->push_back(tmp);
            return;
        }
        for (auto& i: choice_list[now]) {
            if (i <= tot) {
                tmp[now] = i; _getAllCombine(now + 1, tot - i, choice_list, tmp, scheme_list);
            }
        }
    }

    SizeGenHead(Tuple) {
        std::vector<std::vector<int>> choice_list;
        for (auto& sub_type: type->fields) {
            std::vector<int> choices;
            for (int i = 0; i <= size; ++i) {
                if (!gen->getPossibleSplit(sub_type.get(), i)->empty()) {
                    choices.push_back(i);
                }
            }
            choice_list.push_back(choices);
        }
        auto* scheme_list = new SizeSplitList();
        std::vector<int> tmp(choice_list.size());
        _getAllCombine(0, size, choice_list, tmp, scheme_list);
        return scheme_list;
    }

    SizeGenHead(Ind) {
        auto* scheme_list = new SizeSplitList();
        if (!size) return scheme_list;
        auto cons_it = gen->cons_map.find(type->name);
        if (cons_it == gen->cons_map.end()) {
            LOG(FATAL) << "Unknown ind type " << type->name;
        }
        for (auto& [cons_name, cons_type]: cons_it->second->cons_list) {
            auto param_type = getContentTypeForGen(type, cons_type.get());
            if (!gen->getPossibleSplit(param_type.get(), size - 1)->empty()) {
                scheme_list->push_back(std::make_pair(cons_name, param_type));
            }
        }
        return scheme_list;
    }

    SizeGenHead(Compress) {
        auto* scheme_list = new SizeSplitList();
        if (!gen->getPossibleSplit(type->body.get(), size)->empty()) {
            scheme_list->push_back(std::vector<int>(1, size));
        }
        return scheme_list;
    }
}

SizeSplitList *SizeSafeValueGenerator::getPossibleSplit(syntax::TypeData* type, int size) {
    auto feature = type->toString() + "@" + std::to_string(size);
    auto it = split_map.find(feature);
    if (it != split_map.end()) return it->second;

    switch (type->getType()) {
        TYPE_CASE_ANALYSIS(SizeGenCase);
    }
}

SizeSafeValueGenerator::~SizeSafeValueGenerator() noexcept {
    for (auto& [_, scheme_list]: split_map) delete scheme_list;
}

namespace {
    Data _getRandomData(TypeData* type, int size, SizeSafeValueGenerator* gen);

#define GenDataHead(name) Data __getRandomData(Ty ## name* type, int size, SizeSafeValueGenerator* gen)
#define GenDataCase(name) case TypeType::TYPE_TOKEN_ ## name: return __getRandomData(dynamic_cast<Ty ## name*>(type), size, gen)
#define GenDataErrorCase(name) GenDataHead(name) { \
    LOG(FATAL) << "Unexpected type when generation " << type->toString(); }

    GenDataErrorCase(Poly); GenDataErrorCase(Var); GenDataErrorCase(Arr);

    GenDataHead(Int) {
        return gen->getRandomInt();
    }
    GenDataHead(Bool) {
        return gen->getRandomBool();
    }
    GenDataHead(Unit) {
        return Data(std::make_shared<incre::semantics::VUnit>());
    }
    GenDataHead(Tuple) {
        auto* scheme_list = gen->getPossibleSplit(type, size); assert(!scheme_list->empty());
        std::uniform_int_distribution<int> choice_dist(0, int(scheme_list->size()) - 1);
        auto& scheme = std::get<std::vector<int>>(scheme_list->at(choice_dist(gen->env->random_engine)));
        assert(scheme.size() == type->fields.size());
        DataList fields;
        for (int i = 0; i < scheme.size(); ++i) {
            fields.push_back(_getRandomData(type->fields[i].get(), scheme[i], gen));
        }
        return BuildData(Product, fields);
    }
    GenDataHead(Ind) {
        auto* scheme_list = gen->getPossibleSplit(type, size); assert(!scheme_list->empty());
        std::uniform_int_distribution<int> choice_dist(0, int(scheme_list->size()) - 1);
        auto& [cons_name, body_ty] = std::get<std::pair<std::string, Ty>>(scheme_list->at(choice_dist(gen->env->random_engine)));
        auto body = _getRandomData(body_ty.get(), size - 1, gen);
        return Data(std::make_shared<incre::semantics::VInd>(cons_name, body));
    }
    GenDataHead(Compress) {
        auto* labeled_type = dynamic_cast<TyLabeledCompress*>(type);
        if (!labeled_type) LOG(FATAL) << "Unexpected type " << type->toString();
        auto body = _getRandomData(labeled_type->body.get(), size, gen);
        return Data(std::make_shared<incre::semantics::VLabeledCompress>(body, labeled_type->id));
    }

    Data _getRandomData(TypeData* type, int size, SizeSafeValueGenerator* gen) {
        switch (type->getType()) {
            TYPE_CASE_ANALYSIS(GenDataCase);
        }
    }
}

Data SizeSafeValueGenerator::getRandomData(const syntax::Ty &type) {
    std::uniform_int_distribution<int> dis_dist(0, KSizeLimit);
    while (true) {
        auto size = dis_dist(env->random_engine);
        if (getPossibleSplit(type.get(), size)->empty()) continue;
        return _getRandomData(type.get(), size, this);
    }
}

