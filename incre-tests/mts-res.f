let fst = function
| (y, _) -> y

let snd = function
| (_, y) -> y

type list = Nil | Cons of int * list
type clist =
| CNil
| Single of int
| Concat of clist * clist
let max a b = if a < b then b else a

val repr: clist -> int * int
let rec repr = function
| CNil -> (0, 0)
| Single a -> (max a 0, a)
| Concat (a, b) ->
  let m2 = repr a
  in
    let m3 = repr b
    in
      (max (fst m3) (snd m3 + fst m2),
       snd m3 + snd m2)

let program x = let m4 = repr x in fst m4
