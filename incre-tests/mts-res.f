let fst = function
| (y, _) -> y

let snd = function
| (_, y) -> y

type nat = Z | S of nat
type list = Nil | Cons of int * list
type indexed_list =
| CNil
| CCons of int * int * indexed_list
val w: int
let rec length = function
| Nil -> 0
| Cons (_, tl) -> 1 + length tl

val repr: list -> int * int
let rec repr = function
| Nil -> (0, w)
| Cons (h, t) ->
  let m1 = length t
  in
    let m2 = repr t
    in
      (if snd m2 == h then m1 else fst m2,
       snd m2)

let prog xs = let m3 = repr xs in fst m3
