let fst = function
| (y, _) -> y

let snd = function
| (_, y) -> y

type 'a list = Cons of 'a * 'a list | Nil
let head = function
| Cons (h, _) -> h

val tails: int list -> int * int
let rec tails = function
| Nil -> (0, 0)
| Cons (h, t) as xs ->
  let m1 = tails t
  in
    (if fst m1 < h + snd m1
       then h + snd m1
       else fst m1,
     h + snd m1)

let rec sum = function
| Nil -> 0
| Cons (h, t) -> h + sum t

let max a b = if a <= b then b else a

let rec maximum = function
| Cons (h, Nil) -> h
| Cons (h, t) -> max h (maximum t)

let rec map f = function
| Nil -> Nil
| Cons (h, t) -> Cons (f h, map f t)

let mts xs = let m3 = tails xs in fst m3

eval (mts (Cons (3, Cons (-4, Cons (1, Nil)))))