use std::rc::Rc;
fn get_1_from_3<T0, T1, T2>(var0: Rc<(T0, T1, T2)>) -> T0
where
    T0: Clone,
    T1: Clone,
    T2: Clone,
{
    match var0.as_ref() {
        (y, _, _) => y.clone(),
    }
}

fn get_2_from_3<T0, T1, T2>(var0: Rc<(T0, T1, T2)>) -> T1
where
    T0: Clone,
    T1: Clone,
    T2: Clone,
{
    match var0.as_ref() {
        (_, y, _) => y.clone(),
    }
}

fn get_3_from_3<T0, T1, T2>(var0: Rc<(T0, T1, T2)>) -> T2
where
    T0: Clone,
    T1: Clone,
    T2: Clone,
{
    match var0.as_ref() {
        (_, _, y) => y.clone(),
    }
}

#[derive(Clone)]
enum list<T0> {
    Nil(Rc<()>),
    Cons(Rc<(T0, Rc<list<T0>>)>),
}
fn sum(var0: Rc<list<Rc<i32>>>) -> Rc<i32> {
    match var0.as_ref() {
        list::Nil(_) => Rc::new(0),
        list::Cons(m0) => match m0.as_ref() {
            (hd, tl) => Rc::new(*hd.clone() + *((sum)(tl.clone()))),
        },
    }
}

fn max(a: Rc<i32>, b: Rc<i32>) -> Rc<i32> {
    (if (*a.clone() > *b.clone()) {
        a.clone()
    } else {
        b.clone()
    })
}

fn mts(s: Rc<i32>, var0: Rc<list<Rc<i32>>>) -> Rc<i32> {
    match var0.as_ref() {
        list::Nil(_) => s.clone(),
        list::Cons(m0) => match m0.as_ref() {
            (hd, tl) => {
                ((mts)(
                    ((max)(Rc::new(*s.clone() + *hd.clone()), Rc::new(0))),
                    tl.clone(),
                ))
            }
        },
    }
}

fn mps(var0: Rc<list<Rc<i32>>>) -> Rc<i32> {
    match var0.as_ref() {
        list::Nil(_) => Rc::new(0),
        list::Cons(m0) => match m0.as_ref() {
            (hd, tl) => ((max)(Rc::new(*((mps)(tl.clone())) + *hd.clone()), Rc::new(0))),
        },
    }
}

fn spec(l: Rc<list<Rc<i32>>>) -> Rc<(Rc<i32>, Rc<i32>)> {
    Rc::new((((mts)(Rc::new(0), l.clone())), ((mps)(l.clone()))))
}

fn repr(var0: Rc<list<Rc<i32>>>) -> Rc<(Rc<i32>, Rc<i32>, Rc<i32>)> {
    match var0.as_ref() {
        list::Nil(_) => Rc::new((Rc::new(0), Rc::new(0), Rc::new(0))),
        list::Cons(m0) => match m0.as_ref() {
            (h, t) => {
                let m1 = ((repr)(t.clone()));
                Rc::new((
                    (if (*((get_1_from_3)(m1.clone()))
                        < (*h.clone() + *((get_3_from_3)(m1.clone()))))
                    {
                        Rc::new(*h.clone() + *((get_3_from_3)(m1.clone())))
                    } else {
                        ((get_1_from_3)(m1.clone()))
                    }),
                    (if ((0) < (*h.clone() + *((get_2_from_3)(m1.clone())))) {
                        Rc::new(*h.clone() + *((get_2_from_3)(m1.clone())))
                    } else {
                        Rc::new(0)
                    }),
                    Rc::new(*h.clone() + *((get_3_from_3)(m1.clone()))),
                ))
            }
        },
    }
}

fn main(xs: Rc<list<Rc<i32>>>) -> Rc<(Rc<i32>, Rc<i32>)> {
    {
        let m2 = ((repr)(xs.clone()));
        Rc::new((((get_1_from_3)(m2.clone())), ((get_2_from_3)(m2.clone()))))
    }
}
