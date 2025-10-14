type 'a list =
| Cons of 'a * 'a list
| Nil

@Exclude let rec lib_fold op e = function
| Cons (h, t) -> op h (lib_fold op e t)
| Nil -> e

@Combine let lib_inf = 100
@Exclude let lib_err = 100

@Combine let is_err x = (x == lib_err) || (x == 0-lib_err)

@Align
let lib_plus a b = a + b

@Align
let lib_minus a b = a - b

@Align
let lib_times a b = a * b

@Align
let lib_min a b = if a < b then a else b

@Align
let lib_max a b = if a > b then a else b

@Align
let lib_maximum xs = lib_fold lib_max (0 - lib_inf) xs

@Align
let lib_minimum xs = lib_fold lib_min lib_inf xs

@Align
let lib_sum xs = lib_fold lib_plus 0 xs

@Align
let lib_length xs = lib_fold (fun a b -> b + 1) 0 xs

@Align @Extract
let lib_head = function
| Cons (h, _) -> h
| Nil -> lib_err

@Align let lib_inc x = x + 1

@Align let lib_dec x = x - 1

@Align let lib_neg x = 0 - x

@Align
let rec lib_last = function
| Nil -> lib_err
| Cons (h, t) ->
  match t with
  | Nil -> h
  | _ -> lib_last t

@Align
let lib_access pos xs =
  let len = lib_length xs in
  let ind = if pos < 0 then pos + len else pos in
  if ind < 0 || ind >= len then
    lib_err
  else let rec visit v = function
  | Cons (h, t) ->
    if v == 0 then h else visit (v - 1) t
  in visit ind xs

@Align
let rec lib_count p = function
| Nil -> 0
| Cons (h, t) -> if p h then (lib_count p t) + 1 else lib_count p t

@Align
let lib_take pos xs =
  let len = lib_length xs in
  let ind = if pos < 0 then pos + len else pos in
  if ind < 0 || ind >= len then
    Nil
  else let rec visit v = function
  | Nil -> Nil
  | Cons (h, t) ->
    if v < 0 then Nil else Cons (h, visit (v - 1) t)
  in visit ind xs

@Align
let lib_drop pos xs =
  let len = lib_length xs in
  let ind = if pos < 0 then pos + len else pos in
  if ind < 0 || ind >= len then
    Nil
  else let rec visit v = function
  | Nil -> Nil
  | Cons (h, t) ->
    if v == 0 then Cons(h, t) else visit (v - 1) t
  in visit ind xs

@Align
let lib_rev xs =
  let rec res_aux res = function
  | Nil -> res
  | Cons (h, t) -> res_aux (Cons (h, res)) t
  in res_aux Nil xs

@Align
let rec lib_map f = function
| Nil -> Nil
| Cons (h, t) -> Cons (f h, lib_map f t)

@Align
let rec lib_filter p = function
| Nil -> Nil
| Cons (h, t) -> if p h then Cons (h, lib_filter p t) else lib_filter p t

@Align
let rec lib_zip op xs = function
| Nil -> Nil
| Cons (y, yt) ->
  match xs with
  | Nil -> Nil
  | Cons (x, xt) -> Cons (op x y, lib_zip op xt yt)

@Align
let lib_scanl op = function
| Nil -> Nil
| Cons (h, t) ->
  let rec aux op pre = function
  | Nil -> Cons (pre, Nil)
  | Cons (ih, it) ->
    Cons (pre, aux op (op pre ih) it)
  in aux op h t

@Align
let rec lib_scanr op = function
| Nil -> Nil
| Cons (h, t) -> match t with
  | Nil -> Cons (h, t)
  | Cons (ih, it) ->
    let res = lib_scanr op t in
    match res with
    | Cons (rh, _) -> Cons (op h rh, res)

@Align
let lib_isneg a = a < 0

@Align
let lib_ispos a = a > 0

@Align
let lib_iseven a = a == (a / 2 * 2)

@Align
let lib_isodd a = not (lib_iseven a)

@Align let lib_one = 1

@Align let lib_none = -1