type 'a list =
| Cons of 'a * 'a list
| Nil

let fst = function
| (x, _) -> x

let snd = function
| (_, x) -> x

let head = function
| Cons xs -> fst xs

let rec tails = function
| Nil -> Cons (Nil, Nil)
| Cons xs ->
  Cons (fst xs, tails (Cons xs))

let rec sum = function
| Nil -> 0
| Cons xs -> (fst xs) + sum (snd xs)

let max a b =
  if a <= b then b else a

let rec maximum = function
| _ -> 0

let rec map f = function
| Nil -> Nil
| Cons xs -> Cons (f (fst xs), map f (snd xs))

let mts xs =
  maximum (map sum (tails xs))