use crate::{extract};
use std::rc::Rc;
use ariadne::Span;
use crate::language::{bind_into, look_up, Bind, Command, Context, Pattern, SpanPattern, SpanTerm, SpanType, Term, Type, WithSpan};
use crate::processor::processor::Processor;
use crate::{spanned, with_span};

// Change all zero-arity constructor to positive-arity
#[derive(Default)]
pub struct RemoverZeroArity {
    pub context: Context<bool>
}

fn unfold_cons_type(ty: &SpanType) -> (Vec<String>, SpanType) {
    match &ty.content {
        Type::Poly {vars, body} => (vars.clone(), body.clone()),
        _ => (vec![], ty.clone())
    }
}

fn build_cons_type(vars: Vec<String>, ty: SpanType) -> SpanType {
     if vars.is_empty() {
         ty.clone()
     } else {
         with_span!(ty.span, Type::Poly {vars, body: ty})
     }
}

impl Processor for RemoverZeroArity {
    fn process_pattern(&mut self, pattern: &SpanPattern) -> SpanPattern {
        match &pattern.content {
            Pattern::Cons(name, content) => {
                let flag = look_up(name, &self.context).unwrap();
                if let Some(content_pattern) = content {
                    assert!(!flag);
                    let new_pattern = self.process_pattern(content_pattern);
                    with_span!(pattern.span, Pattern::Cons(name.clone(), Some(new_pattern)))
                } else {
                    // add wildcard for empty pattern
                    assert!(look_up(name, &self.context).unwrap());
                    let new_content = with_span!(pattern.span, Pattern::Wildcard);
                    with_span!(pattern.span, Pattern::Cons(name.clone(), Some(new_content)))
                }
            },
            _ => self.default_process_pattern(pattern)
        }
    }

    fn process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        match &term.content {
            Term::App(f, p) => {
                if let Term::NativeCons(name) = &f.content {
                    let info = look_up(name, &self.context).unwrap();
                    if info {
                        let new_term = Term::Cons {cons: name.clone(), body: self.process_term(p)};
                        return with_span!(term.span, new_term);
                    }
                }
                self.default_process_term(term)
            },
            Term::NativeCons(name) => {
                let info = look_up(name, &self.context).unwrap();
                if info {
                    let param = with_span!(term.span, Term::Unit);
                    with_span!(term.span, Term::Cons {cons: name.clone(), body: param})
                } else {
                    self.default_process_term(term)
                }
            }
            _ => self.default_process_term(term)
        }
    }

    fn process_command(&mut self, command: &Command) -> Command {
        match command {
            Command::TypeDef {name, cons_list, arity} => {
                let mut new_cons_list = vec![];
                for cons in cons_list {
                    let (cons_name, cons_type) = cons;
                    let (vars, content_type) = unfold_cons_type(cons_type);
                    if let Type::Arr(_, _)  = &content_type.content {
                        self.context = bind_into(cons_name.clone(), false, self.context.clone());
                        new_cons_list.push(cons.clone());
                    } else {
                        self.context = bind_into(cons_name.clone(), true, self.context.clone());
                        let param = with_span!(cons_type.span, Type::Unit);
                        let new_content = with_span!(cons_type.span, Type::Arr(param, content_type.clone()));
                        let new_type = build_cons_type(vars.clone(), new_content);
                        new_cons_list.push((cons_name.clone(), new_type));
                    }
                }
                Command::TypeDef {name: name.clone(), cons_list: new_cons_list, arity: *arity}
            },
            _ => self.default_process_command(command)
        }
    }
}

#[derive(Default)]
pub struct AddZeroArity {
    pub context: Context<bool>
}


impl Processor for AddZeroArity {
    fn process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        match &term.content {
            Term::App(f, p) => {
                if let Term::NativeCons(name) = &f.content {
                    let info = look_up(name, &self.context).unwrap();
                    if info {
                        assert_eq!(p.content, Term::Unit);
                        return with_span!(term.span, Term::NativeCons(name.clone()));
                    }
                }
                self.default_process_term(term)
            },
            Term::Cons {cons, body} => {
                let info = look_up(cons, &self.context).unwrap();
                if info {
                    assert_eq!(body.content, Term::Unit);
                    with_span!(term.span, Term::NativeCons(cons.clone()))
                } else {
                    self.default_process_term(term)
                }
            },
            Term::NativeCons(name) => {
                let info = look_up(name, &self.context).unwrap();
                if info {
                    let tmp = String::from("tmp");
                    let dummy_func = Term::Func {params: vec![tmp], body: term.clone()};
                    with_span!(term.span, dummy_func)
                } else {
                    term.clone()
                }
            }
            _ => self.default_process_term(term)
        }
    }

    fn process_pattern(&mut self, pattern: &SpanPattern) -> SpanPattern {
        match &pattern.content {
            Pattern::Cons(name, content) => {
                let info = look_up(name, &self.context).unwrap();
                if info {
                    with_span!(pattern.span, Pattern::Cons(name.clone(), None))
                } else {
                    self.default_process_pattern(pattern)
                }
            },
            _ => self.default_process_pattern(pattern)
        }
    }

    fn process_command(&mut self, command: &Command) -> Command {
        match command {
            Command::TypeDef {name, cons_list, arity} => {
                let mut new_cons_list = vec![];
                for (cons_name, cons_type) in cons_list {
                    // println!("Processing {}: {}", cons_name, cons_type.content);
                    let (vars, content) = unfold_cons_type(cons_type);
                    // println!("content: {}", content.content);
                    let (val, res) = extract!(&content.content, Type::Arr(val, res) => (val, res));
                    if val.content == Type::Unit {
                        self.context = bind_into(cons_name.clone(), true, self.context.clone());
                        new_cons_list.push((cons_name.clone(), build_cons_type(vars, res.clone())))
                    } else {
                        self.context = bind_into(cons_name.clone(), false, self.context.clone());
                        new_cons_list.push((cons_name.clone(), cons_type.clone()));
                    }
                }
                Command::TypeDef {name: name.clone(), cons_list: new_cons_list, arity: *arity}
            },
            _ => self.default_process_command(command)
        }
    }
}