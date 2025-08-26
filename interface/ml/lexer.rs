use ariadne::{sources, Color, Label, Report, ReportKind};
use chumsky::{input::BorrowInput, pratt::*, prelude::*};
use crate::language::{Token};


pub fn lexer<'src>(
) -> impl Parser<'src, &'src str, Vec<(Token, SimpleSpan)>, extra::Err<Rich<'src, char>>> {
    recursive(|token| {
        choice((
            // Keywords
            text::ident().map(|s| match s {
                "let" => Token::Let,
                "int" => Token::Int,
                "bool" => Token::Bool,
                "unit" => Token::Unit,
                "in" => Token::In,
                "fun" => Token::Fun,
                "if" => Token::If,
                "eval" => Token::Eval,
                "then" => Token::Then,
                "function" => Token::Function,
                "else" => Token::Else,
                "true" => Token::BoolVal(true),
                "false" => Token::BoolVal(false),
                "as" => Token::As,
                "not" => Token::Not,
                "match" => Token::Match,
                "with" => Token::With,
                "rec" => Token::Rec,
                "_" => Token::Wildcard,
                "type" => Token::Type,
                "of" => Token::Of,
                "val" => Token::Val,
                s => {
                    let first_char = s.chars().next().unwrap();
                    if first_char.is_uppercase() {
                        Token::Cons(s.to_string())
                    } else {
                        Token::Id(s.to_string())
                    }
                }
            }),
            just("'").ignore_then(text::ident().map(
                |s| Token::Var(format!("'{s}"))
            )),
            // Operators
            just("->").to(Token::Arrow),
            just("()").to(Token::UnitVal),
            just("@").to(Token::Deco),
            just("==").to(Token::EqEq),
            just("|").to(Token::Vbar),
            just("<=").to(Token::Leq),
            just(">=").to(Token::Geq),
            just("<").to(Token::Lq),
            just(">").to(Token::Gq),
            just(":").to(Token::Colon),
            just("=").to(Token::Eq),
            just("&&").to(Token::And),
            just("||").to(Token::Or),
            just("+").to(Token::Plus),
            just("*").to(Token::Times),
            just(".").to(Token::Dot),
            just(",").to(Token::Comma),
            just("_").to(Token::Wildcard),
            just("/").to(Token::Slash),
            // Numbers
            just("-").or_not().then(text::int(10).to_slice())
                .map(|s: (Option<&str>, &str)| {
                    let (op, s) = s;
                    let i: i32 = s.parse().unwrap();
                    if op.is_some() {Token::IntVal(-i)} else {Token::IntVal(i)}
                }),
            just("-").to(Token::Dash),
            token
                .repeated()
                .collect()
                .delimited_by(just('('), just(')'))
                .labelled("token tree")
                .as_context()
                .map(Token::Parens),
        ))
            .map_with(|t, e| (t, e.span()))
            .padded()
    })
        .repeated()
        .collect()
}
