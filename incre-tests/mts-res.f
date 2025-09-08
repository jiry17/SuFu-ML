let fst = 
  fun x -> match x with
    | (y, _) -> y

let snd = 
  fun x -> match x with
    | (_, y) -> y

type 'a list = Cons of 'a * 'a list | Nil of unit
let head = 
  fun var0 -> match var0 with
    | Cons (h, _) -> h

val tails: int list -> int * int
let rec tails = 
  fun var0 -> match var0 with
    | Nil _ -> (0, 0)
    | Cons (h, t) as xs ->
      let m1 =  tails t
      in
        (if fst m1 < h + snd m1
           then h + snd m1
           else fst m1,
         h + snd m1)

let rec sum = 
  fun var0 -> match var0 with
    | Nil _ -> 0
    | Cons (h, t) -> h + sum t

let max = 
  fun a -> fun b -> if a <= b then b else a

let rec maximum = 
  fun var0 -> match var0 with
    | Cons (h, Nil _) -> h
    | Cons (h, t) -> max h (maximum t)

let rec map = 
  fun f -> fun var0 -> match var0 with
      | Nil _ -> Nil ()
      | Cons (h, t) -> Cons (f h, map f t)

let mts =  fun xs -> let m3 =  tails xs in fst m3
