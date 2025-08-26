use crate::language::SpanBind;
use crate::language::WithSpan;
use std::rc::Rc;
use chumsky::error::Rich;
use chumsky::{extra, select_ref, Parser};
use chumsky::input::{BorrowInput, MapExtra};
use chumsky::IterParser;
use chumsky::pratt::{infix, left, prefix, Boxed};
use chumsky::pratt::Operator;
use chumsky::prelude::{choice, empty, just, Recursive, SimpleSpan};
use crate::language::{Bind, BindTerm, SpanPattern, SpanTerm, Spanned, Term, Token};
use crate::parser::pattern_parser::make_pattern_parser;
use crate::spanned;

macro_rules! create_prefix_operator {
    ($power:expr, $operator:expr) => {
        prefix($power, just($operator), |op, x, e| {
            spanned!(e, Term::PrimOp {op: format!("{op}"), params: vec![x]})
        }).boxed()
    };
}

macro_rules! create_left_infix_operator {
    ($power:expr, $operator:expr) => {
        infix(left($power), just($operator), |x, op, y, e| {
            spanned!(e, Term::PrimOp {op: format!("{op}"), params: vec![x, y]})
        }).boxed()
    };
}

pub fn make_term_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M
) -> impl Parser<'tokens, I, SpanTerm, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src
{
    let mut term_parser = Recursive::declare();

    // atomic tokens
    let atomic = select_ref! {
        Token::UnitVal => Term::Unit,
        Token::IntVal(n) => Term::Int(*n),
        Token::BoolVal(b) => Term::Bool(*b),
        Token::Id(name) => Term::Var(String::from(name)),
        Token::Cons(name) => Term::NativeCons(String::from(name))
    };

    // build atomic term
    let make_input_copy = make_input.clone();
    let tuple_parser = term_parser.clone()
        .separated_by(just(Token::Comma))
        .at_least(2)
        .collect::<Vec<_>>()
        .nested_in(select_ref! {Token::Parens(ts) = e => make_input_copy(e.span(), ts)});
    let make_input_copy = make_input.clone();
    let atomic_parser = choice((
        atomic.map_with(|t, e| spanned!(e, t)),
        tuple_parser.map_with(|elements, e| spanned!(e, Term::Tuple(elements))),
        term_parser.clone().nested_in(select_ref! { Token::Parens(ts) = e => make_input_copy(e.span(), ts) })
    ));

    // build function call
    let call_parser = atomic_parser.clone()
        .foldl_with(
            atomic_parser.repeated(),
            |f, p, e|
                spanned!(e, Term::App(f, p))
        );

    let mut operators: Vec<Boxed<I, SpanTerm, extra::Err<Rich<'tokens, Token>>>> = Vec::default();

    operators.push(create_prefix_operator!(20, Token::Dash));
    operators.push(create_prefix_operator!(3, Token::Not));

    operators.push(create_left_infix_operator!(10, Token::Times));
    operators.push(create_left_infix_operator!(10, Token::Slash));
    operators.push(create_left_infix_operator!(5, Token::Plus));
    operators.push(create_left_infix_operator!(5, Token::Dash));
    operators.push(create_left_infix_operator!(4, Token::EqEq));
    operators.push(create_left_infix_operator!(4, Token::Lq));
    operators.push(create_left_infix_operator!(4, Token::Gq));
    operators.push(create_left_infix_operator!(4, Token::Leq));
    operators.push(create_left_infix_operator!(4, Token::Geq));
    operators.push(create_left_infix_operator!(2, Token::And));
    operators.push(create_left_infix_operator!(1, Token::Or));

    // build arithmatic
    let arith_parser = call_parser.pratt(operators);

    // let bind TODO: add pattern bind
    let pattern_parser = make_pattern_parser(make_input.clone());
    let case_parser = make_cases_parser(pattern_parser, term_parser.clone());
    let bind_parser = make_bind_parser(case_parser.clone(), term_parser.clone());
    let let_parser = just(Token::Let)
        .ignore_then(bind_parser)
        .then_ignore(just(Token::In))
        .then(term_parser.clone())
        .map_with(
            |(bind, body), e| {
                spanned!(e, Term::Let {bind, body})
            }
        );

    // if then else
    let ite_parser = just(Token::If)
        .ignore_then(term_parser.clone())
        .then(just(Token::Then).ignore_then(term_parser.clone()))
        .then(just(Token::Else).ignore_then(term_parser.clone()))
        .map_with(
            |((c, t), f), e|
                spanned!(e, Term::If(c, t, f))
        );

    // match case
    let match_parser = just(Token::Match)
        .ignore_then(term_parser.clone())
        .then_ignore(just(Token::With))
        .then(just(Token::Vbar).or_not().ignore_then(case_parser))
        .map_with(
            |(def, cases), e|
                spanned!(e, Term::Match {def, cases})
        );

    // fun case
    let id = select_ref! { Token::Id(name) => String::from(name) };
    let fun_parser = just(Token::Fun)
        .ignore_then(id.repeated().collect::<Vec<_>>())
        .then_ignore(just(Token::Arrow))
        .then(term_parser.clone())
        .map_with(
            |(params, body), e|
                spanned!(e, Term::Func {params, body})
        );

    let merged_parser = choice((arith_parser, let_parser, ite_parser, match_parser, fun_parser));

    term_parser.define(merged_parser);
    term_parser
}

pub fn make_cases_parser<'tokens, 'src: 'tokens, I>(
    pattern_parser: impl Parser<'tokens, I, SpanPattern, extra::Err<Rich<'tokens, Token>>> + Clone,
    term_parser: impl Parser<'tokens, I, SpanTerm, extra::Err<Rich<'tokens, Token>>> + Clone
) -> impl Parser<'tokens, I, Vec<(SpanPattern, SpanTerm)>, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan> {
    let cases = pattern_parser
        .then_ignore(just(Token::Arrow))
        .then(term_parser)
        .separated_by(just(Token::Vbar))
        .collect::<Vec<_>>();
    just(Token::Vbar).or_not().ignore_then(cases)
}

pub fn make_bind_parser<'tokens, 'src: 'tokens, I>(
    cases_parser: impl Parser<'tokens, I, Vec<(SpanPattern, SpanTerm)>, extra::Err<Rich<'tokens, Token>>> + Clone,
    term_parser: impl Parser<'tokens, I, SpanTerm, extra::Err<Rich<'tokens, Token>>> + Clone
) -> impl Parser<'tokens, I, SpanBind, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan> {
    let id = select_ref! {Token::Id(name) => String::from(name)};

    let head_parser = id.then(id.repeated().collect::<Vec<_>>());

    let case1 = just(Token::Rec).or_not()
        .then(head_parser.clone())
        .then_ignore(just(Token::Eq))
        .then(term_parser.clone())
        .map_with(
            |((token, (name, params)), def), e|
                spanned!(e, Bind {bind: BindTerm::NormalBind(def), name, params, is_rec: token.is_some()})
        );
    let case2 = just(Token::Rec).or_not()
        .then(head_parser.clone())
        .then_ignore(just(Token::Eq))
        .then_ignore(just(Token::Function))
        .then(cases_parser)
        .map_with(
            |((token, (name, mut params)), cases), e| {
                spanned!(e, Bind {bind: BindTerm::FuncBind(cases), name, params, is_rec: token.is_some()})
            }
        );
    choice((case1, case2))
}