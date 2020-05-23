#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define PR(X...) do { printf(X); fflush(stdout); } while (0)

FILE *sysopen(const char *fn) { return (*fn=='-') ? stdin : fopen(fn, "r"); }
int sysread(FILE *fp, char *buf, int sz) { return fgets(buf, sz, fp) ? 0 : EOF; }
int sysclose(FILE *fp) { fclose(fp); return 0; }

// INT needed for array and lang
typedef long long int INT;

#include "array.c"

// DAT and unbox needed for lang
typedef struct array *DAT;
#define box(T, A) box##T(A)
#define unbox(T, A) unbox##T(A)
#define unboxINT unboxint
#define boxINT boxint

#include "lang.c"

int main(int argc, const char *argv[]) {
    DO(argc-1, FILE *fp=sysopen(argv[i+1]); interpret_file(fp); sysclose(fp));
}

WERB("[", pusharray, push(vec(0)))
// ---

#define cpop(X) check(pop(X))
#define UNOP(T, N, STMT)  _WERB(0, T, N, NULL) { array *A=cpop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) _WERB(0, T, N, NULL) { array *B=cpop(); array *A=cpop();   STMT; return 0; }

BINOP("+",   add,    FORE2(A,B, *px += *py); push(A))
BINOP("*",   mult,   FORE2(A,B, *px *= *py); push(A))
BINOP("/",   div,    FORE2(A,B, *px /= *py); push(A))
BINOP("==",  equals, FORE2(A,B, *px = (*px == *py)); push(A))
BINOP("%",   modulo, FORE2(A,B, *px %= *py); push(A))
//BINOP("%",   modulo, array *r=mat($B, $A); FORE(A, DO($A, DOj($B, CELL(r, i, j)=CELL(A, i, 0)%CELL(B, 0, j); f_CR(0))); push(r))
BINOP("select", select, array *r=vec(ncells(A)); FORE2(A,B, if (*px) append(r, boxint(*py))); push(r))

BINOP(",",   concat,
        INT An=totdim(A);
        INT Bn=totdim(B);
        array *ret=arr(An+Bn, 1, 1);
        int n=0;
        FORE(A, ret->vals[n++]=*px);
        FORE(B, ret->vals[n++]=*px);
        copy(ret->vals+An, B->vals, Bn);
        push(ret))

UNOP("??",   check,  FORE(A, assert(*px)))
UNOP("$",    shape, push(arrptr(3, 1, 1, A->dims+1)))
UNOP("*/",   mult_reduce, INT acc=1; FORE(A, acc *= *px); push(boxint(acc)))
UNOP("+/",   sum_reduce, INT acc=0; FORE(A, acc += *px); push(boxint(acc)))
UNOP("&&/",  and_reduce, INT acc=0; FORE(A, acc = acc && *px); push(boxint(acc)))
UNOP("NOT",  logical_not, FORE(A, *px = !*px); push(A))
UNOP("iota", iota, INT x=0; B=arr($Ax, $Ay, $Az); FORE(B, *px=x++); push(B))

#define A_0 *pcell(A, 0, 0, 0)
#define A_1 *pcell(A, 1, 0, 0)
#define A_2 *pcell(A, 2, 0, 0)
UNOP("reshape", reshape,
    B=peek(0);
    array *r=arr(A_0, ($Ax>=1) ? A_1 : 1, ($Ax>=2) ? A_2 : 1);
    FORE2(B, r, *px=*py);
    push(r))

UNOP(".",    print,  print(A))
