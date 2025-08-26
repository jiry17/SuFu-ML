use std::rc::Rc;
use chumsky::prelude::SimpleSpan;
use serde::{Deserialize, Deserializer};
use crate::language::{Bind, BindTerm, Command, ContextEntry, DecoratedCommand, Pattern, SpanPattern, SpanType, Term, Type, WithSpan};
use crate::language::BindTerm::NormalBind;
use crate::with_span;

const DEFAULT_SPAN: SimpleSpan = SimpleSpan {start: 0, end: 1, context: ()};
impl<'de, T> Deserialize<'de> for WithSpan<T>
where
    T: Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let content = T::deserialize(deserializer)?;
        Ok(WithSpan {
            span: DEFAULT_SPAN,
            content,
        })
    }
}

#[derive(Deserialize)]
#[serde(tag="type")]
enum PatternRepr {
    #[serde(rename="var")]
    Var {
        name: String,
        content: Option<WithSpan<Pattern>>
    },
    #[serde(rename="tuple")]
    Tuple {
        fields: Vec<WithSpan<Pattern>>
    },
    #[serde(rename="underscore")]
    Wildcard,
    #[serde(rename="cons")]
    Cons {
        name: String,
        content: Option<WithSpan<Pattern>>
    }
}

impl<'de> Deserialize<'de> for Pattern {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let repr = PatternRepr::deserialize(deserializer)?;
        match repr {
            PatternRepr::Var { name, content } => {
                Ok(Pattern::Var(content.map(|x| Rc::new(x)), name))
            }
            PatternRepr::Tuple { fields } => {
                Ok(Pattern::Tuple(fields.into_iter().map(|x| Rc::new(x)).collect()))
            }
            PatternRepr::Wildcard => Ok(Pattern::Wildcard),
            PatternRepr::Cons {name, content} => {
                Ok(Pattern::Cons(name, content.map(|x| Rc::new(x))))
            }
        }
    }
}

#[derive(Deserialize)]
#[serde(tag="type")]
enum TypeRepr {
    #[serde(rename="int")]
    Int,
    #[serde(rename="unit")]
    Unit,
    #[serde(rename="bool")]
    Bool,
    #[serde(rename="var")]
    Var {name: String},
    #[serde(rename="poly")]
    Poly {vars: Vec<String>, body: WithSpan<Type>},
    #[serde(rename="arrow")]
    Arr {s: WithSpan<Type>, t: WithSpan<Type>},
    #[serde(rename="tuple")]
    Tuple {fields: Vec<WithSpan<Type>>},
    #[serde(rename="cons")]
    Ind {name: String, params: Vec<WithSpan<Type>>}
}

impl<'de> Deserialize<'de> for Type {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let type_repr = TypeRepr::deserialize(deserializer)?;
        let result: Type = match type_repr {
            TypeRepr::Int => Type::Int,
            TypeRepr::Unit => Type::Unit,
            TypeRepr::Bool => Type::Bool,
            TypeRepr::Var { name } => Type::Var(name),
            TypeRepr::Poly { vars, body } => Type::Poly {vars, body: body.into()},
            TypeRepr::Arr { s, t } => Type::Arr(Rc::new(s), Rc::new(t)),
            TypeRepr::Tuple { fields } =>
                Type::Tuple(fields.into_iter().map(|x| Rc::new(x)).collect()),
            TypeRepr::Ind { name, params } =>
                Type::Ind {name, params: params.into_iter().map(|x| Rc::new(x)).collect()},
        };
        Ok(result)
    }
}

#[derive(Deserialize)]
#[serde(tag="type")]
enum TermRepr {
    #[serde(rename="int")]
    Int {value: i32},
    #[serde(rename="true")]
    True,
    #[serde(rename="false")]
    False,
    #[serde(rename="unit")]
    Unit,
    #[serde(rename="if")]
    If {
        condition: WithSpan<Term>,
        #[serde(rename="true")]
        true_branch: WithSpan<Term>,
        #[serde(rename="false")]
        false_branch: WithSpan<Term>
    },
    #[serde(rename="var")]
    Var {name: String},
    #[serde(rename="op")]
    Op {
        operator: String,
        operand: Vec<WithSpan<Term>>
    },
    #[serde(rename="tuple")]
    Tuple {fields: Vec<WithSpan<Term>>},
    #[serde(rename="func")]
    Func {
        param: String,
        body: WithSpan<Term>
    },
    #[serde(rename="letrec")]
    LetRec {
        name: String, def: WithSpan<Term>,
        content: WithSpan<Term>
    },
    #[serde(rename="let")]
    Let {
        name: String,
        def: WithSpan<Term>,
        content: WithSpan<Term>
    },
    #[serde(rename="match")]
    Match {
        cases: Vec<(WithSpan<Pattern>, WithSpan<Term>)>,
        value: WithSpan<Term>
    },
    #[serde(rename="app")]
    App {
        func: WithSpan<Term>, param: WithSpan<Term>
    },
    #[serde(rename="cons")]
    Cons {
        name: String, content: WithSpan<Term>
    }
}

impl<'de> Deserialize<'de> for Term {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let term_repr = TermRepr::deserialize(deserializer)?;
        let term = match term_repr {
            TermRepr::Int {value} => Term::Int(value),
            TermRepr::True => Term::Bool(true),
            TermRepr::False => Term::Bool(false),
            TermRepr::Unit => Term::Unit,
            TermRepr::If { condition, true_branch, false_branch } =>
                Term::If(condition.into(), true_branch.into(), false_branch.into()),
            TermRepr::Var { name } => Term::Var(name),
            TermRepr::Op { operand, operator } =>
                Term::PrimOp {
                    op: operator,
                    params: operand.into_iter().map(|x| Rc::new(x)).collect()
                },
            TermRepr::Tuple { fields } =>
                Term::Tuple(fields.into_iter().map(|x| Rc::new(x)).collect()),
            TermRepr::Func { param, body } => Term::Func {
                params: vec![param], body: body.into()
            },
            TermRepr::LetRec { name, def, content } => {
                let bind = Bind { name, bind: NormalBind(def.into()), params: vec![], is_rec: true};
                Term::Let {bind: with_span!(DEFAULT_SPAN, bind), body: content.into()}
            }
            TermRepr::Let { name, def, content } => {
                let bind = Bind { name, bind: NormalBind(def.into()), params: vec![], is_rec: false};
                Term::Let {bind: with_span!(DEFAULT_SPAN, bind), body: content.into()}
            }
            TermRepr::Match { cases, value } => Term::Match {
                cases: cases.into_iter().map(|(a, b)| (a.into(), b.into())).collect(),
                def: value.into()
            },
            TermRepr::App { func, param } => Term::App(func.into(), param.into()),
            TermRepr::Cons { name, content } => Term::Cons {cons: name, body: content.into()}
        };
        Ok(term)
    }
}

#[derive(Deserialize)]
#[serde(tag="type")]
enum CommandRepr {
    #[serde(rename="func")]
    Func {
        name: String, def: WithSpan<Term>
    },
    #[serde(rename="bind")]
    Bind {
        name: String, def: WithSpan<Term>
    },
    #[serde(rename="declare")]
    Declare {
        name: String, ty: WithSpan<Type>
    },
    #[serde(rename="type")]
    TypeDef {
        name: String,
        arity: usize,
        cons: Vec<(String, WithSpan<Type>)>
    },
    #[serde(rename="eval")]
    TermEval {
        term: WithSpan<Term>
    }
}

impl<'de> Deserialize<'de> for Command {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let command_repr = CommandRepr::deserialize(deserializer)?;
        let command = match command_repr {
            CommandRepr::Func { name, def } => {
                let bind = Bind {name, bind: BindTerm::NormalBind(def.into()), is_rec: true, params: vec![]};
                Command::TermDef(with_span!(DEFAULT_SPAN, bind))
            }
            CommandRepr::Bind { name, def } => {
                let bind = Bind {name, bind: BindTerm::NormalBind(def.into()), is_rec: false, params: vec![]};
                Command::TermDef(with_span!(DEFAULT_SPAN, bind))
            }
            CommandRepr::Declare { name, ty } =>
                Command::TypeDeclare { name, ty: ty.into()},
            CommandRepr::TypeDef { name, arity, cons } => {
                let cons = cons.into_iter().map(|(s, t)| (s, t.into())).collect();
                Command::TypeDef {name, arity, cons_list: cons}
            }
            CommandRepr::TermEval { term } => Command::TermEval(term.into())
        };
        Ok(command)
    }
}

impl<'de> Deserialize<'de> for DecoratedCommand {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let command = Command::deserialize(deserializer)?;
        Ok(DecoratedCommand {command: with_span!(DEFAULT_SPAN, command), decos: vec![]})
    }
}

#[cfg(test)]
mod tests {
    use chumsky::input::BorrowInput;
    use chumsky::prelude::{*};
    use crate::language::Token;
    use crate::lexer;
    use crate::parser::pattern_parser::make_pattern_parser;
    use super::*;

    fn make_input<'src>(
        eoi: SimpleSpan,
        toks: &'src [(Token, SimpleSpan)],
    ) -> impl BorrowInput<'src, Token = Token, Span = SimpleSpan> {
        toks.map(eoi, |token| (&token.0, &token.1))
    }

    #[test]
    fn test_parse_serialize_deserialize() {
        let input_str = "Cons (_, (x, t) as xs)";


        let tokens = lexer::lexer().parse(input_str).into_result().unwrap();
        let pattern = make_pattern_parser(make_input)
            .parse(make_input((0..input_str.len()).into(), &tokens))
            .into_result()
            .unwrap();

        let json_str = serde_json::to_string(&pattern.content).expect("Failed to serialize A to JSON");

        let pattern: Pattern = serde_json::from_str(&json_str).expect("Failed to deserialize back to A");

        println!("{}", pattern);
    }
}