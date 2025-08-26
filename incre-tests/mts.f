type 'a list =
| Cons of 'a * 'a list
| Nil

let head = function
| Cons (h, _) -> h

val tails: int list -> ((int list) list) compress
let rec tails = function
| Nil -> Cons (Nil, Nil)
| Cons (h, t) as xs ->
  Cons (xs, tails t)

let rec sum = function
| Nil -> 0
| Cons (h, t) -> h + sum t

let max a b =
  if a <= b then b else a

let rec maximum = function
| Cons (h, Nil) -> h
| Cons (h, t) ->
  max h (maximum t)

let rec map f = function
| Nil -> Nil
| Cons (h, t) -> Cons (f h, map f t)

let mts xs =
  maximum (map sum (tails xs))
