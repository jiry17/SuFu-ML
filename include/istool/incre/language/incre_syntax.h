//
// Created by pro on 2023/12/4.
//

#ifndef ISTOOL_INCRE_SYNTAX_H
#define ISTOOL_INCRE_SYNTAX_H

#include "istool/basic/data.h"
#include "istool/basic/env.h"
#include <variant>
#include <unordered_set>

namespace incre::syntax {
    enum class TypeType {
        VAR, UNIT, BOOL, INT, POLY, ARR, TUPLE, IND, COMPRESS
    };

    class TypeData {
    public:
        TypeType type;
        TypeData(const TypeType& _type);
        TypeType getType() const;
        virtual ~TypeData() = default;
        virtual std::string toString() const = 0;
    };
    typedef std::shared_ptr<TypeData> Ty;
    typedef std::vector<Ty> TyList;

    enum VarRange {
        SCALAR = 0, BASE = 1, ANY = 2
    };

    typedef std::variant<Ty, std::tuple<int, int, VarRange>> TypeVarInfo;

    class TyVar: public TypeData {
    public:
        TypeVarInfo info;
        TyVar(const TypeVarInfo& _info);
        std::tuple<int, int, VarRange> get_var_info() const;
        void intersectWith(const VarRange& range);
        Ty get_bound_type() const;
        bool is_bounded() const;
        virtual std::string toString() const;
    };

    class TyBool: public TypeData {
    public:
        TyBool();
        virtual std::string toString() const;
    };

    class TyInt: public TypeData {
    public:
        TyInt();
        virtual std::string toString() const;
    };

    class TyUnit: public TypeData {
    public:
        TyUnit();
        virtual std::string toString() const;
    };

    class TyArr: public TypeData {
    public:
        Ty inp, oup;
        TyArr(const Ty& _func, const Ty& _param);
        virtual std::string toString() const;
    };

    class TyPoly: public TypeData {
    public:
        std::vector<int> var_list;
        Ty body;
        TyPoly(const std::vector<int>& _var_list, const Ty& _body);
        virtual std::string toString() const;
    };

    class TyPolyWithName: public TyPoly {
    public:
        std::vector<std::string> name_list;
        TyPolyWithName(const std::vector<std::string>& _name_list, const std::vector<int>& indices, const Ty& _body);
    };

    class TyTuple: public TypeData {
    public:
        TyList fields;
        TyTuple(const TyList& _fields);
        virtual std::string toString() const;
    };

    class TyInd: public TypeData {
    public:
        std::string name;
        TyList param_list;
        TyInd(const std::string& _name, const TyList& _param_list);
        virtual std::string toString() const;
    };

    class TyCompress: public TypeData {
    public:
        Ty body;
        TyCompress(const Ty& _body);
        virtual std::string toString() const;
    };

    TyList getSubTypes(TypeData* x);
}

namespace incre::syntax {
    enum class PatternType {
        UNDERSCORE, VAR, TUPLE, CONS
    };

    std::string patternType2String(PatternType t);

    class PatternData {
    public:
        PatternType type;
        PatternType getType() const;
        PatternData(const PatternType& _type);
        virtual ~PatternData() = default;
        virtual std::string toString() const = 0;
    };
    typedef std::shared_ptr<PatternData> Pattern;
    typedef std::vector<Pattern> PatternList;

    class PtUnderScore: public PatternData {
    public:
        PtUnderScore();
        virtual std::string toString() const;
    };

    class PtVar: public PatternData {
    public:
        std::string name;
        Pattern body;
        PtVar(const std::string& _name, const Pattern& _body);
        virtual std::string toString() const;
    };

    class PtTuple: public PatternData {
    public:
        PatternList fields;
        PtTuple(const PatternList& _fields);
        virtual std::string toString() const;
    };

    class PtCons: public PatternData {
    public:
        std::string name;
        Pattern body;
        PtCons(const std::string& _name, const Pattern& _body);
        virtual std::string toString() const;
    };

    std::vector<std::string> getVarsInPattern(PatternData* pattern);
}

namespace incre::syntax {
    enum class TermType {
        VALUE, IF, VAR, PRIMARY, APP, TUPLE, PROJ, FUNC, LET, MATCH, CONS, LABEL, UNLABEL, REWRITE
    };

    std::string termType2String(TermType type);

    class TermData {
    public:
        TermType term_type;
        TermType getType() const;
        TermData(const TermType& _term_type);
        virtual ~TermData() = default;
        virtual std::string toString() const = 0;
    };
    typedef std::shared_ptr<TermData> Term;
    typedef std::vector<Term> TermList;

    class TmValue: public TermData {
    public:
        Data v;
        TmValue(const Data& _v);
        virtual std::string toString() const;
    };

    class TmIf: public TermData {
    public:
        Term c, t, f;
        TmIf(const Term& _c, const Term& _t, const Term& _f);
        virtual std::string toString() const;
    };

    class TmVar: public TermData {
    public:
        std::string name;
        TmVar(const std::string& _name);
        virtual std::string toString() const;
    };

    class TmPrimary: public TermData {
    public:
        std::string op_name;
        TermList params;
        TmPrimary(const std::string& _op_name, const TermList& _params);
        virtual std::string toString() const;
    };

    class TmApp: public TermData {
    public:
        Term func, param;
        TmApp(const Term& _func, const Term& _param);
        virtual std::string toString() const;
    };

    class TmFunc: public TermData {
    public:
        std::string name;
        Term body;
        TmFunc(const std::string & _name, const Term& _body);
        virtual std::string toString() const;
    };

    class TmLet: public TermData {
    public:
        std::string name;
        bool is_rec;
        Term def, body;
        TmLet(const std::string& _name, bool _is_rec, const Term& _def, const Term& _body);
        virtual std::string toString() const;
    };

    class TmTuple: public TermData {
    public:
        TermList fields;
        TmTuple(const TermList& _fields);
        virtual std::string toString() const;
    };

    class TmProj: public TermData {
    public:
        Term body; int id, size;
        TmProj(const Term& _body, int _id, int _size);
        virtual std::string toString() const;
    };

    typedef std::pair<Pattern, Term> MatchCase;
    typedef std::vector<MatchCase> MatchCaseList;

    class TmMatch: public TermData {
    public:
        Term def;
        MatchCaseList cases;
        TmMatch(const Term& _def, const MatchCaseList& _cases);
        virtual std::string toString() const;
    };

    class TmCons: public TermData {
    public:
        std::string cons_name;
        Term body;
        TmCons(const std::string& _cons_name, const Term& body);
        virtual std::string toString() const;
    };

    class TmLabel: public TermData {
    public:
        Term body;
        TmLabel(const Term& _body);
        virtual std::string toString() const;
    };

    class TmUnlabel: public TermData {
    public:
        Term body;
        TmUnlabel(const Term& _body);
        virtual std::string toString() const;
    };

    class TmRewrite: public TermData {
    public:
        Term body;
        TmRewrite(const Term& _body);
        virtual std::string toString() const;
    };

    TermList getSubTerms(TermData* term);
}

namespace incre::syntax {
    struct Binding {
        bool is_cons;
        Ty type; Data data;
        Binding(bool _is_cons, const Ty& _type, const Data& _data);
        Binding(const Ty& _type);
        Binding(const Data& _data);
        Ty getType() const;
        Data getData() const;
    };
}

#define TYPE_TOKEN_Var VAR
#define TYPE_TOKEN_Unit UNIT
#define TYPE_TOKEN_Bool BOOL
#define TYPE_TOKEN_Int INT
#define TYPE_TOKEN_Poly POLY
#define TYPE_TOKEN_Arr ARR
#define TYPE_TOKEN_Tuple TUPLE
#define TYPE_TOKEN_Ind IND
#define TYPE_TOKEN_Compress COMPRESS

#define TYPE_CASE_ANALYSIS(macro) \
macro(Var); macro(Unit); macro(Bool); macro(Int); \
macro(Poly); macro(Arr); macro(Tuple); macro(Ind); macro(Compress)

#define TERM_TOKEN_Value VALUE
#define TERM_TOKEN_App APP
#define TERM_TOKEN_Cons CONS
#define TERM_TOKEN_Func FUNC
#define TERM_TOKEN_If IF
#define TERM_TOKEN_Label LABEL
#define TERM_TOKEN_Unlabel UNLABEL
#define TERM_TOKEN_Rewrite REWRITE
#define TERM_TOKEN_Let LET
#define TERM_TOKEN_Primary PRIMARY
#define TERM_TOKEN_Match MATCH
#define TERM_TOKEN_Var VAR
#define TERM_TOKEN_Tuple TUPLE
#define TERM_TOKEN_Proj PROJ

#define TERM_CASE_ANALYSIS(macro) \
macro(Value); macro(App); macro(Cons); macro(Func); macro(If); \
macro(Label); macro(Unlabel); macro(Rewrite); macro(Let); macro(Primary); \
macro(Match); macro(Var); macro(Tuple); macro(Proj);

#endif //ISTOOL_INCRE_SYNTAX_H
