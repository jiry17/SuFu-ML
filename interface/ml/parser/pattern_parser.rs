use std::rc::Rc;
use chumsky::error::Rich;
use chumsky::{extra, Parser};
use chumsky::input::BorrowInput;
use chumsky::prelude::{SimpleSpan, select_ref, recursive, choice, just, Recursive};
use chumsky::IterParser;
use chumsky::primitive::{select, select_ref};
use crate::language::{SpanPattern, Pattern, Spanned, Token, WithSpan};
use crate::language::Token::With;
use crate::spanned;

pub fn make_pattern_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M
) -> impl Parser<'tokens, I, SpanPattern, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src {

    let mut pattern_parser = Recursive::declare();

    let id = select_ref! { Token::Id(name) => String::from(name) };
    let cons = select_ref! { Token::Cons(name) => String::from(name) };
    let make_input_copy = make_input.clone();

    // consider tuple
    let tuple_parser = pattern_parser.clone()
        .separated_by(just(Token::Comma))
        .at_least(2)
        .collect::<Vec<_>>()
        .nested_in(
            select_ref! { Token::Parens(ts) = e => make_input_copy(e.span(), ts) }
        )
        .map_with(|contents, e| {
            spanned!(e, Pattern::Tuple(contents))
        });

    let basic_parser = choice((
        tuple_parser,
        just(Token::Wildcard).map_with(|_, e| spanned!(e, Pattern::Wildcard)),
        id.map_with(|name, e| spanned!(e, Pattern::Var(None, name))),
        pattern_parser.clone().nested_in(select_ref! { Token::Parens(ts) = e => make_input(e.span(), ts) })
    ));

    // consider only unary constructor
    let cons_parser = cons.then(basic_parser.clone().or_not()).map_with(
        |(name, sub), e| {
            spanned!(e, Pattern::Cons(name, sub))
        }
    );
    let cons_parser = choice((cons_parser, basic_parser, ));

    // add as binding
    let as_parser = cons_parser.then(
        just(Token::As).ignore_then(id).or_not()
    ).map_with(
        |(pattern, name), e| {
            if let Some(name) = name {
                spanned!(e, Pattern::Var(Some(pattern), name))
            } else {
                pattern
            }
        }
    );

    pattern_parser.define(as_parser);
    pattern_parser
}