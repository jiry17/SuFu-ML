use std::rc::Rc;
use chumsky::error::Rich;
use chumsky::{extra, select_ref, Parser, IterParser};
use chumsky::input::BorrowInput;
use chumsky::prelude::{choice, just, SimpleSpan};
use crate::language::{Command, DecoratedCommand, Program, SpanCommand, SpanTerm, SpanType, Spanned, Token, Type, WithSpan};
use crate::parser::pattern_parser::make_pattern_parser;
use crate::parser::term_parser::{make_bind_parser, make_cases_parser, make_term_parser};
use crate::parser::type_parser::make_type_parser;
use crate::parser::util::construct_poly_for_free_vars;
use crate::spanned;

pub fn make_command_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M
) -> impl Parser<'tokens, I, SpanCommand, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src {
    let id = select_ref! {Token::Id(name) => String::from(name)};
    let cons = select_ref! {Token::Cons(name) => String::from(name)};
    let var = select_ref! {Token::Var(name) => String::from(name)};
    let var_list = var.repeated().collect::<Vec<_>>();

    let type_parser = make_type_parser(make_input.clone());
    // type rename
    let type_alias_parser = just(Token::Type)
        .ignore_then(var_list)
        .then(id)
        .then_ignore(just(Token::Eq))
        .then(type_parser.clone())
        .map_with(
            |((params, name), type_info), e| {
                let mut new_type = type_info.clone();
                if !params.is_empty() {
                    new_type = Rc::new(WithSpan::new(
                        type_info.span.clone(), Type::Poly {vars: params, body: new_type}
                    ));
                }
                spanned!(e, Command::TypeAlias {name, def: new_type})
            }
        );

    // inductive type define
    let cons_parser = cons
        .then(
            (just(Token::Of).ignore_then(type_parser.clone())).or_not()
        )
        .map_with(
            |(name, ty), e|
                (name, ty, e.span())
        );
    let cons_list_parser = cons_parser.separated_by(just(Token::Vbar))
        .at_least(1)
        .collect::<Vec<_>>();
    let type_def_parser = just(Token::Type)
        .ignore_then(var_list)
        .then(id)
        .then_ignore(just(Token::Eq))
        .then_ignore(just(Token::Vbar).or_not())
        .then(cons_list_parser)
        .map_with(
            |((params, type_name), mut cons_list), e| {
                let mut cons_infos = vec![];
                let var_list: Vec<_> = params.iter().map(
                    |var_name| spanned!(e, Type::Var(var_name.clone()))
                ).collect();

                let full_type = spanned!(e, Type::Ind {name: type_name.clone(), params: var_list});
                for (name, info, span) in cons_list {
                    let mut cons_type = if let Some(info_type) = info {
                        spanned!(e, Type::Arr(info_type, full_type.clone()))
                    } else {
                        full_type.clone()
                    };
                    if !params.is_empty() {
                        cons_type = spanned!(e, Type::Poly {vars: params.clone(), body: cons_type});
                    }
                    cons_infos.push((name, cons_type));
                }

                spanned!(e, Command::TypeDef {name: type_name, cons_list: cons_infos, arity: params.len()})
            }
        );

    // type declare
    let type_declare_parser = just(Token::Val)
        .ignore_then(id)
        .then_ignore(just(Token::Colon))
        .then(type_parser.clone())
        .map_with(
            |(name, mut ty), e| {
                ty = construct_poly_for_free_vars(ty);
                spanned!(e, Command::TypeDeclare { name, ty })
            }
        );

    // term eval
    let term_parser = make_term_parser(make_input.clone());

    let term_eval_parser = just(Token::Eval).ignore_then(term_parser.clone())
        .map_with(
            |term, e| spanned!(e, Command::TermEval(term))
        );

    // term def
    let pattern_parser = make_pattern_parser(make_input.clone());
    let cases_parser = make_cases_parser(pattern_parser, term_parser.clone());
    let bind_parser = make_bind_parser(cases_parser, term_parser.clone());

    let term_bind_parser = just(Token::Let).ignore_then(bind_parser).map_with(
        |bind, e| spanned!(e, Command::TermDef(bind))
    );

    choice ((type_alias_parser, type_def_parser, type_declare_parser, term_bind_parser, term_eval_parser))
}

pub fn make_program_parser<'tokens, 'src: 'tokens, I, M>(
    make_input: M
) -> impl Parser<'tokens, I, Program, extra::Err<Rich<'tokens, Token>>> + Clone
where
    I: BorrowInput<'tokens, Token = Token, Span = SimpleSpan>,
    M: Fn(SimpleSpan, &'tokens [(Token, SimpleSpan)]) -> I + Clone + 'src {
    let command_parser = make_command_parser(make_input);
    let deco_info = select_ref! {
        Token::Id(info) => info.clone(),
        Token::Cons(name) => name.clone()
    };
    let decorated_parser = just(Token::Deco)
        .ignore_then(deco_info)
        .repeated()
        .collect::<Vec<_>>()
        .then(command_parser)
        .map(
            |(deco_list, command)|
                DecoratedCommand {command, decos: deco_list}
        );

    decorated_parser.repeated().collect::<Vec<_>>()
}