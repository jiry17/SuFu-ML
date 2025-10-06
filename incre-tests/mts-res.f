let fst = function
| (y, _) -> y

let snd = function
| (_, y) -> y

let max a b = if a > b then a else b

let min a b = if a < b then a else b

type list = Elt of int | Cons of int * list
type nested_list =
| Line of list
| NCons of list * nested_list
let fi = function
| (x, _) -> x

let se = function
| (_, y) -> y

let rec interval = function
| Elt x -> (x, x)
| Cons (hd, tl) ->
  let res = interval tl
  in
    let lo = fi res
    in let hi = se res in (min hd lo, max hd hi)

val repr: nested_list -> int * int
let rec repr = function
| Line x ->
  let info = interval x
  in
    let c0 = fst info
    in let c1 = snd info in (c0, c1)
| NCons (h, t) ->
  let info = interval h
  in
    let m1 = repr t
    in
      let c0 = fst info
      in
        let c1 = snd info
        in
          (fst m1 + c0 - max (fst m1) c0,
           max (snd m1) c1)

let prog xs =
  let m2 = repr xs in (fst m2, snd m2, false)
