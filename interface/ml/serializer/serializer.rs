use std::collections::HashMap;
use std::fmt::Formatter;
use std::rc::Rc;
use chumsky::span::SimpleSpan;
use phf::{phf_map};
use serde::{Serialize, Serializer};
use serde::ser::{SerializeStruct, SerializeTuple};
use crate::extract;
use crate::language::{BindTerm, Command, ConfigVal, DecoratedCommand, Pattern, SpanPattern, SpanTerm, Term, Type};
use crate::serializer::{ConsInfo, MatchCase};


macro_rules! serialize_struct {
    (
    $serializer:ident,
    $struct_name:expr,
    $type_value:expr
    ) => {{
        let mut state = $serializer.serialize_struct($struct_name, 1)?;
        state.serialize_field("type", $type_value)?;
        state.end()
    }};
    (
        $serializer:ident,
        $struct_name:expr,
        $type_value:expr,
        $arity:expr,
        $($field_name:expr => $field_value:expr),* $(,)?
    ) => {{
        let mut state = $serializer.serialize_struct($struct_name, $arity)?;
        state.serialize_field("type", $type_value)?;
        $(
            state.serialize_field($field_name, &$field_value)?;
        )*
        state.end()
    }};
}

impl Serialize for Pattern {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Pattern::Var(None, var) => {
                serialize_struct!(serializer, "PatternVar", "var", 2, "name" => var)
            }
            Pattern::Var(Some(pt), var) => {
                serialize_struct!(
                    serializer, "PatternVar", "var", 3,
                    "name" => var, "content" => &pt.content
                )
            }
            Pattern::Tuple(contents) => {
                let contents: Vec<_> = contents.iter().map(|c| &c.content).collect();
                serialize_struct!(
                    serializer, "PatternTuple", "tuple", 2,
                    "fields" => &contents
                )
            }
            Pattern::Wildcard => {
                serialize_struct!(
                    serializer, "PatternWildcard", "underscore"
                )
            },
            Pattern::Cons(cons, Some(body)) => {
                serialize_struct!(
                    serializer, "PatternCons", "cons", 3,
                    "name" => cons,
                    "content" => &body.content
                )
            },
            _ => panic!("Unexpected internal pattern {}", self)
        }
    }
}

struct TypeWithIndices<'a> {
    ty: &'a Type,
    index_map: Rc<HashMap<String, usize>>
}

impl<'a> TypeWithIndices<'a> {
    fn new(ty: &'a Type, index_map: Rc<HashMap<String, usize>>) -> Self {
        Self {ty, index_map}
    }
}

impl Serialize for TypeWithIndices<'_> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // println!("try serialize {}", self.ty);
        match self.ty {
            Type::Int => {
                serialize_struct!(serializer, "TypeInt", "int")
            }
            Type::Unit => {
                serialize_struct!(serializer, "TypeUnit", "unit")
            }
            Type::Bool => {
                serialize_struct!(serializer, "TypeBool", "bool")
            }
            Type::Var(name) => {
                if !self.index_map.contains_key(name) {
                    panic!("Unknown variable name {}", name);
                }
                let index = self.index_map[name];
                serialize_struct!(
                    serializer, "TypeVar", "var", 2,
                    "index" => &index
                )
            }
            Type::Arr(inp, oup) => {
                let inp = Self::new(&inp.content, self.index_map.clone());
                let oup = Self::new(&oup.content, self.index_map.clone());
                serialize_struct!(
                    serializer, "TypeArr", "arrow", 3,
                    "s" => &inp, "t" => &oup
                )
            }
            Type::Tuple(contents) => {
                let contents: Vec<_> = contents.iter()
                    .map(|content| Self::new(&content.content, self.index_map.clone()))
                    .collect();
                serialize_struct!(
                    serializer, "TypeTuple", "tuple", 2,
                    "fields" => &contents
                )
            }
            Type::Ind {name, params} => {
                let params: Vec<_> = params.iter()
                    .map(|param| Self::new(&param.content, self.index_map.clone()))
                    .collect();
                serialize_struct!(
                    serializer, "TypeInd", "cons", 3,
                    "name" => name, "params" => &params
                )
            }
            Type::Poly { vars, body } => {
                assert!(self.index_map.is_empty());
                let index_map: HashMap<_, _> = vars.iter().enumerate()
                    .map(|(i, name)| (name.clone(), i))
                    .collect();

                let body = Self::new(&body.content, Rc::new(index_map));
                serialize_struct!(
                    serializer, "TypePoly", "poly", 3,
                    "vars" => &vars, "body" => &body
                )
            }
        }
    }
}

impl Serialize for Type {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let expanded = TypeWithIndices::new(self, Rc::default());
        expanded.serialize(serializer)
    }
}

impl Serialize for Term {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Term::Int(i) => {
                serialize_struct!(serializer, "TermInt", "int", 2, "value" => i)
            },
            Term::Bool(b) => {
                serialize_struct!(serializer, "TermBool", if *b {"true"} else {"false"})
            },
            Term::Unit => {
                serialize_struct!(serializer, "TermUnit", "unit")
            },
            Term::If(c, t, f) => {
                serialize_struct!(
                    serializer, "TermIf", "if", 4,
                    "condition" => &c.content,
                    "true" => &t.content,
                    "false" => &f.content
                )
            },
            Term::Var(name) => {
                serialize_struct!(
                    serializer, "TermVar", "var", 2,
                    "name" => name
                )
            },
            Term::PrimOp {op, params} => {
                let params: Vec<_> = params.iter().map(|t| t.content.clone()).collect();
                serialize_struct!(
                    serializer, "TermPrimOp", "op", 3,
                    "operator" => &op, "operand" => &params
                )
            },
            Term::Tuple(contents) => {
                let contents: Vec<_> = contents.iter()
                    .map(|t| t.content.clone())
                    .collect();
                serialize_struct!(
                    serializer, "TermTuple", "tuple", 2,
                    "fields" => &contents
                )
            },
            Term::Func {params, body} => {
                assert_eq!(params.len(), 1);
                serialize_struct!(
                    serializer, "TermFunc", "func", 3,
                    "name" => &params[0], "content" => &body.content
                )
            },
            Term::Let {bind, body} => {
                let bind = &bind.content;
                let term = extract!(&bind.bind, BindTerm::NormalBind(t) => t);
                assert!(bind.params.is_empty());

                serialize_struct!(
                    serializer, "TermLet", if bind.is_rec {"letrec"} else {"let"}, 4,
                    "name" => &bind.name, "def" => &term.content,
                    "content" => &body.content
                )
            },
            Term::Match {def, cases} => {
                let cases: Vec<_> = cases.iter()
                    .map(|case| MatchCase::from_case(case))
                    .collect();

                serialize_struct!(
                    serializer, "TermMatch", "match", 3,
                    "cases" => &cases, "value" => &def.content
                )
            }
            Term::App(f, p) => {
                serialize_struct!(
                    serializer, "TermApp", "app", 2,
                    "func" => &f.content, "param" => &p.content
                )
            }
            Term::Cons { cons, body} => {
                serialize_struct!(
                    serializer, "TermCons", "cons", 2,
                    "name" => cons, "content" => &body.content
                )
            }
            _ => panic!("Unsupported term {}", self.to_pretty(20))
        }
    }
}

fn _get_arity(command: &Command) -> usize {
    match command {
        Command::TermDef(_) => 3,
        Command::TermEval(_) | Command::TypeAlias {..} => panic!("Unsupported command"),
        Command::TypeDef { ..} => 4,
        Command::Config {..} => 3,
        Command::TypeDeclare { .. } => 3
    }
}

impl Serialize for ConfigVal {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        match self {
            ConfigVal::Int(i) => {
                serialize_struct!(serializer, "ConfigVal", "int", 2, "value" => i)
            },
            ConfigVal::Bool(b) => {
                serialize_struct!(serializer, "ConfigVal", "bool", 2, "value" => b)
            },
            ConfigVal::String(str) => {
                serialize_struct!(serializer, "ConfigVal", "string", 2, "value" => str)
            }
        }
    }
}

impl Serialize for DecoratedCommand {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match &self.command.content {
            Command::Config {name, val} => {
                serialize_struct!(
                    serializer, "CommandConfig", "config", 4,
                    "decos" => &self.decos,
                    "name" => name,
                    "value" => val
                )
            },
            Command::TermDef(bind) => {
                let content = &bind.content;
                let term = extract!(&content.bind, BindTerm::NormalBind(t) => t);
                serialize_struct!(
                    serializer, "CommandTermDef",
                    if content.is_rec {"func"} else {"bind"},
                    3,
                    "name" => &content.name,
                    "def" => &term.content
                )
            }
            Command::TypeDeclare {name, ty} => {
                serialize_struct!(
                    serializer, "CommandTypeDeclare", "declare", 3,
                    "name" => name,
                    "ty" => &ty.content
                )
            },
            Command::TypeDef {name, arity, cons_list} => {
                let cons_list: Vec<_> = cons_list.iter()
                    .map(|info| ConsInfo::from_info(info))
                    .collect();
                serialize_struct!(
                    serializer, "CommandTypeDef", "type", 4,
                    "name" => name,
                    "arity" => arity,
                    "cons" => cons_list
                )
            }
            Command::TermEval(term) => {
                serialize_struct!(
                    serializer, "TermEval", "eval", 2,
                    "term" => &term.content
                )
            }
            Command::TypeAlias {..} => panic!("Unsupported command"),
        }
    }
}
