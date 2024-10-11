(def {fun} 
    (\ {args body} 
        {def (head args)
            (\ (tail args) body)}))

(fun {unpack f xs} 
    {eval (join (list f) xs)})

(fun {pack f & xs} 
    {f xs})

(fun {len l} 
    {if (== l {})
        {0}
        {+ 1 (len (tail l))}})
