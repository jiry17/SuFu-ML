use serde::{Deserialize, Serialize};
use crate::language::{Pattern, SpanPattern, Term, SpanTerm, Type, SpanType};

pub mod serializer;
mod deserializer;

#[derive(Serialize)]
struct MatchCase {
    pattern: Pattern,
    branch: Term
}

impl MatchCase {
    pub fn from_case(case: &(SpanPattern, SpanTerm)) -> MatchCase {
        Self {pattern: case.0.content.clone(), branch: case.1.content.clone()}
    }
}

#[derive(Serialize)]
struct ConsInfo {
    name: String,
    #[serde(rename = "type")]
    ty: Type
}

impl ConsInfo {
    pub fn from_info(cons_info: &(String, SpanType)) -> ConsInfo {
        Self {name: cons_info.0.clone(), ty: cons_info.1.content.clone()}
    }
}