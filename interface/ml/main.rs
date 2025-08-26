use std::{fmt, fs};
use std::slice::SplitInclusiveMut;
use ariadne::{sources, Color, Label, Report, ReportKind};
use chumsky::input::BorrowInput;
use chumsky::Parser;
use chumsky::prelude::{*};
use crate::language::{Spanned, Token, WithSpan};
use crate::parser::pattern_parser::make_pattern_parser;
use crate::parser::program_parser::{make_command_parser, make_program_parser};
use crate::parser::term_parser::make_term_parser;
use crate::parser::type_parser::make_type_parser;
use crate::printer::term_printer::ProgramPrinter;
use crate::processor::{internal_to_surface, surface_to_internal};

mod language;

mod lexer;
mod parser;
mod util;

mod processor;
mod printer;
mod serializer;

const SRC: &str = "/Users/pro/Desktop/work/2025S/SuFu-related-repos/ML-Interface";

fn make_input<'src>(
    eoi: SimpleSpan,
    toks: &'src [(Token, SimpleSpan)],
) -> impl BorrowInput<'src, Token = Token, Span = SimpleSpan> {
    toks.map(eoi, |token| (&token.0, &token.1))
}

fn failure(
    msg: String,
    label: (String, SimpleSpan),
    extra_labels: impl IntoIterator<Item = (String, SimpleSpan)>,
    src: &str,
) -> ! {
    let fname = "example";
    Report::build(ReportKind::Error, (fname, label.1.into_range()))
        .with_config(ariadne::Config::new().with_index_type(ariadne::IndexType::Byte))
        .with_message(&msg)
        .with_label(
            Label::new((fname, label.1.into_range()))
                .with_message(label.0)
                .with_color(Color::Red),
        )
        .with_labels(extra_labels.into_iter().map(|label2| {
            Label::new((fname, label2.1.into_range()))
                .with_message(label2.0)
                .with_color(Color::Yellow)
        }))
        .finish()
        .print(sources([(fname, src)]))
        .unwrap();
    std::process::exit(1)
}


fn parse_failure(err: &Rich<impl fmt::Display>, src: &str) -> ! {
    failure(
        err.reason().to_string(),
        (
            err.found()
                .map(|c| c.to_string())
                .unwrap_or_else(|| "end of input".to_string()),
            *err.span(),
        ),
        err.contexts()
            .map(|(l, s)| (format!("while parsing this {l}"), *s)),
        src,
    )
}

fn try_parse_type(program: &str) {
    let tokens = lexer::lexer().parse(program).into_result().unwrap();
    println!("Processing {program}");
    println!("{:?}", tokens);
    let ty = make_type_parser(make_input)
        .parse(make_input((0..program.len()).into(), &tokens))
        .into_result()
        .unwrap_or_else(|errs| parse_failure(&errs[0], program));
    println!("{:?}", ty);
    println!("{}", ty.content);
}

fn try_parse_pattern(program: &str) {
    let tokens = lexer::lexer().parse(program).into_result().unwrap();
    println!("Processing {program}");
    println!("{:?}", tokens);
    let pt = make_pattern_parser(make_input)
        .parse(make_input((0..program.len()).into(), &tokens))
        .into_result()
        .unwrap_or_else(|errs| parse_failure(&errs[0], program));
    println!("{:?}", pt);
    println!("{}", pt.content);
}

fn try_parse_term(program: &str) {
    let tokens = lexer::lexer().parse(program).into_result().unwrap();
    println!("Processing {program}");
    println!("{:?}", tokens);
    let term = make_term_parser(make_input)
        .parse(make_input((0..program.len()).into(), &tokens))
        .into_result()
        .unwrap_or_else(|errs| parse_failure(&errs[0], program));
    println!("{:?}", term);
    println!("{}", term.content.to_pretty(10))
}

fn try_parse_command(program: &str, width: usize) {
    let tokens = lexer::lexer().parse(program).into_result().unwrap();
    println!("Processing {program}");
    let command = make_command_parser(make_input)
        .parse(make_input((0..program.len()).into(), &tokens))
        .into_result()
        .unwrap_or_else(|errs| parse_failure(&errs[0], program));
    println!("{:?}", command);
    println!("{}", command.content.to_pretty(width))
}

fn try_parse(program: &str) {
    let tokens = lexer::lexer().parse(program).into_result().unwrap();
    let program = make_program_parser(make_input)
        .parse(make_input((0..program.len()).into(), &tokens))
        .into_result()
        .unwrap_or_else(|errs| parse_failure(&errs[0], program));
    for command in program.iter() {
        println!("{:?}", command);
    }
    println!("{}", ProgramPrinter::pretty_print(&program, 30));

    let program = surface_to_internal(&program);
    println!("\ninternal:");
    println!("{}", ProgramPrinter::pretty_print(&program, 30));

    let program = internal_to_surface(&program);
    println!("\nsurface:");
    println!("{}", ProgramPrinter::pretty_print(&program, 30));

}

fn main() {
    let file = format!("{SRC}/benchmarks/2.ml");
    let src = &fs::read_to_string(&file).expect("Failed to read file");

    try_parse(src);
    /*let tasks = ml.split("\n\n");
    for task in tasks {
        try_parse_command(task, 100);
    }*/
}