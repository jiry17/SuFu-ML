use std::borrow::Borrow;
use std::{env, fs};
use std::fs::File;
use std::io::Read;
use chumsky::input::BorrowInput;
use chumsky::span::SimpleSpan;
use chumsky::prelude::{*};
use serde::Serialize;
use ml::{lexer, SRC};
use ml::language::Token;
use ml::parser::program_parser::make_program_parser;
use ml::printer::term_printer::ProgramPrinter;
use ml::processor::surface_to_internal;

fn make_input<'src>(
    eoi: SimpleSpan,
    toks: &'src [(Token, SimpleSpan)],
) -> impl BorrowInput<'src, Token = Token, Span = SimpleSpan> {
    toks.map(eoi, |token| (&token.0, &token.1))
}

fn main() {
    let mut args: Vec<String> = env::args().collect();

    /*if args.len() != 3 {
        args = vec![
            "".to_string(),
            format!("{SRC}/benchmarks/2.ml"),
            format!("{SRC}/benchmarks/oup.json"),
        ];
    }*/

    if args.len() != 3 {
        eprintln!("Usage: {} <input_file> <output_file>", args[0]);
    }

    let inp_file = args[1].clone();
    let oup_file = args[2].clone();

    let src = &fs::read_to_string(&inp_file).expect("Failed to read file");

    let tokens = lexer::lexer().parse(src).into_result().unwrap();
    let program = make_program_parser(make_input)
        .parse(make_input((0..src.len()).into(), &tokens))
        .into_result()
        .unwrap();

    let program = surface_to_internal(&program);
    //println!("{}", ProgramPrinter::pretty_print(&program, 20));
    //println!("{:?}", program.last().unwrap());

    let file = File::create(oup_file).unwrap();
    let mut serializer = serde_json::Serializer::pretty(file);
    program.serialize(&mut serializer);
}