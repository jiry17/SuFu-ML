use std::rc::Rc;
use chumsky::{extra, input, Parser};
use chumsky::error::Rich;
use chumsky::input::BorrowInput;
use chumsky::pratt::*;
use chumsky::prelude::*;
use chumsky::span::SimpleSpan;
use crate::language::*;
use crate::spanned;

pub fn make_type_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M
) -> impl Parser<'tokens, I, SpanType, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src {
    let mut type_parser = Recursive::declare();

    let atom_parser = make_cons_parser(make_input, type_parser.clone());

    // build tuple parser
    let tuple_parser = atom_parser.separated_by(just(Token::Times))
        .at_least(1)
        .collect::<Vec<_>>()
        .map_with(|params, e| {
            if params.len() == 1 {
                params[0].clone()
            } else {
                spanned!(e, Type::Tuple(params))
            }
        });

    // build arrow parser
    let arrow_parser = tuple_parser.pratt(vec![
        infix(right(10), just(Token::Arrow), |x, _, y, e| {
            spanned!(e, Type::Arr(x, y))
        }).boxed()
    ]);

    type_parser.define(arrow_parser);

    type_parser
}

pub fn make_cons_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M, type_parser: impl Parser<'tokens, I, SpanType, extra::Err<Rich<'tokens, Token>>> + Clone
) -> impl Parser<'tokens, I, SpanType, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src
{
    let basic = select_ref! {
        Token::Var(var) => Type::Var(var.clone()),
        Token::Int => Type::Int,
        Token::Bool => Type::Bool,
        Token::Unit => Type::Unit,
        Token::Id(name) => Type::Ind {name: name.to_string(), params: vec![]},
    };
    let id = select_ref! {Token::Id(id) => id.to_string()};
    let make_input_copy = make_input.clone();
    let atom = choice((
        basic.map_with(|b, e| spanned!(e, b)),
        type_parser.clone().nested_in(select_ref! { Token::Parens(ts) = e => make_input(e.span(), ts) }),
    ));

    let param_list = type_parser.clone()
        .separated_by(just(Token::Comma))
        .at_least(2)
        .collect::<Vec<_>>();

    choice((
        param_list.nested_in(select_ref! { Token::Parens(ts) = e => make_input_copy(e.span(), ts) })
            .then(id)
            .map_with(|(param, name), e| {
                spanned!(e, Type::Ind {params: param, name})
            }),
        atom.clone().or_not().then(id)
            .map_with(|(param, name), e| {
                spanned!(e, Type::Ind { params: param.into_iter().collect(), name })
            }),
        atom,
    ))
}