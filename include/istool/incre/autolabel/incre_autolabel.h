//
// Created by pro on 2023/1/22.
//

#ifndef ISTOOL_INCRE_AUTOLABEL_H
#define ISTOOL_INCRE_AUTOLABEL_H

#include "istool/incre/language/incre_types.h"
#include "istool/incre/language/incre_program.h"

namespace incre::autolabel {
    class WrappedType: public syntax::TypeData {
    public:
        z3::expr compress_label;
        syntax::Ty content;
        WrappedType(const z3::expr& _compress_label, const syntax::Ty& _content);
        virtual ~WrappedType() = default;
        virtual std::string toString() const;
    };
    typedef std::shared_ptr<WrappedType> WrappedTy;

    struct Z3LabeledVarInfo {
        int index, level;
        z3::expr scalar_cond, base_cond;
        Z3LabeledVarInfo(int _index, int _level, const z3::expr& _scalar_cond, const z3::expr& _base_cond);
    };

    typedef std::variant<syntax::Ty, Z3LabeledVarInfo> FullZ3LabeledContent;

    class Z3TyVar: public syntax::TypeData {
    public:
        FullZ3LabeledContent content;
        bool isBounded() const;
        syntax::Ty& getBindType();
        Z3LabeledVarInfo& getVarInfo();
        virtual std::string toString() const;
        Z3TyVar(const FullZ3LabeledContent & _content);
        virtual ~Z3TyVar() = default;
    };

    class Z3Context {
    public:
        std::unordered_map<syntax::TermData*, WrappedTy> type_map;
        std::unordered_map<syntax::TermData*, z3::expr> flip_map, rewrite_map;
        z3::context ctx;
        z3::expr_vector cons_list;
        z3::expr KTrue, KFalse;
        int var_index = 0;
        Z3Context();
        z3::expr newVar();
        void addCons(const z3::expr& expr);
    };

#define RegisterWrappedTypingRule(name) virtual WrappedTy _typing(syntax::Tm ## name* term, const IncreContext& ctx);
#define RegisterWrappedUnifyRule(name) virtual void _unify(syntax::Ty ## name* x, syntax::Ty ## name* y, const WrappedTy& _x, const WrappedTy& _y);

    class SymbolicIncreTypeChecker {
    protected:
        int tmp_var_id = 0, _level = 0;
        TERM_CASE_ANALYSIS(RegisterWrappedTypingRule);
        TYPE_CASE_ANALYSIS(RegisterWrappedUnifyRule);
        WrappedTy defaultWrap(const syntax::Ty& type);
        std::shared_ptr<Z3TyVar> getTmpVar(syntax::VarRange range);
        void updateTypeBeforeUnification(syntax::TypeData* x, const Z3LabeledVarInfo& info);
        std::pair<WrappedTy, IncreContext> processPattern(syntax::PatternData* pattern, const IncreContext& ctx);
    public:
        Z3Context* z3_ctx;
        // virtual WrappedTy normalize(const WrappedTy& type);
        void unify(const WrappedTy& x, const WrappedTy& y);
        WrappedTy getTmpWrappedVar(syntax::VarRange range);
        void pushLevel(); void popLevel();
        WrappedTy typing(syntax::TermData* term, const IncreContext& ctx);
        WrappedTy instantiate(const WrappedTy& x);
        WrappedTy generalize(const WrappedTy& x);
        SymbolicIncreTypeChecker(Z3Context* _z3_ctx);
        virtual ~SymbolicIncreTypeChecker() = default;
    };

    namespace util {
        WrappedTy liftNormalType(const syntax::Ty& raw_type, Z3Context* ctx);
        void collectHardConstraints(IncreProgramData* program, SymbolicIncreTypeChecker* checker);
        z3::expr collectSoftConstraints(IncreProgramData* program, Z3Context* ctx);
        IncreProgram rewriteUseResult(IncreProgramData* program, Z3Context* checker, const z3::model& model);
    }

    IncreProgram labelProgram(const IncreProgram& program);
}

#endif //ISTOOL_INCRE_AUTOLABEL_H
