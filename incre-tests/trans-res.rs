use std::rc::Rc;
fn fst<T0, T1>(var0: Rc<(T0, T1)>) -> T0
where
    T0: Clone,
    T1: Clone,
{
    match var0.as_ref() {
        (y, _) => y.clone(),
    }
}

fn snd<T0, T1>(var0: Rc<(T0, T1)>) -> T1
where
    T0: Clone,
    T1: Clone,
{
    match var0.as_ref() {
        (_, y) => y.clone(),
    }
}

#[derive(Clone)]
enum list<T0> {
    Cons(Rc<(T0, Rc<list<T0>>)>),
    Nil(Rc<()>),
}
fn head<T0>(var0: Rc<list<T0>>) -> T0
where
    T0: Clone,
{
    match var0.as_ref() {
        list::Cons(m0) => match m0.as_ref() {
            (h, _) => h.clone(),
        },
        _ => panic!("unexpected case"),
    }
}

fn tails(var0: Rc<list<Rc<i32>>>) -> Rc<(Rc<i32>, Rc<i32>)> {
    match var0.as_ref() {
        list::Nil(_) => Rc::new((Rc::new(0), Rc::new(0))),
        list::Cons(m0) => match m0.as_ref() {
            (h, t) => {
                let xs = Rc::new(list::Cons(Rc::new((h.clone(), t.clone()))));
                let m1 = ((tails)(t.clone()));
                Rc::new((
                    (if (*((fst)(m1.clone())) < (*h.clone() + *((snd)(m1.clone())))) {
                        Rc::new(*h.clone() + *((snd)(m1.clone())))
                    } else {
                        ((fst)(m1.clone()))
                    }),
                    Rc::new(*h.clone() + *((snd)(m1.clone()))),
                ))
            }
        },
    }
}

fn sum(var0: Rc<list<Rc<i32>>>) -> Rc<i32> {
    match var0.as_ref() {
        list::Nil(_) => Rc::new(0),
        list::Cons(m0) => match m0.as_ref() {
            (h, t) => Rc::new(*h.clone() + *((sum)(t.clone()))),
        },
    }
}

fn max(a: Rc<i32>, b: Rc<i32>) -> Rc<i32> {
    (if (*a.clone() <= *b.clone()) {
        b.clone()
    } else {
        a.clone()
    })
}

fn maximum(var0: Rc<list<Rc<i32>>>) -> Rc<i32> {
    match var0.as_ref() {
        list::Cons(m0) => match m0.as_ref() {
            (h, t) => match t.as_ref() {
                list::Nil(_) => h.clone(),
                _ => ((max)(h.clone(), ((maximum)(t.clone())))),
            },
        },
        _ => panic!("unexpected case"),
    }
}

fn map<T0, T1, T2>(f: Rc<T0>, var0: Rc<list<T1>>) -> Rc<list<T2>>
where
    T0: Fn(T1) -> T2,
    T1: Clone,
    T2: Clone,
{
    match var0.as_ref() {
        list::Nil(_) => Rc::new(list::Nil(Rc::new(()))),
        list::Cons(m0) => match m0.as_ref() {
            (h, t) => Rc::new(list::Cons(Rc::new((
                ((*f.clone())(h.clone())),
                ((map)(f.clone(), t.clone())),
            )))),
        },
    }
}

fn mts(xs: Rc<list<Rc<i32>>>) -> Rc<i32> {
    {
        let m3 = ((tails)(xs.clone()));
        ((fst)(m3.clone()))
    }
}
