//
// Created by pro on 2024/3/30.
//

#include "istool/incre/language/incre_util.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::util;
using namespace incre::syntax;

namespace {
    bool _isMovable(TermData* term) {
        if (term->getType() == TermType::LABEL || term->getType() == TermType::UNLABEL) return false;
        if (term->getType() == TermType::REWRITE) return true;
        for (auto& sub_type: getSubTerms(term)) {
            if (!_isMovable(sub_type.get())) return false;
        }
        return true;
    }

    TermList _getConsideredSubTerms(TermData* term) {
        if (term->getType() == TermType::FUNC) return {};
        if (term->getType() == TermType::IF) {
            auto* ti = dynamic_cast<TmIf*>(term);
            return {ti->c};
        }
        if (term->getType() == TermType::LET) {
            auto* tl = dynamic_cast<TmLet*>(term);
            return {tl->def};
        }
        if (term->getType() == TermType::MATCH) {
            auto* tm = dynamic_cast<TmMatch*>(term);
            return {tm->def};
        }
        return getSubTerms(term);
    }

    void _collectMovableSubTerms(const Term& term, TermList& res) {
        if (getSubTerms(term.get()).empty()) return;
        if (_isMovable(term.get())) {
            res.push_back(term); return;
        }
        for (auto& sub_term: _getConsideredSubTerms(term.get())) {
            _collectMovableSubTerms(sub_term, res);
        }
    }

    class _TermReplacer: public IncreTermRewriter {
    public:
        std::unordered_map<TermData*, Term> replace_map;

        virtual Term rewrite(const Term& term) {
            auto it = replace_map.find(term.get());
            if (it != replace_map.end()) {
                return it->second;
            }
            return IncreTermRewriter::rewrite(term);
        }
        _TermReplacer(const std::unordered_map<TermData*, Term>& _replace_map): replace_map(_replace_map) {}
    };

    class _TermExtractor: public IncreTermRewriter {
    public:
        static int tmp_id;
        virtual Term rewrite(const Term& term) {
            if (term->getType() == TermType::REWRITE) {
                auto* tr = dynamic_cast<TmRewrite*>(term.get());
                TermList movable_terms;
                _collectMovableSubTerms(tr->body, movable_terms);
                std::unordered_map<TermData*, Term> replace_map;
                std::vector<std::pair<std::string, Term>> binding_list;
                for (auto& sub_term: movable_terms) {
                    auto new_name = "m" + std::to_string(tmp_id++);
                    replace_map[sub_term.get()] = std::make_shared<TmVar>(new_name);
                    binding_list.emplace_back(new_name, sub_term);
                }

                auto* replacer = new _TermReplacer(replace_map);
                auto new_term = replacer->rewrite(tr->body);
                delete replacer;
                new_term = std::make_shared<TmRewrite>(rewrite(new_term));

                std::reverse(binding_list.begin(), binding_list.end());
                for (auto& [name, def]: binding_list) {
                    new_term = std::make_shared<TmLet>(name, false, def, new_term);
                }
                return new_term;
            }
            return IncreTermRewriter::rewrite(term);
        }
    };

    int _TermExtractor::tmp_id = 0;
}

syntax::Term util::extractIrrelevantSubTerms(const syntax::Term &term) {
    auto* rewriter = new _TermExtractor();
    auto res = rewriter->rewrite(term);
    delete rewriter;
    return res;
}

namespace {
    typedef std::function<Term(const Term&)> AnonymousTermRewriter;

    IncreProgram _rewriteTermsInProgram(IncreProgramData* program, const AnonymousTermRewriter& rewriter) {
        CommandList command_list;
        for (auto& command: program->commands) {
            switch (command->getType()) {
                case CommandType::DEF_IND:
                case CommandType::DECLARE: {
                    command_list.push_back(command); break;
                }
                case CommandType::BIND_TERM: {
                    auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                    auto new_term = rewriter(cb->term);
                    auto new_command = std::make_shared<CommandBindTerm>(cb->name, cb->is_rec, new_term, cb->decos, cb->source);
                    command_list.push_back(new_command); break;
                }
                case CommandType::EVAL: {
                    auto* ce = dynamic_cast<CommandEval*>(command.get());
                    auto new_term = rewriter(ce->term);
                    auto new_command = std::make_shared<CommandEval>(ce->name, ce->term, ce->decos, ce->source);
                    command_list.push_back(new_command);
                }
            }
        }
        return std::make_shared<IncreProgramData>(command_list, program->config_map);
    }
}

IncreProgram util::extractIrrelevantSubTermsForProgram(IncreProgramData *program) {
    return _rewriteTermsInProgram(program, util::extractIrrelevantSubTerms);
}

namespace {
    typedef std::vector<std::pair<std::vector<std::string>, Term>> SubInfo;
    SubInfo _getNewLocalVars(TermData* term, std::vector<std::string>& local_vars, std::unordered_set<std::string>& free_map);

#define LocalVarsHead(name) SubInfo _getNewLocalVars(Tm ## name* term)
#define LocalVarsCase(name) case TermType::TERM_TOKEN_ ## name: return _getNewLocalVars(dynamic_cast<Tm ## name*>(term));
#define DefaultLocalVars(name) LocalVarsHead(name) { \
        SubInfo res;                                 \
        for (auto& sub_term: getSubTerms(term)) {    \
            res.emplace_back((std::vector<std::string>){}, sub_term);\
        }\
        return res;                                       \
    }

    DefaultLocalVars(Value);
    DefaultLocalVars(If);
    DefaultLocalVars(App);
    DefaultLocalVars(Tuple);
    DefaultLocalVars(Proj);
    DefaultLocalVars(Label);
    DefaultLocalVars(Unlabel);
    DefaultLocalVars(Rewrite);
    DefaultLocalVars(Primary);
    DefaultLocalVars(Cons);

    LocalVarsHead(Let) {
        if (term->is_rec) {
            return {{{term->name}, term->def}, {{term->name}, term->body}};
        } else {
            return {{{}, term->def}, {{term->name}, term->body}};
        }
    }

    LocalVarsHead(Func) {
        return {{{term->name}, term->body}};
    }

    void _collectVarsFromPattern(PatternData* pt, std::vector<std::string>& local_vars) {
        switch (pt->getType()) {
            case PatternType::UNDERSCORE: return;
            case PatternType::VAR: {
                auto* pv = dynamic_cast<PtVar*>(pt);
                local_vars.push_back(pv->name);
                if (pv->body) _collectVarsFromPattern(pv->body.get(), local_vars);
                return;
            }
            case PatternType::TUPLE: {
                auto* tuple = dynamic_cast<PtTuple*>(pt);
                for (auto& field: tuple->fields) _collectVarsFromPattern(field.get(), local_vars);
                return;
            }
            case PatternType::CONS: {
                auto* cons = dynamic_cast<PtCons*>(pt);
                _collectVarsFromPattern(cons->body.get(), local_vars);
                return;
            }
        }
    }

    LocalVarsHead(Match) {
        SubInfo res;
        res.push_back({{}, term->def});
        for (auto& [pt, case_term]: term->cases) {
            std::vector<std::string> local_vars;
            _collectVarsFromPattern(pt.get(), local_vars);
            res.emplace_back(local_vars, case_term);
        }
        return res;
    }

    LocalVarsHead(Var) {
        return {};
    }

    void _collectFreeVariables(TermData* term, std::vector<std::string>& local_vars, std::unordered_set<std::string>& free_map) {
        if (term->getType() == TermType::VAR) {
            auto* tv = dynamic_cast<TmVar*>(term);
            for (auto& local: local_vars) {
                if (local == tv->name) return;
            }
            free_map.insert(tv->name); return;
        }
        for (auto& [new_locals, sub_term]: getSubTermWithLocalVars(term)) {
            int pre_size = local_vars.size();
            for (auto& local: new_locals) local_vars.push_back(local);
            _collectFreeVariables(sub_term.get(), local_vars, free_map);
            local_vars.resize(pre_size);
        }
    }
}

SubInfo util::getSubTermWithLocalVars(syntax::TermData *term) {
    switch (term->getType()) {
        TERM_CASE_ANALYSIS(LocalVarsCase);
    }
}

std::vector<std::string> util::getFreeVariables(syntax::TermData *term) {
    std::unordered_set<std::string> free_map;
    std::vector<std::string> local_vars;

    _collectFreeVariables(term, local_vars, free_map);
    return std::vector<std::string>(free_map.begin(), free_map.end());
}

namespace {
    class _FreeVariableRewriter: public IncreTermRewriter {
    public:
        std::string name;
        Term replace_term;
        std::unordered_map<TermData*, std::vector<std::string>> local_vars_map;
        _FreeVariableRewriter(const std::string& _name, const Term& _term): name(_name), replace_term(_term) {}

        virtual Term rewrite(const Term& term) {
            auto it = local_vars_map.find(term.get());
            assert(it != local_vars_map.end());
            for (auto& local: it->second) {
                if (local == name) return term;
            }
            for (auto& [local_vars, sub_term]: getSubTermWithLocalVars(term.get())) {
                local_vars_map[sub_term.get()] = local_vars;
            }
            return IncreTermRewriter::rewrite(term);
        }

        virtual Term _rewrite(TmVar* term, const Term& _term) override {
            if (term->name == name) return replace_term; else return _term;
        }
    };

    bool _isTrivialLet(TmLet* term) {
        if (getSubTerms(term->def.get()).empty()) return true;
        auto free_vars = getFreeVariables(term->body.get());
        for (auto& var: free_vars) if (var == term->name) return false;
        return true;
    }

    class _LetEliminator: public IncreTermRewriter {
    public:
        virtual Term _rewrite(TmLet* term, const Term& _raw_term) override {
            if (_isTrivialLet(term)) {
                auto* rewriter = new _FreeVariableRewriter(term->name, term->def);
                rewriter->local_vars_map.insert({term->body.get(), {}});
                auto new_term = rewriter->rewrite(term->body);
                delete rewriter;
                return rewrite(new_term);
            }
            return IncreTermRewriter::_rewrite(term, _raw_term);
        }
    };
}

syntax::Term util::removeTrivialLet(const syntax::Term &term) {
    auto* rewriter = new _LetEliminator();
    auto res = rewriter->rewrite(term);
    return res;
}

IncreProgram util::removeTrivialLetForProgram(IncreProgramData *program) {
    return _rewriteTermsInProgram(program, removeTrivialLet);
}

namespace {
    class _BoundEliminator: public IncreTypeRewriter {
    public:
        virtual Ty _rewrite(TyVar* type, const Ty& _type) override {
            if (type->is_bounded()) {
                return rewrite(type->get_bound_type());
            } else {
                return _type;
            }
        }
    };
}

syntax::Ty util::removeBoundedVar(const syntax::Ty &type) {
    auto* rewriter = new _BoundEliminator();
    auto res = rewriter->rewrite(type);
    delete rewriter;
    return res;
}