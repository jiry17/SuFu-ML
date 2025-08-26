//
// Created by pro on 2022/9/17.
//

#ifndef ISTOOL_INCRE_JSON_H
#define ISTOOL_INCRE_JSON_H

#include <json/json.h>
#include "istool/incre/language/incre_program.h"


namespace incre::io {

    class IncreParseError: public std::exception {
    private:
        std::string message;
    public:
        IncreParseError(const std::string _message);
        virtual const char* what() const noexcept;
    };

    syntax::TypeType string2TypeType(const std::string& type);
    std::string typeType2String(syntax::TypeType type);

    syntax::TermType string2TermType(const std::string& type);
    syntax::PatternType string2PatternType(const std::string& type);
    IncreConfig string2IncreConfig(const std::string& name);
    CommandDecorate string2Decorate(const std::string& name);

    syntax::Ty json2ty(const Json::Value& node);
    syntax::Pattern json2pattern(const Json::Value& node);
    syntax::Term json2term(const Json::Value& node);

    IncreProgram json2program(const Json::Value& node);
    IncreProgram json2program(const std::string& path);
    IncreProgram parseFromF(const std::string& path);

    Json::Value program2json(IncreProgramData* program);
    void printProgram2F(const std::string& path, IncreProgramData* program);
}

#endif //ISTOOL_INCRE_JSON_H
