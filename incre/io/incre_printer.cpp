//
// Created by pro on 2024/3/31.
//

#include "istool/incre/io/incre_printer.h"
#include "glog/logging.h"
#include <iostream>

using namespace incre;
using namespace incre::io;
using namespace incre::syntax;

namespace {
    void addSkipLine(std::vector<std::string>& lines) {
        if (!lines.empty()) lines.push_back("\n");
    }

    void reportError(const Ty& type) {
        LOG(FATAL) << "Unexpected constructor type " << type->toString();
    }

    Ty getParamType(const Ty& type, int param_num) {
        auto content_type = type;
        if (param_num) {
            auto* tp = dynamic_cast<TyPoly*>(type.get());
            if (!tp || tp->var_list.size() != param_num) reportError(type);
            for (int i = 0; i < tp->var_list.size(); ++i) {
                if (tp->var_list[i] != i) reportError(type);
            }
            content_type = tp->body;
        }
        if (content_type->getType() != TypeType::ARR) reportError(type);
        auto* ta = dynamic_cast<TyArr*>(content_type.get());
        if (param_num) {
            auto* tp = dynamic_cast<TyPoly*>(type.get());
            return std::make_shared<TyPoly>(tp->var_list, ta->inp);
        }
        return ta->inp;
    }

    std::string _command2String(CommandDef* command) {
        auto head = "data " + command->name;
        for (int i = 0; i < command->param; ++i) head += " " + std::string(1, 'a' + i);
        head += " =";
        bool is_start = true;
        for (auto& [cons_name, cons_type]: command->cons_list) {
            auto inp_type = getParamType(cons_type, command->param);
            std::string cons_str = cons_name;
            if (!is_start) head += " |"; is_start = false;

            auto type_str = type2String(inp_type.get());
            if (type_str != "Unit") cons_str += " " + type2String(inp_type.get());
            head += " " + cons_str;
        }
        return head + ";";
    }
}

bool _addSkipLine(CommandType command_type) {
    return command_type == CommandType::BIND_TERM;
}


void io::printProgram(IncreProgramData *program, const std::string &path, bool is_highlight) {
    std::vector<std::string> lines;

    std::unordered_set<std::string> import_set;
    for (auto& command: program->commands) {
        if (!command->source.empty()) {
            if (import_set.find(command->source) == import_set.end()) {
                import_set.insert(command->source);
                lines.push_back("import \"" + command->source + "\";\n");
            }
        }
    }
    CommandType previous_type = CommandType::EVAL;
    for (auto& command: program->commands) {
        if (!command->source.empty()) continue;
        switch (command->getType()) {
            case CommandType::DECLARE: {
                if (_addSkipLine(previous_type)) addSkipLine(lines);
                previous_type = CommandType::DECLARE;

                auto* cd = dynamic_cast<CommandDeclare*>(command.get());
                lines.push_back(cd->name + " :: " + type2String(cd->type.get()) + ";\n");
                break;
            }
            case CommandType::DEF_IND: {
                if (_addSkipLine(previous_type)) addSkipLine(lines);
                previous_type = CommandType::DEF_IND;

                auto* cd = dynamic_cast<CommandDef*>(command.get());
                lines.push_back(_command2String(cd) + "\n");
                break;
            }
            case CommandType::BIND_TERM: {
                if (_addSkipLine(previous_type)) addSkipLine(lines);
                previous_type = CommandType::BIND_TERM;

                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                if (cb->is_rec) {
                    auto res = funcDef2OutputResult(cb->term.get(), "=", is_highlight);
                    res.pushStart("fun " + cb->name); res.appendLast(";");
                    lines.push_back(res.toString());
                } else {
                    auto res = term2OutputResult(cb->term.get(), is_highlight);
                    res.pushStart(cb->name + " = "); res.appendLast(";");
                    lines.push_back(res.toString());
                }
                break;
            }
            case CommandType::EVAL: {
                if (_addSkipLine(previous_type)) addSkipLine(lines);
                previous_type = CommandType::EVAL;
                auto* ce = dynamic_cast<CommandEval*>(command.get());
                lines.push_back(term2OutputResult(ce->term.get(), is_highlight).toString());
            }
        }
    }

    if (path.empty()) {
        for (auto& line: lines) std::cout << line;
    } else {
        auto* f = fopen(path.c_str(), "w");
        for (auto& line: lines) {
            fprintf(f, "%s", line.c_str());
        }
    }

    /*for (auto& command: program->commands) {
        switch (command->getType()) {
            case CommandType::DECLARE: {
                auto* cd = dynamic_cast<CommandDeclare*>(command.get());
                LOG(INFO) << type2String(cd->type.get());
                break;
            }
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                LOG(INFO) << term2String(cb->term.get());
                break;
            }
            default: break;
        }
    }*/
}