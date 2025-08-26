//
// Created by pro on 2022/10/8.
//

#include "istool/incre/io/incre_printer.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::io;
using namespace incre::syntax;

namespace {

    std::string _aType2String(TypeData*, const std::unordered_map<int, std::string>&);
    std::string _arrowType2String(TypeData*, const std::unordered_map<int, std::string>&);

    std::string _dataType2String(TypeData* type, const std::unordered_map<int, std::string>& var_map) {
        switch (type->getType()) {
            case TypeType::IND: {
                auto* ti = dynamic_cast<TyInd*>(type);
                std::string res = ti->name;
                for (auto& param: ti->param_list) res += " " + _aType2String(param.get(), var_map);
                return res;
            }
            case TypeType::COMPRESS: {
                auto* tc = dynamic_cast<TyCompress*>(type);
                return "Reframe " + _aType2String(tc->body.get(), var_map);
            }
            default: return _aType2String(type, var_map);
        }
    }

    std::string _aType2String(TypeData* type, const std::unordered_map<int, std::string>& var_map) {
        switch (type->getType()) {
            case TypeType::BOOL: return "Bool";
            case TypeType::INT: return "Int";
            case TypeType::UNIT: return "Unit";
            case TypeType::VAR: {
                auto* tv = dynamic_cast<TyVar*>(type); assert(tv && !tv->is_bounded());
                auto [index, _level, _info] = tv->get_var_info();
                auto it = var_map.find(index);
                assert(it != var_map.end());
                return it->second;
            }
            default: return "(" + _arrowType2String(type, var_map) + ")";
        }
    }

    std::string _arrowType2String(TypeData* type, const std::unordered_map<int, std::string>& var_map) {
        switch (type->getType()) {
            case TypeType::ARR: {
                auto* ta = dynamic_cast<TyArr*>(type);
                return _aType2String(ta->inp.get(), var_map) + " -> " + _arrowType2String(ta->oup.get(), var_map);
            }
            case TypeType::TUPLE: {
                auto* tt = dynamic_cast<TyTuple*>(type);
                std::string res;
                for (int i = 0; i < tt->fields.size(); ++i) {
                    if (i) res = res + " * ";
                    res += _dataType2String(tt->fields[i].get(), var_map);
                }
                return res;
            }
            default: return _dataType2String(type, var_map);
        }
    }
}

std::string io::type2String(TypeData* type) {
    if (type->getType() == TypeType::POLY) {
        auto* tp = dynamic_cast<TyPoly*>(type);
        assert(tp->var_list.size() < 26);
        std::unordered_map<int, std::string> var_map;
        for (int i = 0; i < tp->var_list.size(); ++i) {
            var_map[tp->var_list[i]] = std::string(1, 'a' + i);
        }
        return _arrowType2String(tp->body.get(), var_map);
    } else return _arrowType2String(type, {});
}