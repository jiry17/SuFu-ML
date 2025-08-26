//
// Created by pro on 2023/12/10.
//

#ifndef ISTOOL_INCRE_INSTRU_TYPES_H
#define ISTOOL_INCRE_INSTRU_TYPES_H

#include "istool/incre/language/incre_syntax.h"
#include "istool/incre/language/incre_types.h"
#include "istool/incre/language/incre_semantics.h"
#include "istool/incre/language/incre_rewriter.h"

namespace incre::syntax {
    class TyLabeledCompress: public TyCompress {
    public:
        int id;
        TyLabeledCompress(const Ty& _ty, int _id);
        virtual std::string toString() const;
        virtual ~TyLabeledCompress() = default;
    };

    class TmLabeledLabel: public TmLabel {
    public:
        int id;
        TmLabeledLabel(const Term& _content, int _id);
        virtual std::string toString() const;
        virtual ~TmLabeledLabel() = default;
    };

    class TmLabeledUnlabel: public TmUnlabel {
    public:
        int id;
        TmLabeledUnlabel(const Term& _content, int _id);
        virtual std::string toString() const;
        virtual ~TmLabeledUnlabel() = default;
    };

    class TmLabeledRewrite: public TmRewrite {
    public:
        int id;
        TmLabeledRewrite(const Term& _content, int _id);
        virtual std::string toString() const;
        virtual ~TmLabeledRewrite() = default;
    };

    class IncreLabeledTypeRewriter: public IncreTypeRewriter {
    protected:
        RegisterTypeRewriteCase(Compress);
    };
}

namespace incre::semantics {
    class VLabeledCompress: public VCompress {
    public:
        int id;
        VLabeledCompress(const Data& _v, int _id);
        virtual std::string toString() const;
        virtual ~VLabeledCompress() = default;
    };

    class IncreLabeledEvaluator: public DefaultEvaluator {
    protected:
        RegisterEvaluateCase(Label);
    };
}

namespace incre::types {
    class IncreLabeledTypeChecker: public types::DefaultIncreTypeChecker {
    protected:
        RegisterUnifyRule(Compress);
        RegisterTypingRule(Label);
        RegisterTypingRule(Unlabel);
        virtual syntax::Ty instantiate(const syntax::Ty& x);
    };
}

#endif //ISTOOL_INCRE_INSTRU_TYPES_H
