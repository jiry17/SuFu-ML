//
// Created by pro on 2023/12/5.
//

#ifndef ISTOOL_CONTEXT_H
#define ISTOOL_CONTEXT_H

#include "incre_syntax.h"
#include "incre_rewriter.h"

namespace incre {
    class EnvAddress {
    public:
        std::string name;
        syntax::Binding bind;
        std::shared_ptr<EnvAddress> next;
        EnvAddress(const std::string &_name, const syntax::Binding& _bind, const std::shared_ptr<EnvAddress>& _next);
    };

    class IncreContext {
    public:
        std::shared_ptr<EnvAddress> start;
        syntax::Ty getRawType(const std::string& _name) const;
        syntax::Ty getFinalType(const std::string& _name, syntax::IncreTypeRewriter* rewriter) const;
        Data getData(const std::string& _name) const;
        bool isContain(const std::string& name);
        EnvAddress* getAddress(const std::string& name);
        void printTypes() const;
        void printDatas() const;
        IncreContext insert(const std::string& name, const syntax::Binding& binding) const;
        IncreContext(const std::shared_ptr<EnvAddress>& _start);
    };

    class IncreFullContextData {
    public:
        IncreContext ctx;
        std::unordered_map<std::string, EnvAddress*> address_map;

        IncreFullContextData(const IncreContext& _ctx, const std::unordered_map<std::string, EnvAddress*>& _address_map);
        void setGlobalInput(const std::unordered_map<std::string, Data>& global_input);
    };
    typedef std::shared_ptr<IncreFullContextData> IncreFullContext;
}

#endif //ISTOOL_CONTEXT_H
