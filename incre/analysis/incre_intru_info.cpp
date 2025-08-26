//
// Created by pro on 2023/12/9.
//

#include "istool/incre/analysis/incre_instru_info.h"
#include "istool/incre/language/incre_rewriter.h"
#include "istool/incre/analysis/incre_instru_types.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;
using namespace incre::semantics;
using namespace incre::analysis;
using namespace incre::example;

int IncreInfoCollector::getNewCompressLabel() {
    int current = father.size();
    father.push_back(current); return current;
}

int IncreInfoCollector::getReprCompressLabel(int k) {
    if (k == father[k]) return k;
    return father[k] = getReprCompressLabel(father[k]);
}

void IncreInfoCollector::unionCompressLabel(int x, int y) {
    x = getReprCompressLabel(x); y = getReprCompressLabel(y);
    father[x] = y;
}

void IncreInfoCollector::constructFinalInfo() {
    final_index.resize(father.size());
    int now = 0;
    for (int i = 0; i < final_index.size(); ++i) {
        if (father[i] == i) final_index[i] = now++;
    }
    for (int i = 0; i < final_index.size(); ++i) {
        final_index[i] = final_index[getReprCompressLabel(i)];
    }
}

int IncreInfoCollector::getFinalCompressLabel(int k) {
    return final_index[k];
}

void IncreInfoCollector::_unify(syntax::TyCompress *ox, syntax::TyCompress *oy, const syntax::Ty &_x, const syntax::Ty &_y) {
    types::DefaultIncreTypeChecker::_unify(ox, oy, _x, _y);
    // LOG(INFO) << "unify compress " << ox->toString() << " " << oy->toString();
    auto* x = dynamic_cast<TyLabeledCompress*>(ox);
    auto* y = dynamic_cast<TyLabeledCompress*>(oy);
    if (!x || !y) throw types::IncreTypingError("All TyCompress types should be labeled, but get " + ox->toString() + " and " + oy->toString());
    unionCompressLabel(x->id, y->id);
}

syntax::Ty IncreInfoCollector::_typing(syntax::TmLabel *term, const IncreContext &ctx) {
    auto body = typing(term->body.get(), ctx);
    return std::make_shared<TyLabeledCompress>(body, getNewCompressLabel());
}

syntax::Ty IncreInfoCollector::_typing(syntax::TmUnlabel *term, const IncreContext &ctx) {
    auto body = typing(term->body.get(), ctx);
    auto content_type = getTmpVar(ANY);
    auto full_type = std::make_shared<TyLabeledCompress>(content_type, getNewCompressLabel());
    unify(full_type, body); return content_type;
}

syntax::Ty IncreInfoCollector::postProcess(syntax::TermData *term, const IncreContext &ctx, const syntax::Ty &res) {
    term_context_map.insert({term, ctx});
    term_type_map.insert({term, res});
    return res;
}

namespace {
    class _LabeledTypeConstructor: public syntax::IncreTypeRewriter {
    public:
        IncreInfoCollector* info;
        _LabeledTypeConstructor(IncreInfoCollector* _info): info(_info) {}
    protected:
        virtual Ty _rewrite(TyCompress* ot, const Ty& _type) {
            return std::make_shared<TyLabeledCompress>(rewrite(ot->body), info->getNewCompressLabel());
        }
    };

    class _LabeledTypeRewriter: public syntax::IncreTypeRewriter {
    public:
        IncreInfoCollector* info;
        _LabeledTypeRewriter(IncreInfoCollector* _info): info(_info) {
        }
    protected:
        virtual Ty _rewrite(TyCompress* ot, const Ty& _type) {
            auto* type = dynamic_cast<TyLabeledCompress*>(ot);
            if (!type) throw types::IncreTypingError("Expect TyLabeledCompress, but get " + ot->toString());
            return std::make_shared<TyLabeledCompress>(rewrite(type->body), info->getFinalCompressLabel(type->id));
        }
    };

    class _LabeledTermConstructor: public syntax::IncreTermRewriter {
    public:
        IncreInfoCollector* info;
        _LabeledTypeRewriter* type_rewriter;
        int rewrite_index = 0, command_id = -1;
        std::vector<std::tuple<int, IncreContext, Ty>> rewrite_raw_list;
        _LabeledTermConstructor(IncreInfoCollector* _info, _LabeledTypeRewriter* _rewriter): info(_info), type_rewriter(_rewriter) {
        }
    protected:
        virtual Term _rewrite(TmLabel* term, const Term& _term) {
            auto ot = type_rewriter->rewrite(info->term_type_map[term]);
            auto *ty = dynamic_cast<TyLabeledCompress*>(ot.get());
            if (!ty) throw types::IncreTypingError("Expect TyLabeledCompress, but get " + ot->toString());
            return std::make_shared<TmLabeledLabel>(rewrite(term->body), ty->id);
        }
        virtual Term _rewrite(TmUnlabel* term, const Term& _term) {
            auto ot = type_rewriter->rewrite(info->term_type_map[term->body.get()]);
            auto *ty = dynamic_cast<TyLabeledCompress*>(ot.get());
            if (!ty) throw types::IncreTypingError("Expect TyLabeledCompress, but get " + ot->toString());
            return std::make_shared<TmLabeledUnlabel>(rewrite(term->body), ty->id);
        }
        virtual Term _rewrite(TmRewrite* term, const Term& _term) {
            int index = rewrite_index++;
            auto it = info->term_context_map.find(term);
            rewrite_raw_list.emplace_back(index, it->second, info->term_type_map[term]);
            return std::make_shared<TmLabeledRewrite>(rewrite(term->body), index);
        }
    };

    class _IncreLabelConstructWalker: public IncreProgramWalker {
    public:
        _LabeledTypeConstructor* type_constructor;
        CommandList command_list;
        IncreInfoCollector * collector;
        IncreContext ctx;
        _IncreLabelConstructWalker(IncreInfoCollector* _collector): collector(_collector), ctx(nullptr) {
            type_constructor = new _LabeledTypeConstructor(collector);
        }
        virtual ~_IncreLabelConstructWalker() {delete type_constructor;}
    protected:
        virtual void visit(CommandDef *command) {
            std::vector<std::pair<std::string, Ty>> cons_list;
            for (auto& [cons_name, cons_ty]: command->cons_list) {
                auto new_cons_ty = type_constructor->rewrite(cons_ty);
                ctx = ctx.insert(cons_name, Binding(true, new_cons_ty, Data()));
                cons_list.emplace_back(cons_name, new_cons_ty);
                LOG(INFO) << "labeled insert " << cons_name << " " << new_cons_ty->toString();
            }
            command_list.push_back(std::make_shared<CommandDef>(command->name, command->param, cons_list, command->decos, command->source));
        }

        virtual void visit(CommandEval* command) {
            collector->pushLevel();
            collector->typing(command->term.get(), ctx);
            collector->popLevel();
        }

        virtual void visit(CommandBindTerm* command) {
            if (ctx.isContain(command->name)) {
                collector->unify(ctx.getRawType(command->name), collector->typing(command->term.get(), ctx));
            } else if (command->is_rec) {
                collector->pushLevel(); auto var = collector->getTmpVar(ANY);
                ctx = ctx.insert(command->name, var);
                auto* address = ctx.start.get();
                collector->unify(var, collector->typing(command->term.get(), ctx));
                collector->popLevel(); var = collector->generalize(var, command->term.get());
                address->bind.type = var;
            } else {
                collector->pushLevel();
                auto res = collector->typing(command->term.get(), ctx);
                collector->popLevel();
                ctx = ctx.insert(command->name, collector->generalize(res, command->term.get()));
            }
            command_list.push_back(std::make_shared<CommandBindTerm>(command->name, command->is_rec, command->term, command->decos, command->source));
        }

        virtual void visit(CommandDeclare* command) {
            auto new_type = type_constructor->rewrite(command->type);
            ctx = ctx.insert(command->name, new_type);
            command_list.push_back(std::make_shared<CommandDeclare>(command->name, new_type, command->decos, command->source));
        }
    };

    class _IndexedIncreProgramRewriter: public IncreProgramRewriter {
    protected:
        _LabeledTermConstructor* labeled_term_rewriter;
        virtual void visit(CommandDef *command) {
            labeled_term_rewriter->command_id += 1;
            IncreProgramRewriter::visit(command);
        }
        virtual void visit(CommandBindTerm* command) {
            labeled_term_rewriter->command_id += 1;
            IncreProgramRewriter::visit(command);
        }
        virtual void visit(CommandDeclare* command) {
            labeled_term_rewriter->command_id += 1;
            IncreProgramRewriter::visit(command);
        }
    public:
        _IndexedIncreProgramRewriter(IncreTypeRewriter* _rewriter, _LabeledTermConstructor* _term_rewriter):
                IncreProgramRewriter(_rewriter, _term_rewriter), labeled_term_rewriter(_term_rewriter) {
        }
    };
}

analysis::RewriteTypeInfo::RewriteTypeInfo(int _index, const IncreInputInfo &_inp_types, const syntax::Ty &_oup_type,
                                           int _command_id): index(_index), inp_types(_inp_types), oup_type(_oup_type), command_id(_command_id) {
}

analysis::IncreInfoData::IncreInfoData(const IncreProgram &_program,
                                       const std::vector<RewriteTypeInfo> &_rewrite_info_list,
                                       example::IncreExamplePool* _example_pool,
                                       const grammar::ComponentPool &_component_pool):
                                       program(_program), rewrite_info_list(_rewrite_info_list),
                                       example_pool(_example_pool), component_pool(_component_pool) {
}
IncreInfoData::~IncreInfoData() {delete example_pool;}

namespace {
    bool _isInclude(const IncreContext& ctx, const std::string& name) {
        for (auto add = ctx.start; add; add = add->next) {
            if (add->name == name) return true;
        }
        return false;
    }

    class _ArrowTypeDetector: public IncreTypeRewriter {
    public:
        bool is_valid = true;
    protected:
        virtual Ty _rewrite(TyArr* type, const Ty& _type) {
            is_valid = false;
            return _type;
        }
    public:
        _ArrowTypeDetector() = default;
        ~_ArrowTypeDetector() = default;
    };
}

bool input_filter::isValidInputType(const syntax::Ty& type) {
    auto* detector = new _ArrowTypeDetector();
    detector->rewrite(type); auto res = detector->is_valid;
    delete detector;
    return res;
}

IncreInputInfo input_filter::buildInputInfo(const IncreContext &local_ctx, const IncreContext &global_ctx) {
    IncreInputInfo info;
    for (auto add = local_ctx.start; add ; add = add->next) {
        if (!_isInclude(global_ctx, add->name)) {
            auto var_type = add->bind.getType();
            if (var_type->getType() == TypeType::POLY) continue;
            if (isValidInputType(var_type)) info.emplace_back(add->name, add->bind.getType());
        }
    }
    return info;
}

#include "istool/incre/grammar/incre_grammar_semantics.h"
#include "istool/sygus/theory/basic/theory_semantics.h"
#include "istool/ext/deepcoder/deepcoder_semantics.h"
#include "istool/incre/io/incre_printer.h"

IncreInfo analysis::buildIncreInfo(IncreProgramData *program, Env* env) {
    auto* collector = new IncreInfoCollector();
    IncreProgram mid_program; IncreContext global_ctx(nullptr);
    {
        auto* constructor = new _IncreLabelConstructWalker(collector);
        constructor->walkThrough(program); global_ctx = constructor->ctx;
        mid_program = std::make_shared<IncreProgramData>(constructor->command_list, program->config_map);
        delete constructor;
    }

    collector->constructFinalInfo();
    auto *type_rewriter = new _LabeledTypeRewriter(collector);
    auto *term_rewriter = new _LabeledTermConstructor(collector, type_rewriter);
    IncreProgram res_program;
    {
        auto *rewriter = new _IndexedIncreProgramRewriter(type_rewriter, term_rewriter);
        rewriter->walkThrough(mid_program.get());
        res_program = rewriter->res;
        delete rewriter;
    }

    //LOG(INFO) << "print whole program";
    //incre::printProgram(res_program);

    std::vector<RewriteTypeInfo> rewrite_info_list;
    {
        for (int index = 0; index < term_rewriter->rewrite_raw_list.size(); ++index) {
            auto& [command_id, local_ctx, oup_ty] = term_rewriter->rewrite_raw_list[index];
            auto new_oup_ty = type_rewriter->rewrite(oup_ty);
            auto inp_infos = input_filter::buildInputInfo(local_ctx, global_ctx);
            for (auto& [_, inp_ty]: inp_infos) inp_ty = type_rewriter->rewrite(inp_ty);
            rewrite_info_list.emplace_back(index, inp_infos, new_oup_ty, command_id);
        }
    }

    delete term_rewriter; delete type_rewriter; delete collector;

    IncreExamplePool* example_pool;
    {
        std::vector<std::vector<std::string>> cared_var_storage;
        for (auto &info: rewrite_info_list) {
            std::vector<std::string> cared_var_list;
            for (auto &inp_var_info: info.inp_types) {
                cared_var_list.push_back(inp_var_info.first);
            }
            cared_var_storage.push_back(cared_var_list);
        }

        auto *generator = new SizeSafeValueGenerator(env, extractConsMap((res_program.get())));
        example_pool = new IncreExamplePool(res_program, cared_var_storage, generator);
    }

    // Prepare the environment
    theory::loadBasicSemantics(env, TheoryToken::CLIA);
    ext::ho::loadDeepCoderSemantics(env);
    incre::semantics::registerIncreExecutionInfo(env, example_pool->global_name_list);

    auto component_pool = incre::grammar::collector::collectComponent(env, res_program.get());

    return std::make_shared<IncreInfoData>(res_program, rewrite_info_list, example_pool, component_pool);
}
