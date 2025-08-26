//
// Created by pro on 2025/8/22.
//

#include "istool/incre/io/incre_json.h"
#include "istool/incre/language/incre_semantics.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::io;

namespace {
    template<typename T> Json::Value _vec2json(const std::vector<T>& contents) {
        Json::Value value(Json::arrayValue);
        for (auto& val: contents) value.append(val);
        return value;
    }
}


namespace {
#define TypeJsonHead(name) Json::Value _ty2json_ ## name(Ty ## name* type, const std::map<int, std::string>& names)
#define TypeJsonCase(name) case TypeType::TYPE_TOKEN_##name: return _ty2json_##name(dynamic_cast<Ty ## name*>(type), names);
#define InitJsonWithType(type) Json::Value value; value["type"] = type

    Json::Value _type2json(TypeData* type, const std::map<int, std::string>& names);

    TypeJsonHead(Int) {
        InitJsonWithType("int"); return value;
    }

    TypeJsonHead(Unit) {
        InitJsonWithType("unit"); return value;
    }

    TypeJsonHead(Bool) {
        InitJsonWithType("bool"); return value;
    }

    TypeJsonHead(Var) {
        if (std::holds_alternative<Ty>(type->info)) {
            auto content = std::get<Ty>(type->info);
            return _type2json(content.get(), names);
        }
        auto [index, _, __] = std::get<std::tuple<int, int, VarRange>>(type->info);
        auto it = names.find(index);
        assert(it != names.end());
        InitJsonWithType("var"); value["name"] = it->second;
        return value;
    }

    TypeJsonHead(Poly) {
        auto* named_type = dynamic_cast<TyPolyWithName*>(type);
        assert(named_type && names.empty());

        std::map<int, std::string> name_map;
        for (int i = 0; i < named_type->name_list.size(); ++i) {
            name_map[named_type->var_list[i]] = named_type->name_list[i];
        }

        InitJsonWithType("poly");
        value["vars"] = _vec2json(named_type->name_list);
        LOG(INFO) << "body " << named_type->body.get();
        value["body"] = _type2json(named_type->body.get(), name_map);
        return value;
    }

    TypeJsonHead(Arr) {
        InitJsonWithType("arrow");
        value["s"] = _type2json(type->inp.get(), names);
        value["t"] = _type2json(type->oup.get(), names);
        return value;
    }

    TypeJsonHead(Tuple) {
        InitJsonWithType("tuple");
        Json::Value fields(Json::arrayValue);
        for (auto& sub_type: type->fields) {
            fields.append(_type2json(sub_type.get(), names));
        }
        value["fields"] = fields;
        return value;
    }

    TypeJsonHead(Ind) {
        InitJsonWithType("cons");
        Json::Value params(Json::arrayValue);
        for (auto& sub_type: type->param_list) {
            params.append(_type2json(sub_type.get(), names));
        }
        value["name"] = type->name;
        value["params"] = params;
        return value;
    }

    TypeJsonHead(Compress) {
        InitJsonWithType("cons");
        Json::Value params(Json::arrayValue);
        params.append(_type2json(type->body.get(), names));
        value["name"] = "compress";
        value["params"] = params;
        return value;
    }

    Json::Value _type2json(TypeData* type, const std::map<int, std::string>& names) {
        // LOG(INFO) << "type to json: " << type->toString();
        switch (type->type) {
            TYPE_CASE_ANALYSIS(TypeJsonCase)
        }
    }

    Json::Value _type2json(TypeData* type) {
        LOG(INFO) << "type " << type->toString() << " to json";
        return _type2json(type, {});
    }
}

namespace {
    Json::Value _pattern2json(PatternData* pattern);

    Json::Value _pattern2json_var(PtVar* pattern) {
        InitJsonWithType("var");
        value["name"] = pattern->name;
        if (pattern->body) {
            value["content"] = _pattern2json(pattern->body.get());
        }
        return value;
    }

    Json::Value _pattern2json_tuple(PtTuple* pattern) {
        InitJsonWithType("tuple");
        Json::Value fields(Json::arrayValue);
        for (auto& content: pattern->fields) {
            fields.append(_pattern2json(content.get()));
        }
        value["fields"] = fields;
        return value;
    }

    Json::Value _pattern2json_wildcard(PtUnderScore* pattern) {
        InitJsonWithType("underscore");
        return value;
    }

    Json::Value _pattern2json_cons(PtCons* pattern) {
        InitJsonWithType("cons");
        value["name"] = pattern->name;
        if (pattern->body) {
            value["content"] = _pattern2json(pattern->body.get());
        }
        return value;
    }

    Json::Value _pattern2json(PatternData* pattern) {
        switch (pattern->getType()) {
            case PatternType::VAR: {
                return _pattern2json_var(dynamic_cast<PtVar*>(pattern));
            }
            case PatternType::CONS: {
                return _pattern2json_cons(dynamic_cast<PtCons*>(pattern));
            }
            case PatternType::TUPLE: {
                return _pattern2json_tuple(dynamic_cast<PtTuple*>(pattern));
            }
            case PatternType::UNDERSCORE: {
                return _pattern2json_wildcard(dynamic_cast<PtUnderScore*>(pattern));
            }
        }
    }
}

#include <set>

namespace {

#define TermJsonHead(name) Json::Value _term2json_ ## name(Tm ## name* term, std::set<std::pair<int, int>>& indices)
#define TermJsonCase(name) case TermType::TERM_TOKEN_##name: return _term2json_##name(dynamic_cast<Tm ## name*>(term), indices);

    Json::Value _term2json(TermData* term, std::set<std::pair<int, int>>& indices);

    TermJsonHead(Value) {
        {
            auto *vi = dynamic_cast<IntValue *>(term->v.get());
            if (vi) {
                InitJsonWithType("int");
                value["value"] = vi->w;
                return value;
            }
        }
        {
            auto *vb = dynamic_cast<BoolValue *>(term->v.get());
            if (vb) {
                if (vb->w) {
                    InitJsonWithType("true");
                    return value;
                } else {
                    InitJsonWithType("false");
                    return value;
                }
            }
        }
        {
            auto *vu = dynamic_cast<incre::semantics::VUnit *>(term->v.get());
            if (vu) {
                InitJsonWithType("unit");
                return value;
            }
        }
        {
            auto *vc = dynamic_cast<incre::semantics::VInd *>(term->v.get());
            if (vc) {
                InitJsonWithType("cons");
                value["name"] = vc->name;
                auto body = std::make_shared<TmValue>(vc->body);
                value["body"] = _term2json_Value(body.get(), indices);
                return value;
            }
        }
        {
            auto *vc = dynamic_cast<incre::semantics::VCompress *>(term->v.get());
            if (vc) {
                InitJsonWithType("cons");
                value["name"] = "compress";
                auto body = std::make_shared<TmValue>(vc->body);
                value["body"] = _term2json_Value(body.get(), indices);
                return value;
            }
        }
        {
            auto *vc = dynamic_cast<incre::semantics::VClosure *>(term->v.get());
            if (vc) {
                InitJsonWithType("func");
                value["param"] = vc->name;
                value["body"] = _term2json(vc->body.get(), indices);
                return value;
            }
        }
        LOG(ERROR) << "Unsupported value type " << term->toString();
        assert(0);
    }

    TermJsonHead(If) {
        InitJsonWithType("if");
        value["condition"] = _term2json(term->c.get(), indices);
        value["true"] = _term2json(term->t.get(), indices);
        value["false"] = _term2json(term->f.get(), indices);
        return value;
    }

    TermJsonHead(Var) {
        InitJsonWithType("var");
        value["name"] = term->name;
        return value;
    }

    TermJsonHead(Primary) {
        InitJsonWithType("op");
        Json::Value operand(Json::arrayValue);
        for (auto& field: term->params) {
            operand.append(_term2json(field.get(), indices));
        }
        value["operator"] = term->op_name;
        value["operand"] = operand;
        return value;
    }

    TermJsonHead(Tuple) {
        InitJsonWithType("tuple");
        Json::Value fields(Json::arrayValue);
        for (auto& field: term->fields) {
            fields.append(_term2json(field.get(), indices));
        }
        value["fields"] = fields;
        return value;
    }

    TermJsonHead(Func) {
        InitJsonWithType("func");
        value["param"] = term->name;
        value["body"] = _term2json(term->body.get(), indices);
        return value;
    }

    TermJsonHead(Let) {
        std::string ty;
        if (term->is_rec) ty = "letrec"; else ty = "let";
        InitJsonWithType(ty);
        value["name"] = term->name;
        value["def"] = _term2json(term->def.get(), indices);
        value["content"] = _term2json(term->body.get(), indices);
        return value;
    }

    TermJsonHead(Match) {
        InitJsonWithType("match");
        Json::Value case_list(Json::arrayValue);
        for (auto& [pt, body]: term->cases) {
            Json::Value case_node(Json::arrayValue);
            case_node.append(_pattern2json(pt.get()));
            case_node.append(_term2json(body.get(), indices));
            case_list.append(case_node);
        }
        value["cases"] = case_list;
        value["value"] = _term2json(term->def.get(), indices);
        return value;
    }

    TermJsonHead(App) {
        InitJsonWithType("app");
        value["func"] = _term2json(term->func.get(), indices);
        value["param"] = _term2json(term->param.get(), indices);
        return value;
    }

    TermJsonHead(Cons) {
        InitJsonWithType("cons");
        value["name"] = term->cons_name;
        value["content"] = _term2json(term->body.get(), indices);
        return value;
    }

    TermJsonHead(Label) {
        InitJsonWithType("app");
        value["func"] = "label";
        value["param"] = _term2json(term->body.get(), indices);
        return value;
    }

    TermJsonHead(Unlabel) {
        InitJsonWithType("app");
        value["func"] = "unlabel";
        value["param"] = _term2json(term->body.get(), indices);
        return value;
    }

    TermJsonHead(Rewrite) {
        InitJsonWithType("app");
        value["func"] = "rewrite";
        value["param"] = _term2json(term->body.get(), indices);
        return value;
    }

    std::string _getProjName(int id, int size) {
        if (size == 2) return (id == 1 ? "fst" : "snd");
        return "get_" + std::to_string(id) + "_from_" + std::to_string(size);
    }

    TermJsonHead(Proj) {
        indices.insert(std::make_pair(term->id, term->size));
        auto proj_name = _getProjName(term->id, term->size);
        auto proj_term = std::make_shared<TmVar>(proj_name);
        auto new_term = std::make_shared<TmApp>(proj_term, term->body);
        return _term2json(new_term.get(), indices);
    }

    Json::Value _term2json(TermData* term, std::set<std::pair<int, int>>& indices) {
        switch (term->getType()) {
            TERM_CASE_ANALYSIS(TermJsonCase)
        }
    }
}

namespace {
    Ty _assignName(const Ty& type) {
        if (type->getType() != TypeType::POLY) return type;
        auto* named_type = dynamic_cast<TyPolyWithName*>(type.get());
        if (named_type) return type;
        auto* poly_type = dynamic_cast<TyPoly*>(type.get());
        assert(poly_type && poly_type->var_list.size() <= 26);
        std::vector<std::string> name_list;
        for (int i = 0; i < poly_type->var_list.size(); ++i) {
            std::string name = "'";
            name += char('a' + i);
            name_list.push_back(name);
        }
        return std::make_shared<TyPolyWithName>(name_list, poly_type->var_list, poly_type->body);
    }

    Json::Value _command2json(CommandData* command, std::set<std::pair<int, int>>& indices) {
        switch (command->getType()) {
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command);
                std::string type_name;
                if (cb->is_rec) type_name = "func"; else type_name = "bind";
                InitJsonWithType(type_name);
                value["name"] = cb->name;
                value["def"] = _term2json(cb->term.get(), indices);
                return value;
            }
            case CommandType::DECLARE: {
                auto* cd = dynamic_cast<CommandDeclare*>(command);
                InitJsonWithType("declare");
                value["name"] = cd->name;
                auto lifted_type = _assignName(cd->type);
                // LOG(INFO) << "type " << cd->type->toString() << " " << dynamic_cast<TyPolyWithName*>(lifted_type.get());
                value["ty"] = _type2json(lifted_type.get());
                // LOG(INFO) << "finished";
                return value;
            }
            case CommandType::DEF_IND: {
                auto* ci = dynamic_cast<CommandDef*>(command);
                InitJsonWithType("type");
                value["name"] = ci->name;
                value["arity"] = ci->param;
                Json::Value cons(Json::arrayValue);
                for (auto& [cons_name, cons_ty]: ci->cons_list) {
                    Json::Value cons_node(Json::arrayValue);
                    cons_node.append(cons_name);
                    auto lifted_type = _assignName(cons_ty);
                    cons_node.append(_type2json(lifted_type.get()));
                    cons.append(cons_node);
                }
                value["cons"] = cons;
                return value;
            }
            case CommandType::EVAL: {
                auto* ce = dynamic_cast<CommandEval*>(command);
                InitJsonWithType("eval");
                value["term"] = _term2json(ce->term.get(), indices);
                return value;
            }
        }
    }

    Term _buildProjTerm(int id, int size) {
        std::vector<Pattern> fields;
        std::string var_name("y");
        std::string inp_name("x");
        for (int i = 1; i <= size; ++i) {
            if (i == id) {
                fields.push_back(std::make_shared<PtVar>(var_name, nullptr));
            } else {
                fields.push_back(std::make_shared<PtUnderScore>());
            }
        }
        auto pattern = std::make_shared<PtTuple>(fields);
        auto body = std::make_shared<TmVar>(var_name);
        auto match_term = std::make_shared<TmMatch>(
                std::make_shared<TmVar>(inp_name), std::vector<std::pair<Pattern, Term>>{ {pattern, body} }
                );
        return std::make_shared<TmFunc>(inp_name, match_term);
    }
}

Json::Value incre::io::program2json(incre::IncreProgramData *program) {
    std::vector<Json::Value> value_list;
    std::set<std::pair<int, int>> indices;
    for (auto& command: program->commands) {
        value_list.push_back(_command2json(command.get(), indices));
    }

    Json::Value result(Json::arrayValue);
    for (auto& [id, size]: indices) {
        auto term = _buildProjTerm(id, size);
        auto name = _getProjName(id, size);
        auto command = std::make_shared<CommandBindTerm>(
                name, false, term, DecorateSet(), std::string()
                );
        result.append(_command2json(command.get(), indices));
    }
    for (auto& value: value_list)  result.append(value);
    return result;
}

#include <fstream>
#include <iostream>
#include "istool/basic/config.h"

void incre::io::printProgram2F(const std::string &path, incre::IncreProgramData *program) {
    std::string tmp_file = "/tmp/" + std::to_string(rand()) + ".json";
    // std::string tmp_file = ::config::KSourcePath + "incre-tests/" + std::to_string(rand()) + ".json";
    auto json_value = program2json(program);

    std::ofstream out(tmp_file);
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(json_value, &out);

    std::string command = ::config::KIncrePrinterPath + " " + tmp_file + " " + path;
    LOG(INFO) << command;

    std::system(command.c_str());
    // std::system(("rm " + tmp_file).c_str());
}