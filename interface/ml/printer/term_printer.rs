use std::arch;
use std::fmt::{Display, Formatter};
use std::str::FromStr;
use std::usize::MAX;
use pretty::RcDoc as rc;
use crate::language::{Bind, BindTerm, Command, ConfigVal, Pattern, Program, Term, Type};

// TODO: read this paper: https://homepages.inf.ed.ac.uk/wadler/papers/prettier/prettier.pdf

impl Term {
    fn to_doc(&self) -> rc<()> {
        self.complex_to_doc().1
    }

    fn atom_to_doc(&self) -> rc<()> {
        match self {
            Term::Unit => rc::text("()"),
            Term::Int(i) => rc::text(i.to_string()),
            Term::Bool(b) => if *b {rc::text("true")} else {rc::text("false")},
            Term::Var(name) => rc::text(name),
            Term::NativeCons(name) => rc::text(name),
            Term::Tuple(contents) => {
                let content = rc::intersperse(
                    contents.iter().map(|c| c.content.to_doc()),
                    rc::text(",").append(rc::line()),
                ).nest(1);
                rc::text("(").append(content).append(rc::text(")"))
            },
            _ => {
                let res = self.to_doc();
                rc::text("(").append(res).append(rc::text(")"))
            }
        }
    }

    fn call_to_doc(&self) -> rc<()> {
        match self {
            Term::App(func, param) => {
                // println!("app {:?} {:?}", func.content, param.content);
                let func_oup = func.content.call_to_doc();
                let param_oup = param.content.atom_to_doc();
                func_oup
                    .append(rc::line().append(param_oup).nest(2))
                    .group()
            },
            Term::Cons {cons, body} => {
                let body_oup = body.content.atom_to_doc();
                rc::text(cons).append(rc::text(" ")).append(body_oup).group()
            }
            _ => self.atom_to_doc(),
        }
    }

    fn arith_to_doc(&self) -> (usize, rc<()>) {
        fn get_op_pri(op: &str, arity: usize) -> usize {
            match op {
                "-" => if arity == 1 {20} else {5},
                "not" => 3, "*" => 10,
                "/" => 10, "+" => 5,
                "==" | "<" | ">" | "<=" | ">=" => 4,
                "&&" => 2, "||" => 1,
                _ => panic!("unknown operator {}", op)
            }
        }

        match self {
            Term::PrimOp {op, params} if params.len() == 1 => {
                let current_pri = get_op_pri(op, 1);
                let (param_pri, mut param_res) = params[0].content.arith_to_doc();
                if param_pri < current_pri {
                    param_res = rc::text("(").append(param_res).append(rc::text(")"));
                }
                let res = rc::group(
                    rc::text(op).append(rc::line()).append(param_res)
                );
                (current_pri, res)
            },
            Term::PrimOp {op, params} if params.len() == 2 => {
                let current_pri = get_op_pri(op, 2);
                let (left_pri, mut left_res) = params[0].content.arith_to_doc();
                let (right_pri, mut right_res) = params[1].content.arith_to_doc();
                if left_pri < current_pri {
                    left_res = rc::text("(").append(left_res).append(rc::text(")"));
                }
                if right_pri <= current_pri {
                    right_res = rc::text("(").append(right_res).append(rc::text(")"));
                }
                let case1 = rc::group(
                    rc::group(left_res.clone().append(rc::line()).append(rc::text(op)))
                        .append(rc::line().append(right_res.clone()).nest(2))
                );
                let case2 = rc::group(
                    left_res.append(rc::line())
                        .append(rc::group(rc::text(op).append(rc::line()).append(right_res).nest(2)))
                );
                (current_pri, rc::union(case1, case2))
            },
            Term::PrimOp {op, params} => {
                panic!("Unexpected arity: {} with arity {}", op, params.len())
            },
            _ => (usize::MAX, self.call_to_doc())
        }
    }

    fn complex_to_doc(&self) -> (bool, rc<()>) {
        fn _merge_let<'a>(head: rc<'a, ()>, body: rc<'a, ()>) -> rc<'a, ()> {
            let case1 = head.clone().append(rc::line()).append(
                rc::group(rc::text("in").append(rc::line()).append(body.clone()).nest(2))
            );
            let case2 = rc::group(head.append(rc::line()).append(rc::text("in")))
                .append(rc::line().append(body).nest(2));
            rc::group(rc::union(case1, case2))
        }

        match self {
            Term::If(c, t, f) => {
                let (_, c_res) = c.content.complex_to_doc();
                let (_, t_res) = t.content.complex_to_doc();
                let (flag, f_res) = f.content.complex_to_doc();
                let full = rc::group(rc::nest(
                    rc::text("if ").append(c_res).append(rc::line())
                        .append(rc::text("then ")).append(t_res)
                        .append(rc::line()).append(rc::text("else "))
                        .append(f_res),
                    2
                ));

                (flag, full)
            },
            Term::Match {def, cases} => {
                let (_, def_res) = def.content.complex_to_doc();
                let mut group_list = vec![
                    rc::group(
                        rc::text("match ").append(def_res).append(" with")
                    )
                ];
                let len = cases.len();
                for (i, (pattern, case)) in cases.iter().enumerate() {
                    let new_group = _build_match_case(&pattern.content, &case.content, i == len - 1);
                    group_list.push(new_group);
                }
                (true, rc::group(rc::intersperse(group_list.into_iter(), rc::hardline())))
            },
            Term::Let {bind, body}  => {
                let head = bind.content.to_doc();
                let (flag, body_res) = body.content.complex_to_doc();
                (flag, _merge_let(head, body_res))
            },
            Term::Func {params, body} => {
                let head = format!("fun {} ->", params.join(" "));
                let (flag, body_res) = body.content.complex_to_doc();
                (flag, rc::group(rc::text(head).append(rc::line())).append(body_res).nest(2))
            }
            _ => (false, self.arith_to_doc().1)
        }
    }

    pub fn to_pretty(&self, width: usize) -> String {
        let mut w = Vec::new();
        self.to_doc().render(width, &mut w).unwrap();
        String::from_utf8(w).unwrap()
    }
}

fn _build_match_case<'a>(pt: &'a Pattern, case: &'a Term, is_last: bool) -> rc<'a, ()> {
    let (flag, mut case_res) = case.complex_to_doc();
    if flag && !is_last {
        case_res = rc::text("(").append(case_res).append(rc::text(")"));
    }
    rc::group(rc::nest(
        rc::text(format!("| {} ->", pt))
            .append(rc::line()).append(case_res),
        2
    ))
}

impl Bind {
    fn to_doc(&self) -> rc<()> {
        let params = self.params.join(" ");
        match &self.bind {
            BindTerm::NormalBind(term) => {
                let head = format!("{} {} {}{}=", if self.is_rec {"let rec"} else {"let"}, self.name, params, if params.is_empty() {""} else {" "});
                let res = term.content.to_doc();
                rc::text(head).append(rc::line()).append(res).nest(2).group()
            },
            BindTerm::FuncBind(cases) => {
                let head = format!("{} {} {}{}= function", if self.is_rec {"let rec"} else {"let"}, self.name, params, if params.is_empty() {""} else {" "});
                let mut groups = vec![rc::text(head)];
                let size = cases.len();
                for (i, (pt, t)) in cases.iter().enumerate() {
                    groups.push(_build_match_case(&pt.content, &t.content, i + 1 == size));
                }
                rc::group(rc::intersperse(groups, rc::hardline()))
            }
        }
    }

    pub fn to_pretty(&self, width: usize) -> String {
        let mut w = Vec::new();
        self.to_doc().render(width, &mut w).unwrap();
        String::from_utf8(w).unwrap()
    }
}

impl Display for ConfigVal {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigVal::Int(i) => write!(f, "{}", i),
            ConfigVal::Bool(b) => write!(f, "{}", if *b {"true"} else {"false"}),
            ConfigVal::String(s) => write!(f, "\"{}\"", s),
        }
    }
}

impl Command {
    fn to_doc(&self) -> rc<()> {
        match self {
            Command::Config {name, val} => {
                let str = format!("@{} = {}", name, val);
                rc::text(str)
            }
            Command::TermDef(bind) => {
                bind.content.to_doc()
            },
            Command::TermEval(term) => {
                term.content.to_doc()
            },
            Command::TypeDeclare {name, ty} => {
                if let Type::Poly {vars, body} = &ty.content {
                    rc::text(format!("val {}: {}", name, body.content.to_string()))
                } else {
                    rc::text(format!("val {}: {}", name, ty.content.to_string()))
                }
            },
            Command::TypeAlias {name, def} => {
                if let Type::Poly {vars, body} = &def.content {
                    assert!(!vars.is_empty());
                    let s = format!("type {} {} = {}",
                        vars.join(" "), name, body.content.to_string()
                    );
                    rc::text(s)
                } else {
                    let s = format!("type {} = {}", name, def.content.to_string());
                    rc::text(s)
                }
            },
            Command::TypeDef {name, cons_list, arity} => {
                let first_type = &cons_list.first().unwrap().1;
                let mut group = vec![];
                if let Type::Poly {vars, body} = &first_type.content {
                    group.push(format!("type {} {} =", vars.join(" "), name));
                } else {
                    group.push(format!("type {} =", name));
                }
                let mut line = group[0].clone();
                for (i, (cons_name, cons_ty)) in cons_list.iter().enumerate() {
                    let content_type =
                        if let Type::Poly {body, ..} = &cons_ty.content
                        {&body.content} else {&cons_ty.content};
                    let base =
                        if let Type::Arr(inp, _) = content_type
                        {format!("{} of {}", cons_name, inp.content.to_string())} else {cons_name.clone()};
                    group.push(base.clone());
                    if i > 0 {line.push_str(" | ")} else {line.push_str(" ")};
                    line.push_str(base.as_str())
                }
                let single_line = rc::text(line);
                let multi_line = group.into_iter()
                    .map(|s| rc::text(s));
                let multi_line = rc::intersperse(multi_line, rc::hardline().append(rc::text("| ")));
                rc::union(single_line, multi_line)
            }
        }
    }

    pub fn to_pretty(&self, width: usize) -> String {
        let mut w = Vec::new();
        self.to_doc().render(width, &mut w).unwrap();
        String::from_utf8(w).unwrap()
    }
}

pub struct ProgramPrinter;

impl ProgramPrinter {
    pub fn pretty_print(program: &Program, width: usize) -> String {
        let mut is_pre_bind = false;
        let mut lines = vec![];
        for command in program {
            if is_pre_bind {lines.push(String::default());}
            lines.push(command.command.content.to_pretty(width));
            is_pre_bind = if let Command::TermDef(_) = &command.command.content {true} else {false};
        }
        lines.join("\n")
    }
}