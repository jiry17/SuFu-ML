//
// Created by pro on 2023/12/14.
//

#include "istool/incre/trans/incre_trans.h"
#include "istool/ext/deepcoder/data_type.h"
#include "istool/incre/analysis/incre_instru_types.h"
#include "glog/logging.h"

using namespace incre::trans;
using namespace incre::syntax;

incre::trans::TIncreInd::TIncreInd(const std::string &_name, const TypeList &_params): name(_name), params(_params) {
}

bool incre::trans::TIncreInd::equal(Type *type) {
    auto* incre_type = dynamic_cast<TIncreInd*>(type);
    if (!incre_type || name != incre_type->name || params.size() != incre_type->params.size())  {
        return false;
    }
    for (int i = 0; i < params.size(); ++i) {
        if (!params[i]->equal(incre_type->params[i].get())) {
            return false;
        }
    }
    return true;
}

PType incre::trans::TIncreInd::clone(const TypeList &new_params) {
    return std::make_shared<TIncreInd>(name, new_params);
}

std::string incre::trans::TIncreInd::getName() {
    auto res = getBaseName() + "(";
    for (int i = 0; i < params.size(); ++i) {
        if (i) res += ","; res += params[i]->getName();
    }
    return res + ")";
}

std::string incre::trans::TIncreInd::getBaseName() {
    return name;
}

TypeList incre::trans::TIncreInd::getParams() {return params;}

incre::trans::TCompress::TCompress(const PType &_body): body(_body) {}

bool incre::trans::TCompress::equal(Type *type) {
    auto* compress_type = dynamic_cast<TCompress*>(type);
    return compress_type && body->equal(compress_type->body.get());
}

PType incre::trans::TCompress::clone(const TypeList &params) {
    return std::make_shared<TCompress>(params[0]);
}

TypeList incre::trans::TCompress::getParams() {
    return {body};
}

std::string incre::trans::TCompress::getName() {
    return getBaseName() + "(" + body->getName() + ")";
}

std::string incre::trans::TCompress::getBaseName() {
    return "Packed";
}

incre::trans::TLabeledCompress::TLabeledCompress(const PType &_body, int _id):
    TCompress(_body), id(_id) {
}

PType incre::trans::TLabeledCompress::clone(const TypeList &params) {
    return std::make_shared<TLabeledCompress>(params[0], id);
}

bool incre::trans::TLabeledCompress::equal(Type *type) {
    auto* labeled_type = dynamic_cast<TLabeledCompress*>(type);
    return labeled_type == type && id == labeled_type->id && body->equal(labeled_type->body.get());
}

std::string incre::trans::TLabeledCompress::getBaseName() {
    return "Packed[" + std::to_string(id) + "]";
}

#define FromIncreCase(name) case TypeType::TYPE_TOKEN_ ## name: return _typeFromIncre(dynamic_cast<Ty ## name*>(type));
#define FromIncreHead(name) PType _typeFromIncre(Ty ## name* type)

namespace {
    FromIncreHead(Int) {
        return theory::clia::getTInt();
    }
    FromIncreHead(Bool) {
        return type::getTBool();
    }
    FromIncreHead(Unit) {
        return std::make_shared<TBot>();
    }
    FromIncreHead(Arr) {
        auto inp = incre::trans::typeFromIncre(type->inp.get());
        auto oup = incre::trans::typeFromIncre(type->oup.get());
        return std::make_shared<TArrow>((TypeList){inp}, oup);
    }
    FromIncreHead(Tuple) {
        TypeList elements;
        for (auto& field: type->fields) {
            elements.push_back(typeFromIncre(field.get()));
        }
        return std::make_shared<TProduct>(elements);
    }
    FromIncreHead(Compress) {
        auto content = typeFromIncre(type->body.get());
        auto* labeled_type = dynamic_cast<TyLabeledCompress*>(type);
        if (labeled_type) return std::make_shared<TLabeledCompress>(content, labeled_type->id);
        return std::make_shared<TCompress>(content);
    }
    FromIncreHead(Poly) {
        LOG(FATAL) << "TyPoly is not supported in translation, but received " << type->toString();
    }
    FromIncreHead(Var) {
        LOG(FATAL) << "TyVar is not supported in translation, but received " << type->toString();
    }
    FromIncreHead(Ind) {
        TypeList params;
        for (auto& param: type->param_list) params.push_back(typeFromIncre(param.get()));
        return std::make_shared<TIncreInd>(type->name, params);
    }
}

PType incre::trans::typeFromIncre(TypeData* type) {
    switch (type->getType()) {
        TYPE_CASE_ANALYSIS(FromIncreCase);
    }
}

Ty incre::trans::typeToIncre(Type *type) {
    {
        auto* it = dynamic_cast<TInt*>(type);
        if (it) return std::make_shared<TyInt>();
    }
    {
        auto* bt = dynamic_cast<TBool*>(type);
        if (bt) return std::make_shared<TyBool>();
    }
    {
        auto* bt = dynamic_cast<TBot*>(type);
        if (bt) return std::make_shared<TyUnit>();
    }
    {
        auto* pt = dynamic_cast<TProduct*>(type);
        if (pt) {
            TyList fields;
            for (const auto& sub: pt->sub_types) {
                fields.push_back(typeToIncre(sub.get()));
            }
            return std::make_shared<TyTuple>(fields);
        }
    }
    {
        auto* at = dynamic_cast<TArrow*>(type);
        if (at) {
            auto res = typeToIncre(at->oup_type.get());
            for (int i = int(at->inp_types.size()) - 1; i >= 0; --i) {
                auto inp = typeToIncre(at->inp_types[i - 1].get());
                res = std::make_shared<TyArr>(inp, res);
            }
            return res;
        }
    }
    {
        auto* it = dynamic_cast<TIncreInd*>(type);
        if (it) {
            TyList params;
            for (auto& param: it->params) {
                params.push_back(typeToIncre(param.get()));
            }
            return std::make_shared<TyInd>(it->name, params);
        }
    }
    LOG(FATAL) << "Unexpected type for translating to incre: " << type->getName();
}


namespace {
#define RegisterOperator(name) {name, name}
    const std::unordered_map<std::string, std::string> KOperatorNameMap = {
            RegisterOperator("+"), RegisterOperator("*"), RegisterOperator("/"),
            RegisterOperator("=="), RegisterOperator("<"), RegisterOperator("<="),
            RegisterOperator(">"), RegisterOperator(">="), RegisterOperator("and"),
            RegisterOperator("or"), RegisterOperator("not"), {"||", "or"},
            {"&&", "and"}, {"!", "not"}, {"=", "=="},
            RegisterOperator("-")
    };
}

std::string incre::trans::operatorNameToIncre(const std::string &name) {
    auto it = KOperatorNameMap.find(name);
    if (it == KOperatorNameMap.end()) LOG(FATAL) << "Unknown operator " << name;
    return it->second;
}

