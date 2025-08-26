use std::collections::HashSet;
use std::rc::Rc;
use chumsky::primitive::JustCfg;
use crate::language::{Type, SpanType, WithSpan, Term, Pattern, Context, look_up, bind_into, BindTerm, SpanPattern, SpanTerm};

const VAR_NAME: &str = "var";

pub fn collect_pattern_vars(pattern: &Pattern) -> Vec<String> {

    fn _collect_new_vars(pattern: &Pattern, vars: &mut Vec<String>) {
        match pattern {
            Pattern::Wildcard => {}
            Pattern::Var(pattern, var) => {
                if let Some(content) = pattern {
                    _collect_new_vars(&content.content, vars);
                }
                vars.push(var.clone());
            }
            Pattern::Tuple(elements) => {
                for element in elements.iter() {
                    _collect_new_vars(&element.content, vars);
                }
            }
            Pattern::Cons(cons, pattern) => {
                if let Some(content) = pattern {
                    _collect_new_vars(&content.content, vars);
                }
            }
        }
    }

    let mut result = Vec::default();
    _collect_new_vars(pattern, &mut result);
    result
}

pub fn collect_free_vars(term: &Term) -> Vec<String> {
    let context = Context::default();
    let mut res = HashSet::default();

    fn traverse(term: &Term, context: &Context<()>, free_vars: &mut HashSet<String>) {

        fn process_cases(cases: &Vec<(SpanPattern, SpanTerm)>, context: &Context<()>, free_vars: &mut HashSet<String>) {
            for (pt, case) in cases.iter() {
                let vars = collect_pattern_vars(&pt.content);
                let mut new_context = context.clone();
                for var in vars.iter() {
                    new_context = bind_into(var.clone(), (), new_context);
                }
                traverse(&case.content, &new_context, free_vars);
            }
        }

        match term {
            Term::Int(_) | Term::Bool(_) | Term::Unit | Term::NativeCons(_) => (),
            Term::Var(name) => {
                if look_up(name, context).is_none() {
                    free_vars.insert(name.clone());
                }
            },
            Term::If(c, t, f) => {
                traverse(&c.content, context, free_vars);
                traverse(&t.content, context, free_vars);
                traverse(&f.content, context, free_vars);
            }
            Term::PrimOp { op, params } => {
                for param in params.iter() {
                    traverse(&param.content, context, free_vars);
                }
            }
            Term::App(f, p) => {
                traverse(&f.content, context, free_vars);
                traverse(&p.content, context, free_vars);
            }
            Term::Func { params, body } => {
                let mut new_context = context.clone();
                for param in params.iter() {
                    new_context = bind_into(param.clone(), (), new_context);
                }
                traverse(&body.content, &new_context, free_vars);
            }
            Term::Tuple(contents) => {
                for content in contents.iter() {
                    traverse(&content.content, context, free_vars);
                }
            }
            Term::Match { def, cases } => {
                traverse(&def.content, context, free_vars);
                process_cases(cases, context, free_vars);
            }
            Term::Cons { cons, body } => {
                traverse(&body.content, context, free_vars);
            }
            Term::Let { bind, body } => {
                match &bind.content.bind {
                    BindTerm::NormalBind(t) => {
                        traverse(&t.content, context, free_vars);
                    },
                    BindTerm::FuncBind(cases) => {
                        process_cases(cases, context, free_vars);
                    }
                }
                let new_context = bind_into(bind.content.name.clone(), (), context.clone());
                traverse(&body.content, &new_context, free_vars);
            }
        }
    }

    traverse(term, &context, &mut res);
    res.into_iter().collect()
}

pub fn get_free_name(terms: &Vec<SpanTerm>) -> String {
    let vars: Vec<String> = terms.iter()
        .map(|t| collect_free_vars(&t.content))
        .collect::<Vec<_>>()
        .concat();
    let vars: HashSet<_> = vars.into_iter().collect();

    for i in 0..usize::MAX {
        let current_name = format!("{}{}", VAR_NAME, i);
        if !vars.contains(&current_name) {
            return current_name;
        }
    }
    panic!();
}