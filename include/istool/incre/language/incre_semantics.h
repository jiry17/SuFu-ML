//
// Created by pro on 2023/12/5.
//

#ifndef ISTOOL_INCRE_SEMANTICS_H
#define ISTOOL_INCRE_SEMANTICS_H

#include "incre_context.h"
#include "istool/sygus/theory/basic/clia/clia.h"
#include "istool/ext/deepcoder/data_value.h"

namespace incre::semantics {

    class IncreSemanticsError: public std::exception {
    private:
        std::string message;
    public:
        IncreSemanticsError(const std::string _message);
        virtual const char* what() const noexcept;
    };

    typedef IntValue VInt;
    typedef BoolValue VBool;
    typedef ProductValue VTuple;

    class VUnit: public Value {
    public:
        virtual std::string toString() const;
        virtual bool equal(Value* value) const;
    };

    class VClosure: public Value {
    public:
        IncreContext context;
        std::string name; syntax::Term body;
        VClosure(const IncreContext& _context, const std::string& _name, const syntax::Term& _body);
        VClosure(const std::tuple<IncreContext, std::string, syntax::Term>& _content);
        virtual std::string toString() const;
        virtual bool equal(Value* value) const;
    };

    class VInd: public Value {
    public:
        std::string name;
        Data body;
        VInd(const std::string& _name, const Data& _body);
        VInd(const std::pair<std::string, Data>& _content);
        virtual std::string toString() const;
        virtual bool equal(Value* value) const;
    };

    class VCompress: public Value {
    public:
        Data body;
        VCompress(const Data& _body);
        virtual std::string toString() const;
        virtual bool equal(Value* value) const;
    };

#define RegisterAbstractEvaluateCase(name) virtual Data _evaluate(syntax::Tm ## name* term, const IncreContext& ctx) = 0
    class IncreEvaluator {
    protected:
        TERM_CASE_ANALYSIS(RegisterAbstractEvaluateCase);
        virtual void preProcess(syntax::TermData* term, const IncreContext& ctx) = 0;
        virtual void postProcess(syntax::TermData* term, const IncreContext& ctx, const Data& res) = 0;
    public:
        virtual ~IncreEvaluator() = default;
        Data evaluate(syntax::TermData* term, const IncreContext& ctx);
    };

#define RegisterEvaluateCase(name) virtual Data _evaluate(syntax::Tm ## name* term, const IncreContext& ctx)
    class DefaultEvaluator: public IncreEvaluator {
    protected:
        TERM_CASE_ANALYSIS(RegisterEvaluateCase);
        virtual void preProcess(syntax::TermData* term, const IncreContext& ctx);
        virtual void postProcess(syntax::TermData* term, const IncreContext& ctx, const Data& res);
    public:
        virtual ~DefaultEvaluator() = default;
    };

    typedef std::function<IncreEvaluator*()> IncreEvaluatorGenerator;

    bool isValueMatchPattern(syntax::PatternData* pattern, const Data& data);
    IncreContext bindValueWithPattern(syntax::PatternData* pattern, const Data& data, const IncreContext& ctx);
    Data invokePrimary(const std::string& name, const DataList& params);

}

#endif //ISTOOL_INCRE_SEMANTICS_H
