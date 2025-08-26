use std::borrow::Borrow;
use std::{env, fs};
use std::fs::File;
use std::io::{BufReader, Write};
use chumsky::prelude::{*};
use ml::{lexer, SRC};
use ml::language::Program;
use ml::printer::term_printer::ProgramPrinter;
use ml::processor::internal_to_surface;

fn main() {
    let mut args: Vec<String> = env::args().collect();

    // let inp_file = args[1].clone();
    // let oup_file = args[2].clone();
    let inp_file = "/tmp/282475249.json";
    let oup_file = "/Users/pro/Desktop/work/2025S/SuFu-ML/incre-tests/res.f";

    let file = File::open(inp_file).unwrap();
    let reader = BufReader::new(file);
    let program: Program = serde_json::from_reader(reader).unwrap();
    let program = internal_to_surface(&program);

    let result = ProgramPrinter::pretty_print(&program, 50);

    let mut file = File::create(oup_file).unwrap();
    writeln!(file, "{}", result);
}