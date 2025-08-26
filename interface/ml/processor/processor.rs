use crate::language::WithSpan;
use std::rc::Rc;
use crate::language::{look_up, Bind, BindTerm, Command, Context, DecoratedCommand, Pattern, Program, SpanBind, SpanPattern, SpanTerm, SpanType, Term, Type};
use crate::{spanned, with_span};

pub trait Processor {
    fn default_process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        match &term.content {
            Term::Int(_) | Term::Bool(_) | Term::Unit | Term::Var(_) | Term::NativeCons(_) => {
                term.clone()
            },
            Term::If(c, t, f) => {
                let new_term = Term::If(
                    self.process_term(c),
                    self.process_term(t),
                    self.process_term(f)
                );
                with_span!(term.span, new_term)
            },
            Term::PrimOp { params, op } => {
                let new_params = params.iter().map(|param| self.process_term(param)).collect();
                with_span!(term.span, Term::PrimOp { op: op.clone(), params: new_params })
            },
            Term::App(f, p) => {
                with_span!(term.span, Term::App(self.process_term(f), self.process_term(p)))
            },
            Term::Tuple(contents) => {
                let new_contents = contents.iter().map(|term| self.process_term(term)).collect();
                with_span!(term.span, Term::Tuple(new_contents))
            },
            Term::Match { def, cases } => {
                let new_def = self.process_term(def);
                let new_cases = cases.iter().map(
                    |(pattern, case)| (pattern.clone(), self.process_term(case))
                ).collect();
                with_span!(term.span, Term::Match { def: new_def, cases: new_cases })
            },
            Term::Cons { cons, body } => {
                let new_body = self.process_term(body);
                with_span!(term.span, Term::Cons { cons: cons.clone(), body: new_body })
            },
            Term::Let { bind, body} => {
                let bind_term = &bind.content.bind;
                let new_bind_term = match bind_term {
                    BindTerm::NormalBind(t) => BindTerm::NormalBind(self.process_term(t)),
                    BindTerm::FuncBind(cases) => {
                        let new_cases = cases.iter()
                            .map(|(pt, t)| (
                                self.process_pattern(pt), self.process_term(t)
                            ))
                            .collect();
                        BindTerm::FuncBind(new_cases)
                    }
                };
                let new_bind = with_span!(bind.span, bind.content.clone_with_new_bind(new_bind_term));
                with_span!(term.span, Term::Let { bind: new_bind, body: self.process_term(body) })
            },
            Term::Func { params, body } => {
                let new_body = self.process_term(body);
                with_span!(term.span, Term::Func { params: params.clone(), body: new_body })
            }
        }
    }

    fn process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        self.default_process_term(term)
    }

    fn default_process_type(&mut self, ty: &SpanType) -> SpanType {
        match &ty.content {
            Type::Unit | Type::Int | Type::Bool | Type::Var(_) => ty.clone(),
            Type::Poly { vars, body } => {
                let new_body = self.process_type(body);
                with_span!(ty.span, Type::Poly { vars: vars.clone(), body: new_body })
            }
            Type::Tuple(contents) => {
                let new_contents = contents.iter().map(|ty| self.process_type(ty)).collect();
                with_span!(ty.span, Type::Tuple(new_contents))
            }
            Type::Arr(f, p) => {
                let new_f = self.process_type(f);
                let new_p = self.process_type(p);
                with_span!(ty.span, Type::Arr(new_f, new_p))
            }
            Type::Ind { name, params } => {
                let new_params = params.iter().map(|param| self.process_type(param)).collect();
                with_span!(ty.span, Type::Ind { name: name.clone(), params: new_params })
            }
        }
    }

    fn process_type(&mut self, ty: &SpanType) -> SpanType {
        self.default_process_type(ty)
    }

    fn default_process_pattern(&mut self, pattern: &SpanPattern) -> SpanPattern {
        match &pattern.content {
            Pattern::Wildcard => {pattern.clone()}
            Pattern::Var(whole, var) => {
                let new_whole = whole.as_ref().map(|x| self.process_pattern(x));
                with_span!(pattern.span, Pattern::Var(new_whole, var.clone()))
            }
            Pattern::Tuple(contents) => {
                let new_contents = contents.iter().map(|content| self.process_pattern(content)).collect();
                with_span!(pattern.span, Pattern::Tuple(new_contents))
            }
            Pattern::Cons(name, content) => {
                let new_content: Option<_> = content.as_ref().map(|p| self.process_pattern(p));
                with_span!(pattern.span, Pattern::Cons(name.clone(), new_content))
            }
        }
    }

    fn process_pattern(&mut self, pattern: &SpanPattern) -> SpanPattern {
        self.default_process_pattern(pattern)
    }

    fn default_process_command(&mut self, command: &Command) -> Command {
        match command {
            Command::Config {..} => {
                command.clone()
            }
            Command::TypeDef { name, cons_list, arity } => {
                let mut new_cons_list = vec![];
                for (cons_name, cons_type) in cons_list.iter() {
                    let new_type = self.process_type(&cons_type);
                    new_cons_list.push((cons_name.clone(), new_type));
                }
                Command::TypeDef {name: name.clone(), cons_list: new_cons_list, arity: *arity}
            }
            Command::TypeAlias { name, def } => {
                let new_type = self.process_type(def);
                Command::TypeAlias {name: name.clone(), def: new_type}
            }
            Command::TermEval(term) => {
                let new_term = self.process_term(&term);
                Command::TermEval(new_term)
            }
            Command::TermDef(def) => {
                let new_term = match &def.content.bind {
                    BindTerm::NormalBind(t) => {BindTerm::NormalBind(self.process_term(t))},
                    BindTerm::FuncBind(cases) => {
                        let new_cases = cases.iter()
                            .map(|(p, t)| (self.process_pattern(p), self.process_term(t)))
                            .collect();
                        BindTerm::FuncBind(new_cases)
                    }
                };
                let new_bind = with_span!(def.span,
                    Bind {bind: new_term, name: def.content.name.clone(), params: def.content.params.clone(), is_rec: def.content.is_rec}
                );
                Command::TermDef(new_bind)
            }
            Command::TypeDeclare { name, ty } => {
                let new_type = self.process_type(ty);
                Command::TypeDeclare {name: name.clone(), ty: new_type}
            }
        }
    }

    fn process_command(&mut self, command: &Command) -> Command {
        self.default_process_command(command)
    }

    fn process_program(&mut self, program: &Program) -> Program {
        let mut commands = vec![];
        for deco_command in program.iter() {
            let content = &deco_command.command;
            let new_command = self.process_command(&content.content);
            commands.push(
                DecoratedCommand {decos: deco_command.decos.clone(), command: with_span!(content.span, new_command)}
            );
        }
        commands
    }
}
