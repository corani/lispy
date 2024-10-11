; Fibonacci
; Be careful with the number you pick, it will take a while to compute!
(fun {fib n}
     {select
        {(== n 0) 0}
        {(== n 1) 1}
        {otherwise 
            (+ 
              (fib (- n 1)) 
              (fib (- n 2)))}})

(print "Fibonacci 20")
(print (fib 20))
