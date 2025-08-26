//
// Created by pro on 2022/9/25.
//

#include "istool/incre/incre_solver.h"
#include "istool/incre/language/incre_rewriter.h"
#include "glog/logging.h"
#include <iostream>

using namespace incre;
using namespace incre::syntax;
using namespace incre::analysis;

IncreSolution::IncreSolution(const syntax::TyList &_type_results, const syntax::TermList &_term_results,
                             const syntax::TermList &_repr_list): type_results(_type_results), term_results(_term_results), repr_list(_repr_list) {
}
void IncreSolution::print() const {
    for (int i = 0; i < type_results.size(); ++i) std::cout << "compress #" << i << ": " << type_results[i]->toString() << std::endl;
    for (int i = 0; i < repr_list.size(); ++i) std::cout << "repr #" << i << ": " << repr_list[i]->toString() << std::endl;
    for (int i = 0; i < term_results.size(); ++i) {
        std::cout << "pass #" << i << ": " << term_results[i]->toString() << "\n";
        // incre::printTerm(align_list[i]); std::cout << std::endl;
    }
}
IncreSolver::IncreSolver(const IncreInfo& _info): info(_info) {}

namespace {
    class _TypeRewriterWithSolution: public IncreTypeRewriter {
        TyList type_results;
    public:
        _TypeRewriterWithSolution(const TyList& _type_results): type_results(_type_results) {}
    protected:
        virtual Ty _rewrite(TyCompress* type, const Ty& _type) {
            auto* labeled_ty = dynamic_cast<TyLabeledCompress*>(type);
            if (!labeled_ty) LOG(FATAL) << "Expected TyLabeledCompress, but got " << type->toString();
            return type_results[labeled_ty->id];
        }
    };

    class _TermRewriterWithSolution: public IncreTermRewriter {
        TermList term_results;
        bool is_keep_rewrite;
    public:
        _TermRewriterWithSolution(const TermList& _term_results, bool _is_keep_rewrite):
            term_results(_term_results), is_keep_rewrite(_is_keep_rewrite) {
        }
    protected:
        virtual Term _rewrite(TmRewrite* term, const Term& _term) {
            auto* labeled_term = dynamic_cast<TmLabeledRewrite*>(term);
            if (!labeled_term) LOG(FATAL) << "Expected TmLabeledRewrite, but got " << term->toString();
            auto res = term_results[labeled_term->id];
            if (is_keep_rewrite) return std::make_shared<TmRewrite>(res); else return res;
        }
        virtual Term _rewrite(TmLabel* term, const Term& _term) {
            LOG(FATAL) << "Unexpected TmLabel " << term->toString();
        }
        virtual Term _rewrite(TmUnlabel* term, const Term& _term) {
            LOG(FATAL) << "Unexpected TmUnlabel " << term->toString();
        }
    };
}

IncreProgram incre::rewriteWithIncreSolution(IncreProgramData *program, const IncreSolution &solution, bool is_keep_rewrite) {
    auto* type_rewriter = new _TypeRewriterWithSolution(solution.type_results);
    auto* term_rewriter = new _TermRewriterWithSolution(solution.term_results, is_keep_rewrite);
    auto* rewriter = new IncreProgramRewriter(type_rewriter, term_rewriter);
    rewriter->walkThrough(program); auto res = rewriter->res;
    delete type_rewriter; delete term_rewriter; delete rewriter;
    return res;
}