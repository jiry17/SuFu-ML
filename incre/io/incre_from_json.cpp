//
// Created by pro on 2022/9/17.
//

#include "istool/incre/io/incre_json.h"
#include "glog/logging.h"
#include <fstream>
#include "istool/basic/config.h"
#include "istool/sygus/theory/basic/clia/clia.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::semantics;
using namespace incre::io;

IncreParseError::IncreParseError(const std::string _message): message(_message) {}

const char *IncreParseError::what() const noexcept {return message.c_str();}

namespace {
    const std::unordered_map<std::string, TypeType> KTypeTypeNameMap = {
            {"var", TypeType::VAR}, {"unit", TypeType::UNIT},
            {"bool", TypeType::BOOL}, {"int", TypeType::INT},
            {"poly", TypeType::POLY}, {"arrow", TypeType::ARR},
            {"tuple", TypeType::TUPLE}, {"cons", TypeType::IND}
    };
}

syntax::TypeType io::string2TypeType(const std::string &type) {
    auto it = KTypeTypeNameMap.find(type);
    if (it == KTypeTypeNameMap.end()) throw IncreParseError("unknown TypeType " + type);
    return it->second;
}

std::string io::typeType2String(syntax::TypeType type) {
    for (auto& [type_name, type_type]: KTypeTypeNameMap) {
        if (type_type == type) {
            return type_name;
        }
    }
    throw IncreParseError("unknown TypeType");
}

namespace {
#define JsonTypeHead(name) syntax::Ty _json2ty_ ## name (const Json::Value & node)

    JsonTypeHead(Var) {
        TypeVarInfo info(std::make_tuple(node["index"].asInt(), 0, ANY));
        return std::make_shared<TyVar>(info);
    }

    JsonTypeHead(Int) {return std::make_shared<TyInt>();}
    JsonTypeHead(Bool) {return std::make_shared<TyBool>();}
    JsonTypeHead(Unit) {return std::make_shared<TyUnit>();}

    JsonTypeHead(Poly) {
        std::vector<std::string> name_list;
        for (auto& index_node: node["vars"]) name_list.push_back(index_node.asString());
        std::vector<int> var_list(name_list.size());
        for (int i = 0; i < name_list.size(); ++i) var_list[i] = i;
        return std::make_shared<TyPolyWithName>(name_list, var_list, json2ty(node["body"]));
    }

    JsonTypeHead(Arr) {
        return std::make_shared<TyArr>(json2ty(node["s"]), json2ty(node["t"]));
    }

    JsonTypeHead(Tuple) {
        TyList fields;
        for (auto& field: node["fields"]) fields.push_back(json2ty(field));
        return std::make_shared<TyTuple>(fields);
    }

    JsonTypeHead(Compress) {
        throw IncreParseError("Unexpected TypeType COMPRESS");
    }

    JsonTypeHead(Ind) {
        auto cons_name = node["name"].asString();
        TyList param_list;
        for (auto& param: node["params"]) param_list.push_back(json2ty(param));
        if (cons_name == "Packed" || cons_name == "Reframe" || cons_name == "compress") {
            if (param_list.size() != 1) throw IncreParseError("Packed should have exactly one parameter");
            return std::make_shared<TyCompress>(param_list[0]);
        }
        return std::make_shared<TyInd>(cons_name, param_list);
    }
}

#define JsonTypeCase(name) case TypeType::TYPE_TOKEN_##name: return _json2ty_ ## name(node);
syntax::Ty io::json2ty(const Json::Value &node) {
    // LOG(INFO) << "Try parsing " << node;
    switch (string2TypeType(node["type"].asString())) {
        TYPE_CASE_ANALYSIS(JsonTypeCase);
    }
}

namespace {
    const std::unordered_map<std::string, PatternType> KPatternTypeNameMap = {
            {"underscore", PatternType::UNDERSCORE}, {"var", PatternType::VAR},
            {"tuple", PatternType::TUPLE}, {"cons", PatternType::CONS}
    };
}

syntax::PatternType io::string2PatternType(const std::string &type) {
    auto it = KPatternTypeNameMap.find(type);
    if (it == KPatternTypeNameMap.end()) {
        throw IncreParseError("Unexpected PatternType " + type);
    }
    return it->second;
}

syntax::Pattern io::json2pattern(const Json::Value &node) {
    switch (string2PatternType(node["type"].asString())) {
        case PatternType::UNDERSCORE: return std::make_shared<PtUnderScore>();
        case PatternType::VAR: {
            if (node.isMember("content")) {
                return std::make_shared<PtVar>(node["name"].asString(), json2pattern(node["content"]));
            } else {
                return std::make_shared<PtVar>(node["name"].asString(), nullptr);
            }
        }
        case PatternType::TUPLE: {
            PatternList fields;
            for (auto& field: node["fields"]) fields.push_back(json2pattern(field));
            return std::make_shared<PtTuple>(fields);
        }
        case PatternType::CONS: {
            return std::make_shared<PtCons>(node["name"].asString(), json2pattern(node["content"]));
        }
    }
}

namespace {
    const std::unordered_map<std::string, TermType> KTermTypeNameMap {
            {"true", TermType::VALUE}, {"false", TermType::VALUE},
            {"if", TermType::IF}, {"unit", TermType::VALUE},
            {"var", TermType::VAR}, {"int", TermType::VALUE},
            {"op", TermType::PRIMARY}, {"app", TermType::APP},
            {"tuple", TermType::TUPLE}, {"proj", TermType::PROJ},
            {"let", TermType::LET}, {"func", TermType::FUNC},
            {"letrec", TermType::LET}, {"match", TermType::MATCH},
            {"cons", TermType::CONS}
    };
}

syntax::TermType io::string2TermType(const std::string &type) {
    auto it = KTermTypeNameMap.find(type);
    if (it == KTermTypeNameMap.end()) throw IncreParseError("Unexpected TermType " + type);
    return it->second;
}

namespace {
#define JsonTermHead(name) Term _json2term_## name (const Json::Value& node)

    JsonTermHead(Value) {
        auto name = node["type"].asString();
        Data v;
        if (name == "true") v = BuildData(Bool, false);
        else if (name == "false") v = BuildData(Bool, true);
        else if (name == "unit") v = Data(std::make_shared<VUnit>());
        else if (name == "int") v = BuildData(Int, node["value"].asInt());
        else throw IncreParseError("Unknown value " + name);
        return std::make_shared<TmValue>(v);
    }

    JsonTermHead(If) {
        auto c = json2term(node["condition"]), t = json2term(node["true"]), f = json2term(node["false"]);
        return std::make_shared<TmIf>(c, t, f);
    }

    JsonTermHead(Var) {
        return std::make_shared<TmVar>(node["name"].asString());
    }

    JsonTermHead(Primary) {
        TermList params;
        for (auto& sub: node["operand"]) params.push_back(json2term(sub));
        return std::make_shared<TmPrimary>(node["operator"].asString(), params);
    }

    JsonTermHead(Tuple) {
        TermList fields;
        for (auto& field: node["fields"]) fields.push_back(json2term(field));
        return std::make_shared<TmTuple>(fields);
    }

    JsonTermHead(Proj) {
        return std::make_shared<TmProj>(json2term(node["content"]), node["index"].asInt(), node["size"].asInt());
    }

    JsonTermHead(Func) {
        return std::make_shared<TmFunc>(node["name"].asString(), json2term(node["content"]));
    }

    JsonTermHead(Let) {
        bool is_rec = (node["type"].asString() == "letrec");
        auto def = json2term(node["def"]), body = json2term(node["content"]);
        return std::make_shared<TmLet>(node["name"].asString(), is_rec, def, body);
    }

    JsonTermHead(Match) {
        auto def = json2term(node["value"]);
        MatchCaseList cases;
        for (auto& case_node: node["cases"]) {
            cases.emplace_back(json2pattern(case_node["pattern"]), json2term(case_node["branch"]));
        }
        return std::make_shared<TmMatch>(def, cases);
    }

    bool _isReserved(const Term& term, const std::string& name) {
        auto* tv = dynamic_cast<TmVar*>(term.get());
        return tv && tv->name == name;
    }

    JsonTermHead(App) {
        auto func = json2term(node["func"]), param = json2term(node["param"]);
        if (_isReserved(func, "label")) return std::make_shared<TmLabel>(param);
        if (_isReserved(func, "unlabel")) return std::make_shared<TmUnlabel>(param);
        if (_isReserved(func, "rewrite")) return std::make_shared<TmRewrite>(param);
        return std::make_shared<TmApp>(func, param);
    }

    JsonTermHead(Cons) {
        return std::make_shared<TmCons>(node["name"].asString(), json2term(node["content"]));
    }

    JsonTermHead(Label) {throw IncreParseError("Unexpected TermType LABEL");}
    JsonTermHead(Unlabel) {throw IncreParseError("Unexpected TermType UNLABEL");}
    JsonTermHead(Rewrite) {throw IncreParseError("Unexpected TermType REWRITE");}

}

#define JsonTermCase(name) case TermType::TERM_TOKEN_##name: return _json2term_##name(node);
syntax::Term io::json2term(const Json::Value &node) {
    switch (string2TermType(node["type"].asString())) {
        TERM_CASE_ANALYSIS(JsonTermCase);
    }
}

#include "istool/sygus/theory/basic/string/str.h"

namespace {
    Data _json2data(const Json::Value& node) {
       auto type = node["type"].asString();
       if (type == "int") return BuildData(Int, node["value"].asInt());
       if (type == "bool") return BuildData(Bool, node["value"].asBool());
       if (type == "string") return BuildData(String, node["value"].asString());
       throw IncreParseError("Unknown data type " + type);
    }

    const std::unordered_map<std::string, IncreConfig> KIncreConfigNameMap {
            {"ComposeNum", IncreConfig::COMPOSE_NUM}, {"VerifyBase", IncreConfig::VERIFY_BASE},
            {"NonLinear", IncreConfig::NON_LINEAR}, {"SampleSize", IncreConfig::SAMPLE_SIZE},
            {"EnableFold", IncreConfig::ENABLE_FOLD}, {"SampleIntMin", IncreConfig::SAMPLE_INT_MIN},
            {"SampleIntMax", IncreConfig::SAMPLE_INT_MAX}, {"PrintAlign", IncreConfig::PRINT_ALIGN},
            {"TermNum", IncreConfig::TERM_NUM}, {"ClauseNum", IncreConfig::CLAUSE_NUM},
            {"SlowCombine", IncreConfig::SLOW_COMBINE}
    };

    const std::unordered_map<std::string, CommandDecorate> KIncreDecorateNameMap {
            {"Input", CommandDecorate::INPUT}, {"Start", CommandDecorate::START},
            {"Compress", CommandDecorate::SYN_COMPRESS}, {"Combine", CommandDecorate::SYN_COMBINE},
            {"Extract", CommandDecorate::SYN_EXTRACT}, {"NoPartial", CommandDecorate::SYN_NO_PARTIAL},
            {"Exclude", CommandDecorate::SYN_EXCLUDE}
    };
}

CommandDecorate io::string2Decorate(const std::string &name) {
    auto it = KIncreDecorateNameMap.find(name);
    if (it == KIncreDecorateNameMap.end()) throw IncreParseError("Unknown CommandDecorate " + name);
    return it->second;
}

IncreConfig io::string2IncreConfig(const std::string &name) {
    auto it = KIncreConfigNameMap.find(name);
    if (it == KIncreConfigNameMap.end()) throw IncreParseError("Unknown IncreConfig " + name);
    return it->second;
}

namespace {
    std::unordered_set<CommandDecorate> _extractDecros(const Json::Value& node) {
        std::unordered_set<CommandDecorate> deco_list;
        for (auto& config_node: node) {
            deco_list.insert(string2Decorate(config_node.asString()));
        }
        return deco_list;
    }

    void _collectCommands(const Json::Value& node, const std::string& source, IncreConfigMap& configs, CommandList& commands) {
        for (auto& command_node: node) {
            auto command_type = command_node["type"].asString();
            auto decos = _extractDecros(command_node["decos"]);
            if (command_type == "config") {
                configs[string2IncreConfig(command_node["name"].asString())] = _json2data(command_node["value"]);
            } else if (command_type == "import") {
                std::string new_source = source;
                if (new_source.empty()) new_source = command_node["source"].asString();
                _collectCommands(command_node["content"], new_source, configs, commands);
            } else if (command_type == "bind") {
                auto var_name = command_node["name"].asString();
                auto def = json2term(command_node["def"]);
                commands.push_back(std::make_shared<CommandBindTerm>(var_name, false, def, decos, source));
            } else if (command_type == "type") {
                auto type_name = command_node["name"].asString();
                auto arity = command_node["arity"].asInt();
                std::vector<std::pair<std::string, Ty>> cons_list;
                for (auto& cons_node: command_node["cons"]) {
                    cons_list.emplace_back(cons_node["name"].asString(), json2ty(cons_node["type"]));
                }
                commands.push_back(std::make_shared<CommandDef>(type_name, arity, cons_list, decos, source));
            } else if (command_type == "func") {
                auto var_name = command_node["name"].asString();
                auto def = json2term(command_node["def"]);
                commands.push_back(std::make_shared<CommandBindTerm>(var_name, true, def, decos, source));
            } else if (command_type == "declare") {
                auto var_name = command_node["name"].asString();
                auto type = json2ty(command_node["ty"]);
                commands.push_back(std::make_shared<CommandDeclare>(var_name, type, decos, source));
            } else if (command_type == "eval") {
                auto term = json2term(command_node["term"]);
                commands.push_back(std::make_shared<CommandEval>("", term, decos, source));
            }
        }
    }
}

IncreProgram io::json2program(const Json::Value &node) {
    IncreConfigMap configs;
    CommandList commands;
    _collectCommands(node, "", configs, commands);
    return std::make_shared<IncreProgramData>(commands, configs);
}

incre::IncreProgram io::json2program(const std::string &path) {
    Json::Reader reader;
    Json::Value root;
    std::ifstream inp(path, std::ios::out);
    std::stringstream buf;
    buf << inp.rdbuf();
    inp.close();
    assert(reader.parse(buf.str(), root));
    return json2program(root);
}

IncreProgram io::parseFromF(const std::string &path) {
    std::string tmp_file = "/tmp/" + std::to_string(rand()) + ".json";
    std::string command = ::config::KIncreParserPath + " " + path + " " + tmp_file;
    std::system(command.c_str());
    auto res = json2program(tmp_file);
    std::system(("rm " + tmp_file).c_str());
    return res;
}