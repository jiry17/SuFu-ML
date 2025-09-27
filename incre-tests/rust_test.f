type 'a tree = Node of 'a tree * 'a tree | Leaf of 'a

let rec map f = function
| Leaf(a) -> f a
| Node(l, r) -> Node(map f l, map f r)