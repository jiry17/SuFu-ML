(1, (()), 2, 3)

(f (1 2), f a b)

f (f (1 2), f a b)

(f (1, 2, 3, 4, 5), f (1, 2, 3, 4, 5, 6))

1 - -3 - f 3

1 - 3 - f 3

1 - - -3 - f 3

match x with
| y -> match y with
| _ -> 123

match x with
| Cons x as whole -> whole

match x with
| y -> (match y with
  | _ -> 123)
| _ -> 123

let f x = function
| t -> 1
| _ -> 2
in match x with
| t -> 1
| _ -> f 1

let f x y = fun z -> match z with
| _ -> 1
| a -> 2
in f 1 2 3