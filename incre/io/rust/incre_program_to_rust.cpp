//
// Created by pro on 2025/9/9.
//

#include "istool/incre/io/incre_to_rust.h"
#include "istool/incre/language/incre_types.h"
#include "glog/logging.h"

using namespace incre::syntax;
using namespace incre::rust;
using namespace incre::types;

namespace {
    void processCommand(incre::IncreProgramWalker* walker, const incre::Command& command) {
        auto program = new incre::IncreProgramData({command}, {});
        walker->walkThrough(program);
        delete program;
    }
}

incre::rust::TranslationError::TranslationError(const std::string &_info, const std::string &_position):
    message(std::format("Error: {}, Position: {}", _info, _position)) {
}

const char *TranslationError::what() const noexcept {
    return message.c_str();
}


Ty util::unfoldBoundVariable(const syntax::Ty &raw_type) {
    IncreTypeRewriter rewriter;
    return rewriter.rewrite(raw_type);
}

std::string util::getAuxFuncName(bool is_move) {
    static int index = 0;
    auto name = "aux" + std::to_string(index);
    if (is_move) index += 1;
    return name;
}

util::RustContextEntry::RustContextEntry(const std::string &_name, const std::string &_expr,
                                         const std::shared_ptr<RustContextEntry> &_entry):
                                         name(_name), expr(_expr), next_entry(_entry) {
}

util::RustContext util::RustContext::insert(const std::string &_name, const std::string &_expr) const {
    auto new_entry = std::make_shared<RustContextEntry>(_name, _expr, start);
    return {new_entry};
}

util::RustContextEntry *util::RustContext::lookup(const std::string &name, bool is_strict) const {
    for (auto current = start; current; current = current->next_entry) {
        if (current->name == name) return current.get();
    }
    if (is_strict) LOG(FATAL) << "unknown variable " << name;
    return nullptr;
}

util::RustContext::RustContext(const std::shared_ptr<RustContextEntry> &_start): start(_start) {
}

namespace {
    std::string getAuxMatchVar() {
        static int index = 0;
        return std::format("m{}", index);
    }

    std::pair<Pattern, Term> _fillPattern(PatternData* pattern) {
        switch (pattern->getType()) {
            case PatternType::UNDERSCORE: {
                auto name = getAuxMatchVar();
                return {std::make_shared<PtVar>(name, nullptr), std::make_shared<TmVar>(name)};
            }
            case PatternType::VAR: {
                auto* pv = dynamic_cast<PtVar*>(pattern);
                if (pv->body) {
                    auto [res_pattern, res_term] = _fillPattern(pv->body.get());
                    return {std::make_shared<PtVar>(pv->name, res_pattern), res_term};
                } else {
                    return {std::make_shared<PtVar>(pv->name, nullptr), std::make_shared<TmVar>(pv->name)};
                }
            }
            case PatternType::TUPLE: {
                auto* pt = dynamic_cast<PtTuple*>(pattern);
                std::vector<Pattern> sub_patterns; sub_patterns.reserve(pt->fields.size());
                std::vector<Term> sub_terms; sub_terms.reserve(pt->fields.size());
                for (auto& field: pt->fields) {
                    auto [sub_pattern, sub_term] = _fillPattern(field.get());
                    sub_patterns.push_back(sub_pattern);
                    sub_terms.push_back(sub_term);
                }
                return {std::make_shared<PtTuple>(sub_patterns), std::make_shared<TmTuple>(sub_terms)};
            }
            case PatternType::CONS: {
                auto* pt = dynamic_cast<PtCons*>(pattern);
                auto [sub_pattern, sub_term] = _fillPattern(pt->body.get());
                return {std::make_shared<PtCons>(pt->name, sub_pattern), std::make_shared<TmCons>(pt->name, sub_term)};
            }
        }
    }

    MatchCase _unfoldVarBinding(const Pattern& pattern, const Term& term) {
        auto* pv = dynamic_cast<PtVar*>(pattern.get());
        if (pv && pv->body) {
            auto [inner, def] = _fillPattern(pv->body.get());
            return _unfoldVarBinding(inner, std::make_shared<TmLet>(pv->name, false, def, term));
        } else {
            return {pattern, term};
        }
    }

    bool _isPatternMatch(PatternData* x, PatternData* y) {
        if (x->getType() != y->getType()) return false;
        if (x->getType() == PatternType::TUPLE) return true;
        auto* x_cons = dynamic_cast<PtCons*>(x);
        auto* y_cons = dynamic_cast<PtCons*>(y);
        assert(x_cons && y_cons);
        return x_cons->name == y_cons->name;
    }

    MatchCase _unionConsGroup(const MatchCaseList& cases) {
        auto cons_name = dynamic_cast<PtCons*>(cases[0].first.get())->name;
        auto var = getAuxMatchVar();
        auto merged_pattern = std::make_shared<PtCons>(cons_name, std::make_shared<PtVar>(var, nullptr));

        MatchCaseList inner_cases;
        for (auto& [pt, term]: cases) {
            auto* pc = dynamic_cast<PtCons*>(pt.get());
            assert(pc->name == cons_name);
            inner_cases.emplace_back(pc->body, term);
        }

        if (inner_cases.size() == 1) {
            auto inner_type = inner_cases[0].first->getType();
            if (inner_type == PatternType::UNDERSCORE || inner_type == PatternType::VAR) {
                return cases[0];
            }
        }

        auto var_term = std::make_shared<TmVar>(var);
        auto inner_term = std::make_shared<TmMatch>(var_term, inner_cases);
        return {merged_pattern, inner_term};
    }
    MatchCase _unionTupleGroup(const MatchCaseList& cases, TermData* _term) {
        if (cases.size() > 1) {
            throw TranslationError("cannot support multi-case split on tuples", _term->toString());
        }
        auto& [pattern, inner_term] = cases[0];
        auto* pt = dynamic_cast<PtTuple*>(pattern.get());
        int size = int(pt->fields.size());
        std::vector<Pattern> top_patterns(size);

        Term res_term = inner_term;
        for (int index = size - 1; index >= 0; --index) {
            auto field = pt->fields[index];
            switch (field->getType()) {
                case PatternType::UNDERSCORE: {
                    top_patterns[index] = field; break;
                }
                case PatternType::VAR: {
                    auto* pv = dynamic_cast<PtVar*>(field.get());
                    if (pv->body) {
                        top_patterns[index] = std::make_shared<PtVar>(pv->name, nullptr);
                        MatchCase single_case(pv->body, res_term);
                        res_term = std::make_shared<TmMatch>(std::make_shared<TmVar>(pv->name), (MatchCaseList){single_case});
                    } else {
                        top_patterns[index] = field;
                    }
                    break;
                }
                case PatternType::TUPLE:
                case PatternType::CONS: {
                    auto var = getAuxMatchVar();
                    top_patterns[index] = std::make_shared<PtVar>(var, nullptr);
                    MatchCase single_case(field, res_term);
                    res_term = std::make_shared<TmMatch>(std::make_shared<TmVar>(var), (MatchCaseList){single_case});
                }
            }
        }
        return {std::make_shared<PtTuple>(top_patterns), res_term};
    }

    class MatchTermNormalizer: public IncreTermRewriter {
    protected:
        Term _rewrite(TmMatch* term, const Term& _term) override {
            MatchCaseList cases; cases.reserve(term->cases.size());
            for (auto& [pattern, sub_term]: term->cases) {
                cases.push_back(_unfoldVarBinding(pattern, sub_term));
            }

            std::vector<MatchCaseList> groups;
            for (auto& [pattern, sub_term]: cases) {
                auto pattern_type = pattern->getType();
                if (pattern_type == PatternType::UNDERSCORE || pattern_type == PatternType::VAR) {
                    groups.push_back({{pattern, sub_term}});
                    break;
                }
                bool is_unique = true;
                for (auto& group: groups) {
                    if (_isPatternMatch(group[0].first.get(), pattern.get())) {
                        group.emplace_back(pattern, sub_term);
                        is_unique = false; break;
                    }
                }
                if (is_unique) groups.push_back({{pattern, sub_term}});
            }

            MatchCaseList result;
            for (auto& group: groups) {
                auto group_type = group[0].first->getType();
                if (group_type == PatternType::TUPLE) {
                    result.push_back(_unionTupleGroup(group, term));
                } else if (group_type == PatternType::CONS) {
                    result.push_back(_unionConsGroup(group));
                } else {
                    assert(group.size() == 1);
                    result.push_back(group[0]);
                }
            }

            // LOG(INFO) << "raw result for " << term->toString();
            // for (auto& [pt, sub_term]: result) LOG(INFO) << pt->toString() << " @@ " << sub_term->toString();

            for (auto& [_, sub_term]: result) sub_term = rewrite(sub_term);
            auto def = rewrite(term->def);
            return std::make_shared<TmMatch>(def, result);
        }
    };
}

void incre::rust::program2Rust(std::ostream &out, incre::IncreProgramData *program) {
    auto* walker = new DefaultContextBuilder(nullptr, new DefaultIncreTypeChecker());
    util::RustContext rust_ctx;
    out << "use std::rc::Rc;" << std::endl;

    // adhoc process for TermDeclare
    std::unordered_set<std::string> defined_names;
    for (auto& command: program->commands) {
        if (command->getType() == CommandType::BIND_TERM) {
            defined_names.insert(command->name);
        }
    }

    for (auto& command: program->commands) {
        if (command->getType() != CommandType::DECLARE || defined_names.contains(command->name)) continue;
        auto* cd = dynamic_cast<CommandDeclare*>(command.get());
        Term default_term;
        if (cd->type->getType() == TypeType::INT) {
            default_term = std::make_shared<TmValue>(BuildData(Int, 0));
        } else if (cd->type->getType() == TypeType::BOOL) {
            default_term = std::make_shared<TmValue>(BuildData(Bool, false));
        } else {
            LOG(FATAL) << "unexpected type " << cd->type->toString();
        }
        command = std::make_shared<CommandBindTerm>(command->name, false, default_term, command->decos, command->source);
    }

    for (auto& command: program->commands) {
        switch (command->getType()) {
            case CommandType::EVAL: break;
            case CommandType::DEF_IND: {
                auto* ci = dynamic_cast<CommandDef*>(command.get());
                util::indDef2Rust(out, ci);
                for (auto& [cons_name, _]: ci->cons_list) {
                    rust_ctx = rust_ctx.insert(cons_name, std::format("{}::{}", ci->name, cons_name));
                }
                break;
            }
            case CommandType::DECLARE: break;
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                if (cb->term->getType() == TermType::VALUE) {
                    auto value = dynamic_cast<TmValue*>(cb->term.get());
                    out << "static " << cb->name << ": ";
                    if (dynamic_cast<incre::semantics::VInt*>(value->v.get())) {
                        out << "i32 = " << value->v.toString() << ";\n";
                    } else if (dynamic_cast<incre::semantics::VBool*>(value->v.get())) {
                        out << "bool = " << (value->v.isTrue() ? "true" : "false") << ";\n";
                    } else {
                        LOG(FATAL) << "unknown value " << value->v.toString();
                    }
                    rust_ctx = rust_ctx.insert(cb->name, std::format("Rc::new({})", cb->name));
                } else {
                    assert(cb->term->getType() == TermType::FUNC);

                    MatchTermNormalizer rewriter;
                    auto new_term = rewriter.rewrite(cb->term);
                    // LOG(INFO) << "init term " << cb->term->toString();
                    // LOG(INFO) << "normalized term " << new_term->toString();
                    auto [new_context, related_functions] = util::function2Rust(new_term, walker->ctx, command->name,
                                                                                rust_ctx, walker->getTypeChecker());

                    for (auto &function_result: related_functions) {
                        out << function_result << "\n";
                    }
                    rust_ctx = new_context;
                }
            }
        }

        processCommand(walker, command);
    }
}