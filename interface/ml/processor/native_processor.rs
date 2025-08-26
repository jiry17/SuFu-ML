use crate::{extract};
use std::rc::Rc;
use crate::util::{collect_pattern_vars, get_free_name};
use crate::language::{Bind, BindTerm, Command, SpanBind, SpanCommand, SpanTerm, Term, WithSpan};
use crate::processor::processor::Processor;
use crate::util::collect_free_vars;
use crate::with_span;

#[derive(Default)]
pub struct RemoveNative {}

const TMP_NAME: &str = "tmp";

impl RemoveNative {
    fn remove_native_bind(bind: &SpanBind) -> SpanBind {
        match &bind.content.bind {
            BindTerm::NormalBind(_) => bind.clone(),
            BindTerm::FuncBind(cases) => {
                let terms = cases.iter().map(|(_, t)| t.clone()).collect();
                let free_var = get_free_name(&terms);
                let var_term = with_span!(bind.span, Term::Var(free_var.clone()));
                let match_term = with_span!(bind.span, Term::Match {def: var_term, cases: cases.clone()});
                let mut full_params = bind.content.params.clone();
                full_params.push(free_var);
                let new_bind = Bind {
                    is_rec: bind.content.is_rec,
                    params: full_params,
                    bind: BindTerm::NormalBind(match_term),
                    name: bind.content.name.clone()
                };
                with_span!(bind.span, new_bind)
            }
        }
    }

    fn remove_multi_param_bind(bind: &SpanBind) -> SpanBind {
        let mut term = extract!(&bind.content.bind, BindTerm::NormalBind(t) => t).clone();
        for var in bind.content.params.iter().rev() {
            term = with_span!(term.span, Term::Func {params: vec![var.clone()], body: term});
        }
        let new_bind = Bind {
            is_rec: bind.content.is_rec,
            params: vec![],
            bind: BindTerm::NormalBind(term), name: bind.content.name.clone()
        };
        with_span!(bind.span, new_bind)
    }
}

fn process_bind<P: Processor>(bind: &SpanBind, p: &mut P) -> SpanBind {
    let new_bind_term = match &bind.content.bind {
        BindTerm::NormalBind(term) => {
            BindTerm::NormalBind(p.process_term(term))
        },
        BindTerm::FuncBind(cases) => {
            let new_cases = cases.iter()
                .map(|(pt, case)| (p.process_pattern(pt), p.process_term(case)))
                .collect();
            BindTerm::FuncBind(new_cases)
        }
    };
    with_span!(bind.span, bind.content.clone_with_new_bind(new_bind_term))
}

impl Processor for RemoveNative {
    fn process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        // println!("processing {}", term.content.to_pretty(20));
        match &term.content {
            Term::Let {bind, body} => {
                let new_bind = Self::remove_native_bind(bind);
                let new_bind = Self::remove_multi_param_bind(&new_bind);
                let new_bind = process_bind(&new_bind, self);
                let new_body = self.process_term(body);
                with_span!(term.span, Term::Let {bind: new_bind, body: new_body})
            },
            Term::App(f, p) => {
                if let Term::NativeCons(name) = &f.content {
                    let p_res = self.process_term(p);
                    with_span!(term.span, Term::Cons {cons: name.clone(), body: p_res})
                } else {
                    self.default_process_term(term)
                }
            }
            Term::NativeCons(name) => {
                // println!("try remove native cons {}", name);
                let var = with_span!(term.span, Term::Var(TMP_NAME.to_string()));
                let body = with_span!(term.span, Term::Cons {cons: name.clone(), body: var});
                let func = with_span!(term.span, Term::Func {params: vec![TMP_NAME.to_string()], body});
                func
            },
            Term::Func {params, body} => {
                let mut res = self.process_term(body);
                for var in params.iter().rev() {
                    res = with_span!(term.span, Term::Func {params: vec![var.clone()], body: res});
                }
                res
            },
            _ => self.default_process_term(term)
        }
    }

    fn process_command(&mut self, command: &Command) -> Command {
        match command {
            Command::TermDef(bind) => {
                let new_bind = Self::remove_native_bind(bind);
                let new_bind = Self::remove_multi_param_bind(&new_bind);
                Command::TermDef(process_bind(&new_bind, self))
            }
            _ => self.default_process_command(command)
        }
    }
}


#[derive(Default)]
pub struct AddNative {}

impl AddNative {
    fn add_multi_param(bind: &SpanBind) -> SpanBind {
        let mut term = extract!(&bind.content.bind, BindTerm::NormalBind(t) => t).clone();
        let mut full_params = bind.content.params.clone();
        while let Term::Func {params, body} = &term.content {
            for param in params.iter() { full_params.push(param.clone()); }
            term = body.clone();
        }
        // println!("pre: {}", bind.content.to_pretty(40));
        // println!("  {:?}", full_params);
        let new_bind = Bind {
            is_rec: bind.content.is_rec, params: full_params,
            name: bind.content.name.clone(), bind: BindTerm::NormalBind(term)
        };
        // println!("new: {}", new_bind.to_pretty(40));
        with_span!(bind.span, new_bind)
    }

    fn add_func_bind(bind: &SpanBind) -> SpanBind {
        let content = &bind.content;

        let mut term = extract!(&content.bind, BindTerm::NormalBind(t) => t).clone();
        if content.params.is_empty() {return bind.clone();}
        let last_var = content.params.last().unwrap();

        if let Term::Match {def, cases} = &term.content {
            if def.content != Term::Var(last_var.clone()) {return bind.clone();}
            for (pt, case) in cases.iter() {
                let pt_free_vars = collect_pattern_vars(&pt.content);
                if pt_free_vars.contains(last_var) {continue;}
                let term_free_vars = collect_free_vars(&case.content);
                if term_free_vars.contains(last_var) {return bind.clone();}
            }
            let new_bind_term = BindTerm::FuncBind(cases.clone());
            let mut params = content.params.clone(); params.pop();
            let new_bind = Bind {
                is_rec: content.is_rec, name: content.name.clone(),
                params, bind: new_bind_term
            };
            with_span!(bind.span, new_bind)
        } else {
            bind.clone()
        }
    }
}

impl Processor for AddNative {
    fn process_term(&mut self, term: &SpanTerm) -> SpanTerm {
        match &term.content {
            Term::Let {bind, body} => {
                let new_bind = Self::add_multi_param(bind);
                let new_bind = Self::add_func_bind(&new_bind);
                let new_bind = process_bind(&new_bind, self);
                let new_body = self.process_term(body);
                with_span!(term.span, Term::Let {bind: new_bind, body: new_body})
            },
            Term::Func {params, body} => {
                match &body.content {
                    Term::Func {params: inner_params, body: inner_body} => {
                        let new_params: Vec<_> = params.iter().chain(inner_params.iter()).cloned().collect();
                        let new_term = with_span!(term.span, Term::Func {params: new_params, body: inner_body.clone()});
                        self.process_term(&new_term)
                    },
                    Term::Cons {cons, body} => {
                        if params.is_empty() {return self.default_process_term(term);}
                        let last_var = params.last().unwrap();

                        if body.content == Term::Var(last_var.clone()) {
                            let new_body = with_span!(body.span, Term::NativeCons(cons.clone()));
                            let mut new_params = params.clone(); new_params.pop();
                            with_span!(term.span, Term::Func {params: new_params, body: new_body})
                        } else {
                            self.default_process_term(term)
                        }
                    },
                    _ => self.default_process_term(term)
                }
            },
            _ => self.default_process_term(term)
        }
    }


    fn process_command(&mut self, command: &Command) -> Command {
        match command {
            Command::TermDef(bind) => {
                let new_bind = Self::add_multi_param(bind);
                let new_bind = Self::add_func_bind(&new_bind);
                let new_bind = process_bind(&new_bind, self);
                Command::TermDef(new_bind)
            },
            _ => self.default_process_command(command)
        }
    }
}