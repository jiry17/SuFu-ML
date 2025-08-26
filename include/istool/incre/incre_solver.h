//
// Created by pro on 2022/9/25.
//

#ifndef ISTOOL_INCRE_SOLVER_H
#define ISTOOL_INCRE_SOLVER_H

#include "analysis/incre_instru_info.h"

namespace incre {

    struct IncreSolution {
        syntax::TyList type_results;
        syntax::TermList term_results;
        syntax::TermList repr_list;
        IncreSolution(const syntax::TyList& _type_results, const syntax::TermList& _term_results, const syntax::TermList& _repr_list);
        IncreSolution() = default;
        void print() const;
    };

    class IncreSolver {
    public:
        analysis::IncreInfo info;
        IncreSolver(const analysis::IncreInfo& _info);
        virtual IncreSolution solve() = 0;
        virtual ~IncreSolver() = default;
    };

    IncreProgram rewriteWithIncreSolution(IncreProgramData* program, const IncreSolution& solution, bool is_keep_rewrite);
}

#endif //ISTOOL_INCRE_SOLVER_H
