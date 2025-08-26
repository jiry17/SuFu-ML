use crate::language::Program;
use crate::processor::arity_processor::{AddZeroArity, RemoverZeroArity};
use crate::processor::native_processor::{AddNative, RemoveNative};
use crate::processor::processor::Processor;

mod processor;
mod arity_processor;
mod native_processor;

pub fn surface_to_internal(program: &Program) -> Program {
    let mut processor = RemoverZeroArity::default();
    let program = processor.process_program(program);
    let mut processor = RemoveNative::default();
    processor.process_program(&program)
}

pub fn internal_to_surface(program: &Program) -> Program {
    let mut processor = AddNative::default();
    let program = processor.process_program(program);
    let mut processor = AddZeroArity::default();
    processor.process_program(&program)
}