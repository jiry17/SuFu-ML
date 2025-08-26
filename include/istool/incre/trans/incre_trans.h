//
// Created by pro on 2022/9/25.
//

#ifndef ISTOOL_TANS_H
#define ISTOOL_TANS_H

#include "istool/incre/language/incre_syntax.h"

namespace incre::trans {
    class TIncreInd: public Type {
    public:
        std::string name;
        TypeList params;
        TIncreInd(const std::string& _name, const TypeList& _params);
        virtual std::string getName();
        virtual bool equal(Type* type);
        virtual std::string getBaseName();
        virtual TypeList getParams();
        virtual PType clone(const TypeList& params);
        virtual ~TIncreInd() = default;
    };

    class TCompress: public Type {
    public:
        PType body;
        TCompress(const PType& _body);
        virtual std::string getName();
        virtual bool equal(Type* type);
        virtual std::string getBaseName();
        virtual TypeList getParams();
        virtual PType clone(const TypeList& params);
        virtual ~TCompress() = default;
    };

    class TLabeledCompress: public TCompress {
    public:
        int id;
        TLabeledCompress(const PType& _body, int _id);
        virtual bool equal(Type* type);
        virtual std::string getBaseName();
        virtual PType clone(const TypeList& params);
        virtual ~TLabeledCompress() = default;
    };


    PType typeFromIncre(syntax::TypeData* type);
    syntax::Ty typeToIncre(Type* type);
    std::string operatorNameToIncre(const std::string& name);
}

#endif //ISTOOL_TANS_H
