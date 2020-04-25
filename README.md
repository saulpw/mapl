# a very simple stack-based array language, in C

## 1. stack, parsing, numberizing, add, print, basic REPL
test: `2 3 + .`
output: `5`

## 2. array append; convert add/print to array form
test: `[ 2 3 [ 2 1 + .`
output: `4 4`

## 3.
test: `[ 5 iota  [ 5 iota + .`
output: `0 2 4 6 8`

## 4.
test: `[ 5 iota  [ 3 iota + .`
output: `0 2 4 3 6`

## 5. add tests.mapl and tests-golden.txt
