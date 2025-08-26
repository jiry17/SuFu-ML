//
// Created by pro on 2022/10/8.
//

#ifndef ISTOOL_INCRE_PRINTER_H
#define ISTOOL_INCRE_PRINTER_H

#include "istool/incre/language/incre_program.h"
#include "istool/incre/language/incre_semantics.h"
#include "istool/incre/language/incre_syntax.h"

namespace incre::io {

    struct OutputResult {
        std::vector<std::string> output_list;
        void addIndent(int start = 0);
        void pushStart(const std::string& s);
        void appendLast(const std::string& s);
        std::string toString() const;
        OutputResult(const std::string& s);
        OutputResult(const std::vector<std::string>& _output_list);
        OutputResult() = default;
    };

    std::string type2String(syntax::TypeData* ty);
    OutputResult term2OutputResult(syntax::TermData* term, bool is_highlight);
    OutputResult funcDef2OutputResult(syntax::TermData* term, const std::string& linker, bool is_highlight);
    std::string term2String(syntax::TermData* term, bool is_highlight);
    std::string pattern2String(syntax::PatternData* pattern);
    void printProgram(IncreProgramData* program, const std::string& path = {}, bool is_highlight = false);

    extern const int KLineWidth;
    extern const int KIndent;
}


#endif //ISTOOL_INCRE_PRINTER_H
