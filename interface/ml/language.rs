use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt;
use std::ops::Deref;
use std::rc::Rc;
use chumsky::extra::ParserExtra;
use chumsky::input::{Input, MapExtra};
use chumsky::span::SimpleSpan;
use either::Either;

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Id(String), Cons(String),
    Var(String),
    IntVal(i32), BoolVal(bool), UnitVal,
    Parens(Vec<(Self, SimpleSpan)>),
    Let, In, Fun, If, Then, Else, Of, Eval,
    Eq, Plus, Times, Dot, Int, Bool, Unit, Dash,
    Slash, EqEq, Lq, Leq, Gq, Geq, And, Or, Not,
    Arrow, Comma, Wildcard, As, Rec, Vbar,
    Match, With, Function, Type, Val, Colon, Deco
}

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Var(String),
    Unit, Bool, Int,
    Poly {vars: Vec<String>, body: SpanType},
    Tuple(Vec<SpanType>),
    Arr (SpanType, SpanType),
    Ind {name: String, params: Vec<SpanType>}
}

#[derive(Debug, Clone, PartialEq)]
pub struct WithSpan<T> {
    pub span: SimpleSpan,
    pub content: T
}

impl<T> WithSpan<T> {
    pub fn new(span: SimpleSpan, content: T) -> Self {
        Self {span, content}
    }
}

#[macro_export] macro_rules! spanned {
    ($e:expr, $content:expr) => {
        Rc::new(WithSpan::new($e.span(), $content))
    };
}
#[macro_export] macro_rules! with_span {
    ($e:expr, $content:expr) => {
        Rc::new(WithSpan::new($e.clone(), $content))
    };
}

#[macro_export] macro_rules! extract {
    ($expression:expr, $pattern:pat => $result:expr) => {
        match $expression {
            $pattern => $result,
            _ => panic!("pattern mismatch"),
        }
    };
}

pub type Spanned<T> = Rc<WithSpan<T>>;

pub type SpanType = Spanned<Type>;
pub type SpanPattern = Spanned<Pattern>;
pub type SpanTerm = Spanned<Term>;



#[derive(Debug, Clone, PartialEq)]
pub enum Pattern {
    Wildcard,
    Var(Option<SpanPattern>, String),
    Tuple(Vec<SpanPattern>),
    Cons(String, Option<SpanPattern>)
}

#[derive(Debug, Clone, PartialEq)]
pub enum Term {
    Int(i32), Bool(bool), Unit,
    If(SpanTerm, SpanTerm, SpanTerm),
    Var(String),
    PrimOp {op: String, params: Vec<SpanTerm>},
    App(SpanTerm, SpanTerm),
    Func {params: Vec<String>, body: SpanTerm},
    Tuple (Vec<SpanTerm>),
    Match {def: SpanTerm, cases: Vec<(SpanPattern, SpanTerm)>},
    Cons {cons: String, body: SpanTerm},
    Let {bind: SpanBind, body: SpanTerm},

    // Temporary Structures
    NativeCons(String)
}

#[derive(Debug, Clone, PartialEq)]
pub enum BindTerm {
    NormalBind(SpanTerm),
    FuncBind(Vec<(SpanPattern, SpanTerm)>)
}

#[derive(Debug, Clone, PartialEq)]
pub struct Bind {
    pub bind: BindTerm,
    pub name: String,
    pub params: Vec<String>,
    pub is_rec: bool
}

impl Bind {
    pub fn clone_with_new_bind(&self, bind: BindTerm) -> Self {
        Self {bind, name: self.name.clone(), params: self.params.clone(), is_rec: self.is_rec}
    }
}

pub type SpanBind = Spanned<Bind>;

#[derive(Debug, Clone, PartialEq)]
pub enum ConfigVal {
    Int(i32), Bool(bool), String(String)
}

#[derive(Debug, Clone, PartialEq)]
pub enum Command {
    Config {name: String, val: ConfigVal},
    TypeDef {name: String, cons_list: Vec<(String, SpanType)>, arity: usize},
    TypeAlias {name: String, def: SpanType},
    TermEval(SpanTerm),
    TypeDeclare {name: String, ty: SpanType},
    TermDef(SpanBind)
}

pub type SpanCommand = Spanned<Command>;

#[derive(Debug, Clone, PartialEq)]
pub struct DecoratedCommand {
    pub command: SpanCommand,
    pub decos: Vec<String>
}

pub type Program = Vec<DecoratedCommand>;
pub type ContextBind = Either<SpanTerm, SpanType>;


pub enum ContextEntry<T> {
    Empty,
    Bind {name: String, info: T, next: Rc<RefCell<Self>>}
}

impl<T> Default for ContextEntry<T> {
    fn default() -> Self { ContextEntry::Empty }
}

pub type Context<T> = Rc<RefCell<ContextEntry<T>>>;
pub type DefaultContext = Context<ContextBind>;

pub fn bind_into<T>(name: String, info: T, context: Context<T>) -> Context<T> {
    Rc::new(RefCell::new(ContextEntry::Bind {name, info, next: context }))
}

pub fn look_up<T: Clone>(target: &str, context: &Context<T>) -> Option<T> {
    match context.borrow().deref() {
        ContextEntry::Empty => None,
        ContextEntry::Bind { name, info, next } => {
            if name == target {Some(info.clone())} else {look_up(target, next)}
        }
    }
}


impl fmt::Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Token::Id(x) => write!(f, "{x}"),
            Token::Var(x) => write!(f, "{x}"),
            Token::Parens(_) => write!(f, "(...)"),
            Token::Arrow => write!(f, "->"),
            Token::Plus => write!(f, "+"),
            Token::Times => write!(f, "*"),
            Token::Comma => write!(f, ","),
            Token::As => write!(f, "as"),
            Token::Rec => write!(f, "rec"),
            Token::Unit => write!(f, "unit"),
            Token::Function => write!(f, "function"),
            Token::UnitVal => write!(f, "()"),
            Token::Dash => write!(f, "-"),
            Token::Eval => write!(f, "eval"),
            Token::Slash => write!(f, "/"),
            Token::Eq => write!(f, "="),
            Token::EqEq => write!(f, "=="),
            Token::Lq => write!(f, "<"),
            Token::Match => write!(f, "match"),
            Token::With => write!(f, "with"),
            Token::Gq => write!(f, ">"),
            Token::Leq => write!(f, "<="),
            Token::Geq => write!(f, ">="),
            Token::And => write!(f, "&&"),
            Token::Or => write!(f, "||"),
            Token::Not => write!(f, "not"),
            Token::If => write!(f, "if"),
            Token::Then => write!(f, "then"),
            Token::Else => write!(f, "else"),
            Token::Fun => write!(f, "fun"),
            Token::Of => write!(f, "of"),
            Token::Val => write!(f, "val"),
            Token::Colon => write!(f, ":"),
            Token::Deco => write!(f, "@"),
            Token::Cons(cons) => write!(f, "{}", cons),
            Token::IntVal(i) => write!(f, "{}", i),
            Token::BoolVal(b) => if *b {write!(f, "true")} else {write!(f, "false")}
            Token::Let => write!(f, "let"),
            Token::In => write!(f, "in"),
            Token::Dot => write!(f, "."),
            Token::Int => write!(f, "int"),
            Token::Bool => write!(f, "bool"),
            Token::Wildcard => write!(f, "_"),
            Token::Vbar => write!(f, "|"),
            Token::Type => write!(f, "type")
        }
    }
}