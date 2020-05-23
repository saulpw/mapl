\ sum of numbers less than n, divisible by any of divisors[]

\ : modulo ( ints[n] divisors[m] -- mods[n,m] ) ;

: sum_divisibles     ( divisors[m] n -- sum )
..
    iota               ( divisors[m] numbers[n] )
    ..
    DUP ROT            ( numbers numbers divisors )
    ..
    %                  ( numbers mods[n,m] )
    ..
    NOT                ( numbers !mods[n,m] )
    ..
    &&/                ( numbers !mods[n] )
    ..
    SWAP select        ( numbers[] )
    ..
    +/                 ( sum )
  ;

[ 3 5 [ 20  sum_divisibles .
