//
// Created by pro on 2023/12/7.
//

#ifndef ISTOOL_INCRE_REWRITER_H
#define ISTOOL_INCRE_REWRITER_H

#include "incre_syntax.h"

namespace incre::syntax {
#define RegisterTypeRewriteCase(name) virtual Ty _rewrite(Ty ## name* type, const Ty& _type);
    class IncreTypeRewriter {
    protected:
        TYPE_CASE_ANALYSIS(RegisterTypeRewriteCase);
    public:
        Ty rewrite(const Ty& type);
        virtual ~IncreTypeRewriter() = default;
    };

    typedef std::function<IncreTypeRewriter*()> IncreTypeRewriterGenerator;

#define RegisterTermRewriteCase(name) virtual Term _rewrite(Tm ## name* term, const Term& _term);
    class IncreTermRewriter {
    protected:
        TERM_CASE_ANALYSIS(RegisterTermRewriteCase);
        virtual Term postProcess(const Term& original_term, const Term& res);
    public:
        virtual Term rewrite(const Term& term);
        virtual ~IncreTermRewriter() = default;
    };

    typedef std::function<IncreTermRewriter*()> IncreTermRewriterGenerator;
}
#endif //ISTOOL_INCRE_REWRITER_H
