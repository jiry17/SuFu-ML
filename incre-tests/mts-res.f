type list = Nil | Cons of int * list
type clist =
| CNil
| Single of int
| Concat of clist * clist
let prog x = false
