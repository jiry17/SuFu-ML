type 'a list =
| Cons of 'a * 'a list
| Nil

val tails: int list -> (int list) list
let rec tails = function
| Nil -> Nil
| Cons (_, t) as xs ->
  Cons (xs, tails t)

let rec sum = function
| Nil -> 0
| Cons (h, t) -> h + sum t

let max =
  if a <= b then b else a

let rec maximum = function
| Cons (h, Nil) -> h
| Cons (h, t) ->
  max h maximum t

let rec map = function
| Nil -> Nil
| Cons (h, t) -> Cons (f h, t)

let mts =
  maximum (map sum (tails xs))
