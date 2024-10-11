(fun {reverse l} {
     if (== l {}) 
        {{}} 
        {join (reverse (tail l)) (head l)}
})

; Test 
(print 
  (reverse (list 1 2 3))) ; (3 2 1)
