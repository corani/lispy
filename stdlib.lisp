; Atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

; Function Definition
(def {fun} 
     (\ {args body} 
        {def (head args)
            (\ (tail args) body)}))

; Unpack list
(fun {unpack f xs} 
     {eval (join (list f) xs)})

; Pack list
(fun {pack f & xs} 
    {f xs})

; Currying
(def {curry} unpack)
(def {uncurry} pack)

; First, Second, Third Item in a List
(fun {fst l}
     {eval (head l)})
(fun {snd l}
     {eval (head (tail l))})
(fun {trd l}
     {eval (head (tail (tail l)))})

; Length of list
(fun {len l} 
     {if (== l nil)
        {0}
        {+ 1 (len (tail l))}})

; Nth item in a list
(fun {nth n l}
     {if (== n 0)
        {fst l}
        {nth (- n 1) (tail l)}})

; Last item in a list (rather inefficiently)
(fun {last l}
     {nth (- (len l) 1) l})

; Take N items
(fun {take n l}
     {if (== n 0)
        {nil}
        {join (head l) (take (- n 1) (tail l))}})

; Drop N items
(fun {drop n l}
     {if (== n 0)
        {l}
        {drop (- n 1) (tail l)}})

; Split at N (rather inefficiently)
(fun {split n l}
     {list (take n l) (drop n l)})

; Element of list
(fun {elem x l}
     {if (== l nil)
        {false}
        {if (== x (fst l))
            {true}
            {elem x (tail l)}}})

; Map
(fun {map f l}
     {if (== l nil)
        {nil}
        {join 
            (list (f (fst l))) 
            (map f (tail l))}})

; Filter
(fun {filter f l}
     {if (== l nil)
        {nil}
        {join 
            (if (f (fst l))
                {head l}
                {nil})
            (filter f (tail l))}})

; Sequence
(fun {do & l}
     {if (== l nil)
        {nil}
        {last l}})

; Let scope
(fun {let b}
     {((\ {_} b) ())})

; Logical operators
(fun {not x} 
     {- 1 x})
(fun {or x y}
     {+ x y})
(fun {and x y}
     {* x y})

; Misc
(fun {flip f a b}   ; Is this essentially Forth's `over`?
     {f b a})
(fun {ghost & xs}
     {eval xs})
(fun {comp f g x}   ; Compose two functions
     {f (g x)})

; Fold
(fun {foldl f z l}
     {if (== l nil)
        {z}
        {foldl f (f z (fst l)) (tail l)}})

(fun {sum l}
     {foldl + 0 l})
(fun {product l}
     {foldl * 1 l})

; Select
(fun {select & cs}
     {if (== cs nil)
        {error "No selection found"}
        {if (fst (fst cs))
            {snd (fst cs)}
            {unpack select (tail cs)}}})
; Default
(def {otherwise} true)

(fun {case x & cs}
     {if (== cs nil)
        {error "No case found"}
        {if (== x (fst (fst cs)))
            {snd (fst cs)}
            {unpack case (join (list x) (tail cs))}}})
