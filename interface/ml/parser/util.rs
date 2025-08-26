use crate::language::WithSpan;
use std::rc::Rc;
use std::collections::HashMap;
use crate::language::{SpanType, Type};
use crate::with_span;

pub fn construct_poly_for_free_vars(ty: SpanType) -> SpanType {
    fn collect_free(ty: &Type, free_vars: &mut Vec<String>) {
        match ty {
            Type::Var(name) => {free_vars.push(name.clone())}
            Type::Unit | Type::Bool | Type:: Int => {}
            Type::Poly { .. } => assert!(false, "unexpected poly type"),
            Type::Tuple(contents) => {
                for content in contents.iter() {
                    collect_free(&content.content, free_vars);
                }
            }
            Type::Arr(func, param) => {
                collect_free(&func.content, free_vars);
                collect_free(&param.content, free_vars);
            }
            Type::Ind { name, params } => {
                for param in params.iter() {
                    collect_free(&param.content, free_vars);
                }
            }
        }
    }


    let mut var_set = Vec::new();
    collect_free(&ty.content, &mut var_set);
    if var_set.is_empty() {
        ty
    } else {
        Rc::new(WithSpan::new(ty.span.clone(),  Type::Poly { vars: var_set.into_iter().collect(), body: ty.clone()}))
    }
}