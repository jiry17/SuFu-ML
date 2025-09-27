//
// Created by pro on 2025/9/8.
//

#include "istool/incre/io/incre_to_rust.h"
#include <iostream>
#include <ranges>
#include <sstream>
#include "glog/logging.h"

using namespace incre::syntax;
using namespace incre::rust;
using util::TypeSignature;

std::string util::wrapWithRc(const std::string &type) {
    return "Rc<" + type + ">";
}

// every non-empty type should be appeared as Rc<xxx>
// function types should be introduced as a trait variable
// every parameter should have implemented Clone

namespace {
    std::string _getParamName(int index) {
        return "T" + std::to_string(index);
    }

    struct TypeSignatureContext {
        std::unordered_map<int, int> var_index_map;
        std::vector<std::pair<std::string, std::string>> param_infos;

        std::string insertVar(int index) {
            auto it = var_index_map.find(index);
            if (it == var_index_map.end()) {
                int param_index = param_infos.size();
                auto param_name = _getParamName(param_index);
                param_infos.emplace_back(param_name, "Clone");
                var_index_map[index] = param_index;
                return param_name;
            }
            return param_infos[it->second].first;
        }
    };

    std::string _buildTypeSignature(TypeData* type, TypeSignatureContext& ctx);

#define TypeSignatureHead(Name) std::string __buildTypeSignature(Ty ## Name* type, TypeSignatureContext& ctx)
#define TypeSignatureCase(Name) case TypeType::TYPE_TOKEN_ ## Name: return __buildTypeSignature(dynamic_cast<Ty ## Name*>(type), ctx);

    TypeSignatureHead(Int) {return "i32";}

    TypeSignatureHead(Bool) {return "bool";}

    TypeSignatureHead(Unit) {return "()";}

    TypeSignatureHead(Tuple) {
        std::string result;
        for (int i = 0; i < type->fields.size(); ++i) {
            if (i) result += ", "; else result += "(";
            result += _buildTypeSignature(type->fields[i].get(), ctx);
        }
        result += ")";
        return util::wrapWithRc(result);
    }

    TypeSignatureHead(Poly) {
        for (auto var_index: type->var_list) ctx.insertVar(var_index);
        return _buildTypeSignature(type->body.get(), ctx);
    }

    TypeSignatureHead(Arr) {
        int param_index = ctx.param_infos.size();
        auto param_name = _getParamName(param_index);
        ctx.param_infos.emplace_back(param_name, "");
        auto inp = _buildTypeSignature(type->inp.get(), ctx);
        auto oup = _buildTypeSignature(type->oup.get(), ctx);
        auto def = "Fn(" + inp + ") -> " + oup;
        ctx.param_infos[param_index].second = def;
        return util::wrapWithRc(param_name);
    }

    TypeSignatureHead(Ind) {
        std::string result = type->name;
        if (!type->param_list.empty()) {
            for (int i = 0; i < type->param_list.size(); ++i) {
                if (i) result += ", "; else result += "<";
                result += _buildTypeSignature(type->param_list[i].get(), ctx);
            }
            result += ">";
        }
        return util::wrapWithRc(result);
    }

    TypeSignatureHead(Compress) {
        LOG(FATAL) << "Do not support " << type->toString();
    }

    TypeSignatureHead(Var) {
        if (type->is_bounded()) {
            return _buildTypeSignature(type->get_bound_type().get(), ctx);
        }
        auto [index, _, __] = type->get_var_info();
        return ctx.insertVar(index);
    }

    std::string _buildTypeSignature(TypeData* type, TypeSignatureContext& ctx) {
        switch (type->getType()) {
            TYPE_CASE_ANALYSIS(TypeSignatureCase)
        }
    }
}

TypeSignature util::buildTypeSignature(const std::vector<std::string> &inp_names, syntax::TypeData *func_type) {
    TypeSignature result;
    TypeSignatureContext ctx;
    for (const auto & inp_name : inp_names) {
        assert(func_type->getType() == TypeType::ARR);
        auto* tr = dynamic_cast<TyArr*>(func_type);
        func_type = tr->oup.get();
        result.inp_list.emplace_back(inp_name, _buildTypeSignature(tr->inp.get(), ctx));
    }
    result.oup = _buildTypeSignature(func_type, ctx);
    result.params = ctx.param_infos;
    return result;
}

namespace {
    std::pair<Ty, Ty> _unfoldConsType(TypeData* type) {
        auto* tp = dynamic_cast<TyPoly*>(type);
        if (tp) type = tp->body.get();
        auto* ta = dynamic_cast<TyArr*>(type);
        if (!ta) {
            LOG(FATAL) << "Expect an arrow type, but got " << ta->toString();
        }
        return {ta->inp, ta->oup};
    }
}

/*
 * type 'a list =
* | Cons of 'a * 'a list
* | Nil
 */

/*
 * enum List<A> {
    Nil(()),
    Cons(A, Box<List<A>>)
}
 */

using util::indent;
const std::string INDENT = "  ";

std::string util::indent(int num) {
    std::string res;
    for (int i = 0; i < num; ++i) res += INDENT;
    return res;
}

void incre::rust::util::indDef2Rust(std::ostream &out, incre::CommandDef *command) {
    std::vector<std::string> cons_name_list; cons_name_list.reserve(command->cons_list.size());
    std::vector<Ty> cons_type_list; cons_type_list.reserve(command->cons_list.size());
    Ty ind_type;
    for (auto& [cons_name, cons_type]: command->cons_list) {
        auto [inp_type, oup_type] = _unfoldConsType(cons_type.get());
        cons_name_list.push_back(cons_name);
        cons_type_list.push_back(inp_type);
        ind_type = oup_type;
    }
    auto full_type = ind_type;
    for (auto& cons_type: cons_type_list | std::views::reverse) {
        full_type = std::make_shared<TyArr>(cons_type, full_type);
    }

    auto result = util::buildTypeSignature(cons_name_list, full_type.get());
    assert(result.params.size() == command->param);
    auto type_name = result.oup.substr(3, result.oup.size() - 4);
    out << "enum " << type_name << " {" << std::endl;
    for (auto& [cons_name, cons_type_name]: result.inp_list) {
        out << indent(1) << cons_name << "(" << cons_type_name << ")," << std::endl;
    }
    out << "}" << std::endl;
}