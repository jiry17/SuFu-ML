use std::fmt::{Display, Formatter};
use crate::language::Type;


fn type_to_string(ty: &Type) -> String {
    fn atom_type_to_string(ty: &Type) -> String {
        match ty {
            Type::Bool => "bool".to_string(),
            Type::Unit => "unit".to_string(),
            Type::Int => "int".to_string(),
            Type::Var(name) => name.clone(),
            Type::Ind {name, params} if params.is_empty() => {
                name.clone()
            }
            _ => {
                format!("({})", type_to_string(ty))
            }
        }
    }

    fn cons_type_to_string(ty: &Type) -> String {
        match ty {
            Type::Ind {name, params} => {
                if params.is_empty() {
                    name.to_string()
                } else if params.len() == 1 {
                    format!("{} {}", atom_type_to_string(&params[0].content), name)
                } else {
                    let contents = params.iter()
                        .map(|param| type_to_string(&param.content))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("({}) {}", contents, name)
                }
            }
            _ => atom_type_to_string(ty),
        }
    }

    fn tuple_to_string(ty: &Type) -> String {
        match ty {
            Type::Tuple(contents) => {
                let contents = contents.iter()
                    .map(|param| cons_type_to_string(&param.content))
                    .collect::<Vec<_>>()
                    .join(" * ");
                format!("{}", contents)
            },
            _ => cons_type_to_string(ty),
        }
    }

    match &ty {
        Type::Arr(inp, oup) => {
            format!("{} -> {}", tuple_to_string(&inp.content), type_to_string(&oup.content))
        },
        Type::Poly {vars, body} => {
            format!("∀ {}. {}", vars.join(" "), type_to_string(&body.content))
        }
        _ => tuple_to_string(ty),
    }

}

impl Display for Type {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        let result = type_to_string(self);
        f.write_str(result.as_str())
    }
}