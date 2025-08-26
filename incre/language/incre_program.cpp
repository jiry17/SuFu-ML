//
// Created by pro on 2023/12/8.
//

#include "istool/incre/language/incre_program.h"
#include "istool/incre/language/incre_util.h"
#include "glog/logging.h"

using namespace incre;
using namespace incre::syntax;


CommandData::CommandData(const CommandType &_type, const std::string& _name, const DecorateSet &_decos, const std::string& _source):
        type(_type), decos(_decos), name(_name), source(_source) {
}

bool CommandData::isDecrorateWith(CommandDecorate deco) const {
    return decos.find(deco) != decos.end();
}

CommandType CommandData::getType() const {return type;}

CommandBindTerm::CommandBindTerm(const std::string &_name, bool _is_func, const syntax::Term &_term,
                                 const DecorateSet &decos, const std::string& _source):
        CommandData(CommandType::BIND_TERM, _name, decos, _source),
        is_rec(_is_func), term(_term) {
}

CommandDef::CommandDef(const std::string &_name, int _param, const std::vector<std::pair<std::string, Ty>> &_cons_list,
                       const DecorateSet &decos, const std::string& _source):
                       param(_param), cons_list(_cons_list), CommandData(CommandType::DEF_IND, _name, decos, _source) {
}

CommandDeclare::CommandDeclare(const std::string &_name, const syntax::Ty &_type, const DecorateSet &decos, const std::string& _source):
                               CommandData(CommandType::DECLARE, _name, decos, _source), type(_type) {
}

CommandEval::CommandEval(const std::string &_name, const syntax::Term &_term, const incre::DecorateSet &decos,
                         const std::string &_source): CommandData(CommandType::EVAL, _name, decos, _source), term(_term) {
}

IncreProgramData::IncreProgramData(const CommandList &_commands, const IncreConfigMap &_config_map):
        commands(_commands), config_map(_config_map) {
}

void incre::IncreProgramWalker::walkThrough(IncreProgramData *program) {
    initialize(program);
    for (auto& command: program->commands) {
        preProcess(command.get());
        switch (command->getType()) {
            case CommandType::BIND_TERM: {
                auto* cb = dynamic_cast<CommandBindTerm*>(command.get());
                visit(cb); break;
            }
            case CommandType::DEF_IND: {
                auto* cd = dynamic_cast<CommandDef*>(command.get());
                visit(cd); break;
            }
            case CommandType::DECLARE: {
                auto* ci = dynamic_cast<CommandDeclare*>(command.get());
                visit(ci); break;
            }
            case CommandType::EVAL: {
                auto* ce = dynamic_cast<CommandEval*>(command.get());
                visit(ce); break;
            }
        }
        postProcess(command.get());
    }
}

incre::DefaultContextBuilder::DefaultContextBuilder(incre::semantics::IncreEvaluator *_evaluator,
                                                    incre::types::IncreTypeChecker *_checker):
                                                    evaluator(_evaluator), checker(_checker), ctx(nullptr) {
}

void incre::DefaultContextBuilder::visit(CommandBindTerm *command) {
    // LOG(INFO) << "processing bind term " << command->name << " " << command->is_rec;
    if (ctx.isContain(command->name)) {
        auto* address = ctx.getAddress(command->name);
        if (!address->bind.data.isNull()) LOG(FATAL) << "Duplicated declaration on name " << command->name;
        if (checker) {
            assert(address->bind.type);
            checker->unify(address->bind.type, checker->typing(command->term.get(), ctx));
        }
        if (evaluator) {
            address->bind.data = evaluator->evaluate(command->term.get(), ctx);
        }
    } else if (command->is_rec) {
        ctx = ctx.insert(command->name, Binding(false, {}, {}));
        auto* address = ctx.getAddress(command->name);
        if (checker) {
            checker->pushLevel(); auto current_type = checker->getTmpVar(ANY);
            address->bind.type = current_type;
            checker->unify(current_type, checker->typing(command->term.get(), ctx));
            checker->popLevel();
            auto final_type = checker->generalize(current_type, command->term.get());
            address->bind.type = final_type;
        }
        if (evaluator) {
            auto v = evaluator->evaluate(command->term.get(), ctx);
            address->bind.data = v;
        }
    } else {
        Ty type = nullptr;
        if (checker) {
            // LOG(INFO) << command->term->toString();
            checker->pushLevel(); type = checker->typing(command->term.get(), ctx);
            checker->popLevel(); type = checker->generalize(type, command->term.get());
        }
        auto res = evaluator ? evaluator->evaluate(command->term.get(), ctx): Data();
        ctx = ctx.insert(command->name, Binding(false, type, res));
    }
}

void incre::DefaultContextBuilder::visit(CommandDef *command) {
    for (auto& [cons_name, cons_type]: command->cons_list) {
        ctx = ctx.insert(cons_name, Binding(true, cons_type, {}));
    }
}

void incre::DefaultContextBuilder::visit(CommandEval* command) {
}

void incre::DefaultContextBuilder::visit(CommandDeclare *command) {
    if (ctx.isContain(command->name)) LOG(FATAL) << "Duplicated name " << command->name;
    ctx = ctx.insert(command->name, command->type);
}

IncreFullContext incre::buildContext(IncreProgramData *program, semantics::IncreEvaluator *evaluator, types::IncreTypeChecker *checker) {
    auto* walker = new DefaultContextBuilder(evaluator, checker);
    walker->walkThrough(program);
    auto res = std::make_shared<IncreFullContextData>(walker->ctx, walker->address_map);
    delete walker;
    return res;
}

IncreFullContext incre::buildContext(IncreProgramData *program, const semantics::IncreEvaluatorGenerator &eval_gen,
                                     const types::IncreTypeCheckerGenerator &type_checker_gen) {
    auto *eval = eval_gen(); auto* type_checker = type_checker_gen();
    auto ctx = buildContext(program, eval, type_checker);
    delete eval; delete type_checker;
    return ctx;
}


void IncreProgramRewriter::visit(CommandDef *command) {
    std::vector<std::pair<std::string, Ty>> cons_list;
    for (auto& [cons_name, cons_ty]: command->cons_list) {
        cons_list.emplace_back(cons_name, type_rewriter->rewrite(cons_ty));
    }
    res->commands.push_back(std::make_shared<CommandDef>(command->name, command->param, cons_list, command->decos, command->source));
}

void IncreProgramRewriter::visit(CommandBindTerm *command) {
    auto new_term = term_rewriter->rewrite(command->term);
    res->commands.push_back(std::make_shared<CommandBindTerm>(command->name, command->is_rec, new_term, command->decos, command->source));
}

void IncreProgramRewriter::visit(CommandDeclare *command) {
    auto new_type = type_rewriter->rewrite(command->type);
    res->commands.push_back(std::make_shared<CommandDeclare>(command->name, new_type, command->decos, command->source));
}

void IncreProgramRewriter::initialize(IncreProgramData *program) {
    res = std::make_shared<IncreProgramData>(CommandList(), program->config_map);
}

IncreProgramRewriter::IncreProgramRewriter(IncreTypeRewriter *_type_rewriter, IncreTermRewriter *_term_rewriter):
        type_rewriter(_type_rewriter), term_rewriter(_term_rewriter) {
}

void IncreProgramRewriter::visit(incre::CommandEval *command) {
    auto new_term = term_rewriter->rewrite(command->term);
    res->commands.push_back(std::make_shared<CommandEval>(command->name, new_term, command->decos, command->source));
}

IncreProgram syntax::rewriteProgram(IncreProgramData *program, const IncreTypeRewriterGenerator &type_gen,
                                    const IncreTermRewriterGenerator &term_gen) {
    auto* type_rewriter = type_gen(); auto* term_rewriter = term_gen();
    auto* rewriter = new IncreProgramRewriter(type_rewriter, term_rewriter);
    rewriter->walkThrough(program); auto res = rewriter->res;
    delete type_rewriter; delete term_rewriter; delete rewriter;
    return res;
}

ProgramRunner::ProgramRunner(incre::semantics::IncreEvaluator *_evaluator, incre::types::IncreTypeChecker *_checker):
        DefaultContextBuilder(_evaluator, _checker) {
    assert(_evaluator && _checker);
}

void ProgramRunner::postProcess(incre::CommandData *command) {
    switch (command->getType()) {
        case CommandType::EVAL: {
            auto* ce = dynamic_cast<CommandEval*>(command);
            ctx.printTypes();
            auto res_value = evaluator->evaluate(ce->term.get(), ctx);
            auto res_term = std::make_shared<TmValue>(res_value);
            auto new_command = std::make_shared<CommandEval>(ce->name, res_term, ce->decos, ce->source);
            commands.push_back(new_command);
            break;
        }
        case CommandType::BIND_TERM: {
            auto bind = ctx.getAddress(command->name);
            auto res_type = util::removeBoundedVar(bind->bind.type);
            auto new_command = std::make_shared<CommandDeclare>(
                    command->name, res_type, command->decos, command->source
                    );
            commands.push_back(new_command);
            break;
        }
        case CommandType::DECLARE:
        case CommandType::DEF_IND: break;
    }
}

IncreProgram incre::runProgram(incre::IncreProgramData *program, semantics::IncreEvaluator *evaluator,
                               types::IncreTypeChecker *checker) {
    auto* runner = new ProgramRunner(evaluator, checker);
    runner->walkThrough(program);
    auto res = std::make_shared<IncreProgramData>(runner->commands, program->config_map);
    delete runner;
    return res;
}