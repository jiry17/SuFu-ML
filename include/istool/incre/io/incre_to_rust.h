//
// Created by pro on 2025/9/8.
//

#ifndef ISTOOL_INCRE_TO_RUST_H
#define ISTOOL_INCRE_TO_RUST_H

#include "istool/incre/language/incre_program.h"
#include <iostream>

namespace incre::rust {
    namespace util {
        struct TypeSignature {
            std::vector<std::pair<std::string, std::string>> params;
            std::vector<std::pair<std::string, std::string>> inp_list;
            std::string oup;
        };

        TypeSignature buildTypeSignature(const std::vector<std::string> &inp_names, syntax::TypeData *func_type);

        std::string wrapWithRc(const std::string &type);

        std::string indent(int num);

        void indDef2Rust(std::ostream &out, CommandDef *command);

        syntax::Ty unfoldBoundVariable(const syntax::Ty &raw_type);

        std::string getAuxFuncName();

        std::string funcSignature2String(const TypeSignature &signature, const std::string &func_name);

        struct RecursiveFunctionInfo {
            std::string name, global_params;

            RecursiveFunctionInfo(const std::string &name, const TypeSignature &info);

            std::string buildFunctionCall(const std::string &input);
        };

        std::pair<std::string, std::vector<std::string>>
        function2Rust(const syntax::Term& term, const IncreContext& global, const std::optional<std::string>& _func_name,
                      const std::unordered_map<std::string, RecursiveFunctionInfo>& func_infos)

        std::pair<RecursiveFunctionInfo, std::vector<std::string>>
        function2Rust(const syntax::Term &term, const IncreContext &ctx,
                      const std::optional<std::string> &name,
                      const std::unordered_map<std::string, RecursiveFunctionInfo> &rec_infos);
    }

    void program2Rust(std::ostream& out, IncreProgramData* program);
}

#endif //ISTOOL_INCRE_TO_RUST_H
