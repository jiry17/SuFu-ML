//
// Created by pro on 2024/3/30.
//

#ifndef ISTOOL_INCER_UTIL_H
#define ISTOOL_INCER_UTIL_H

#include "incre_program.h"

namespace incre::util {
    syntax::Term extractIrrelevantSubTerms(const syntax::Term& term);
    IncreProgram extractIrrelevantSubTermsForProgram(IncreProgramData* program);

    std::vector<std::pair<std::vector<std::string>, syntax::Term>> getSubTermWithLocalVars(syntax::TermData* term);

    syntax::Term removeTrivialLet(const syntax::Term& term);
    IncreProgram removeTrivialLetForProgram(IncreProgramData* program);

    syntax::Ty removeBoundedVar(const syntax::Ty& type);

    std::vector<std::string> getFreeVariables(syntax::TermData* term);
}

#endif //ISTOOL_INCER_UTIL_H
