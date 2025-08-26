//
// Created by pro on 2023/12/5.
//
#include "istool/incre/language/incre_context.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;

EnvAddress::EnvAddress(const std::string &_name, const syntax::Binding &_bind,
                       const std::shared_ptr<EnvAddress> &_next):
                       name(_name), bind(_bind), next(_next) {
}
IncreContext::IncreContext(const std::shared_ptr<EnvAddress> &_start): start(_start) {}

syntax::Ty IncreContext::getRawType(const std::string &name) const {
    for (auto now = start; now; now = now->next) {
        if (now->name == name) return now->bind.getType();
    }
    LOG(FATAL) << "No type is bound to " << name;
}

syntax::Ty IncreContext::getFinalType(const std::string &_name, syntax::IncreTypeRewriter *rewriter) const {
    return rewriter->rewrite(getRawType(_name));
}

Data IncreContext::getData(const std::string &name) const {
    for (auto now = start; now; now = now->next) {
        if (now->name == name) return now->bind.getData();
    }
    LOG(FATAL) << "No data is bound to " << name;
}

#include <iostream>
void IncreContext::printTypes() const {
    std::vector<std::pair<std::string, Ty>> info_list;
    for (auto pos = start; pos; pos = pos->next) {
        info_list.emplace_back(pos->name, pos->bind.type);
    }
    std::reverse(info_list.begin(), info_list.end());
    for (auto& [name, ty]: info_list) {
        if (ty) std::cout << name << ": " << ty->toString() << std::endl;
        else std::cout << name << ": NA" << std::endl;
    }
}
void IncreContext::printDatas() const {
    std::vector<std::pair<std::string, Data>> info_list;
    for (auto pos = start; pos; pos = pos->next) {
        info_list.emplace_back(pos->name, pos->bind.data);
    }
    std::reverse(info_list.begin(), info_list.end());
    for (auto& [name, v]: info_list) {
        if (!v.isNull()) std::cout << name << ": " << v.toString() << std::endl;
        else std::cout << name << ": NA" << std::endl;
    }
}

EnvAddress *IncreContext::getAddress(const std::string &name) {
    for (auto now = start; now; now = now->next) {
        if (now->name == name) return now.get();
    }
    LOG(FATAL) << "Nothing is bound to " << name;
}

bool IncreContext::isContain(const std::string &name) {
    for (auto now = start; now; now = now->next) {
        if (now->name == name) return true;
    }
    return false;
}

IncreContext IncreContext::insert(const std::string &name, const syntax::Binding &binding) const {
    return std::make_shared<EnvAddress>(name, binding, start);
}

IncreFullContextData::IncreFullContextData(const IncreContext &_ctx,
                                           const std::unordered_map<std::string, EnvAddress *> &_address_map):
                                           ctx(_ctx), address_map(_address_map) {
}

void IncreFullContextData::setGlobalInput(const std::unordered_map<std::string, Data> &global_input) {
    if (global_input.size() != address_map.size()) {
        LOG(FATAL) << "Expect " << std::to_string(address_map.size()) << " global inputs, but received " << std::to_string(global_input.size());
    }
    for (auto& [name, value]: global_input) {
        auto it = address_map.find(name);
        if (it == address_map.end()) LOG(FATAL) << "Unknown global input " << name;
        it->second->bind.data = value;
    }
}