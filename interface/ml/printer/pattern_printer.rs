use std::fmt::{Display, Formatter};
use crate::language::Pattern;

fn pattern_to_string(pattern: &Pattern) -> String {

    fn basic_to_string(pattern: &Pattern) -> String {
        match pattern {
            Pattern::Wildcard => "_".to_string(),
            Pattern::Var(None, name) => name.clone(),
            Pattern::Tuple(contents) => {
                let contents_str = contents.iter()
                    .map(|p| pattern_to_string(&p.content))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("({})", contents_str)
            }
            _ => format!("({})", pattern_to_string(pattern))
        }
    }

    fn cons_to_string(pattern: &Pattern) -> String {
        match pattern {
            Pattern::Cons(name, param) => {
                if let Some(content) = param {
                    format!("{} {}", name, basic_to_string(&content.content))
                } else {
                    format!("{}", name)
                }
            },
            _ => format!("{}", basic_to_string(pattern))
        }
    }

    match pattern {
        Pattern::Var(Some(content), name) => {
            format!("{} as {}", cons_to_string(&content.content), name)
        },
        _ => cons_to_string(&pattern)
    }
}

impl Display for Pattern {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", pattern_to_string(self))
    }
}