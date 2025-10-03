//
// Created by pro on 2025/9/8.
//

#ifndef ISTOOL_INCRE_TO_RUST_H
#define ISTOOL_INCRE_TO_RUST_H

#include "istool/incre/language/incre_program.h"
#include <iostream>

namespace incre::rust {
    class TranslationError: std::exception {
    public:
        std::string message;

        TranslationError(const std::string& _info, const std::string& _position);
        virtual const char* what() const noexcept;
    };

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

        std::string getAuxFuncName(bool is_move=true);

        std::string funcSignature2String(const TypeSignature &signature, const std::string &func_name);

        struct RustContextEntry {
            std::string name, expr;
            std::shared_ptr<RustContextEntry> next_entry;
            RustContextEntry(const std::string& _name, const std::string& _expr, const std::shared_ptr<RustContextEntry>& _entry);
        };

        struct RustContext {
            std::shared_ptr<RustContextEntry> start;

            RustContextEntry* lookup(const std::string& name, bool is_strict=true) const;
            RustContext insert(const std::string& _name, const std::string& _expr) const;
            RustContext(const std::shared_ptr<RustContextEntry>& _start = nullptr);
        };

        std::pair<RustContext, std::vector<std::string>>
        function2Rust(const syntax::Term& term, const IncreContext& global, const std::string& func_name,
                      const RustContext& rust_ctx, incre::types::IncreTypeChecker* checker);
    }

    void program2Rust(std::ostream& out, IncreProgramData* program);
}

#endif //ISTOOL_INCRE_TO_RUST_H
