#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "mpl.h"

char PAD[128];

DAT STACK[16];
RET RSTACK[16];
DAT *SP;
RET *RP;

XT *IP;

char dict[8192];
char *DP = dict;
werb *LATEST = NULL;

int COMPILING=0;

WERB("BYE",      BYE,    exit(0))
WERB("DUP",      DUP,    push(peek(0)))
WERB("DROP",     DROP,   pop())
WERB("ENTER",    ENTER,  rpush(IP); IP=w->args)
WERB("EXIT",     EXIT,   IP=rpop())
WERB("(BRANCH)", BRANCH, INT i=(INT) *IP++; IP += i)

void    copy(INT *d, INT *s, int n) { DO(n, d[i]=s[i]); }
void revcopy(INT *d, INT *s, int n) { DO(n, d[n-i-1]=s[i]); }
void    zero(INT *d, int n) { DO(n, d[i]=0); }

// --- arrays ---

int ncells(int rank, INT dims[rank]) { i64 z=1; DO(rank, z*=dims[i]); return z; }
int totdim(array *A) { return ncells(A->rank, A->dims); }
int totcap(array *A) { return ncells(A->rank, A->strides); }

array *arr(int rank, INT caps[rank]) {
    array *A = alloc(sizeof(array));
    bzero(A, sizeof(array));
    A->rank = rank;
    copy(A->strides, caps, rank);
    A->vals = alloc(totcap(A)*sizeof(INT));
    return A;
}

// change strides in A and copy existing contents into new structure
array *restride(array *A, int rank, INT strides[rank]) {
    array *B = arr(rank, strides);
    copy(B->dims, A->dims, rank);
    copy(B->vals, A->vals, MIN($A, $B));
    return B;
}

array *resize(array *A, int rank, INT dims[rank]) {
    array *B = restride(A, rank, dims);
    copy(B->dims, dims, rank);
    return B;
}

array *redim(array *A, int rank, INT strides[rank]) {
    if (!A) { A=alloc(sizeof(array)); bzero(A, sizeof(*A)); A->rank=1; }
    INT total = ncells(rank, strides);
    if (total > totcap(A)) { A->vals=realloc(A->vals, sizeof(*A->vals)*total); }
    A->rank=rank;
    copy(A->strides, strides, rank);
    return A;
}
array *reshape(array *A, int rank, INT dims[rank]) {
    INT revdims[rank];
    revcopy(revdims, dims, rank);
    A=redim(A, rank, revdims);
    copy(A->dims, revdims, rank);
    return A;
}

void append(array *A, i64 v) { INT s[A->rank]; copy(s, A->strides, A->rank); s[0]++; redim(A, A->rank, s); A->vals[$A++] = v; }
INT unboxint(array *A) { assert($$A == 1 && $A == 1); return A->vals[0]; }
array *boxint(INT v) { INT x=1; array *A=redim(NULL, 1, &x); append(A, v); return A; }
#define boxptr(P) boxint ((INT) (P))

void print_rank(array *A, int n) {
    if (!A) { PR("(null) "); return; }
    if (n == 1) { DO1(A, PR("%lld ", *p)); }
    else {
        array inner = *A;
        inner.rank = n-1;
        DO(A->dims[n-1], inner.vals=&A->vals[i*A->strides[n-2]]; print_rank(&inner, n-1));
    }
    PR("\n");
}
void print(array *A) { print_rank(A, A->rank); }

// --- compile/find ---

void *allot(int n) { void *ptr=DP; DP += n; return ptr; }
int compile(void *ptr) { void **r = allot(sizeof(ptr)); *r = ptr; return 0; }

werb *find(const char *tok) {
    werb *w = LATEST;
    while (w) { if(!strcasecmp(w->name, tok)) return w; else w=w->prev; }
    DO(__stop_werbs-__start_werbs, if(!strcasecmp(__start_werbs[i].name, tok)) return &__start_werbs[i]);
    return NULL;
}

int matches(int fl, int ch) {
    if (fl) return (fl == ch);
    else return isspace(ch);
}

void parse(int ch, char *_out)
{
    static char _TIB[128];
    static char *TIB = _TIB;
    static int NUMTIB = 0;

    char *out = _out;

    while (1) {
        while (NUMTIB && matches(ch, *TIB)) { --NUMTIB, ++TIB; }; // skip spaces
        while (NUMTIB && !matches(ch, *TIB)) { --NUMTIB; *out++ = *TIB++; } // copy non-spaces
        if (!NUMTIB) {
            NUMTIB = fread(_TIB, 1, sizeof(_TIB), stdin);
            if (NUMTIB <= 0) f_BYE(0);
            TIB = _TIB;
        } else if (out-_out) {
            NUMTIB--; TIB++; // skip final character
            *out = 0;
            PR("%s ", PAD);
            return;
        }
    }
}

// ---

WERB("PARSE", PARSE, parse(unboxint(pop()), PAD); push(boxptr(PAD)))
WERB("DOLIT", DOLIT, push(boxint(LOAD(INT))))
WERB(".S", printstack, DO(DEPTH, print(peek(i))))
WERB("[", pusharray, INT x=0; push(redim(NULL, 1, &x)))

WERB("CREATE", CREATE,
        parse(0, PAD);
        w=allot(sizeof(werb));
        strncpy(w->name, PAD, sizeof(w->name));
        w->func=f_ENTER; w->args=(void *) DP;
        w->prev=LATEST; LATEST=w)

WERB(":", COLON, f_CREATE(0); COMPILING=1)
IWERB(";", SEMICOLON, compile(&wEXIT); COMPILING=0)
WERB("IMMEDIATE", IMMEDIATE, LATEST->flags=1)

// parse single token from input buffer, and either parse+append number, or find+execute word
// return -1 if more input needed.
_WERB(0, "INTERPRET", INTERPRET, NULL) {
    parse(0, PAD);

    char *endptr = NULL;
    i64 val = strtoll(PAD, &endptr, 0);
    if (endptr != PAD) { // some valid number
        if (COMPILING) {
            compile(&wDOLIT);
            compile((void *) val);
        } else {
            append(peek(0), val);
        }
    } else {
        werb *w = find(PAD);
        if (!w) { PR("unknown: %s\n", PAD); }
        else { if (COMPILING && !w->flags) compile(w); else w->func(w); }
    }
    return 0;
}

_WERB(0, "ABORT", ABORT, NULL) {
    SP = STACK;
    RP = RSTACK;
    IP = find("QUIT")->args;
    return 0;
}

XT QUIT_args[] = { &wINTERPRET, &wBRANCH, (XT) -3 };
werb w_QUIT SECTION(werbs) = { .name="QUIT", .func=f_ENTER, .args=QUIT_args };

// ---

#define UNOP(T, N, STMT)  _WERB(0, T, N, NULL) { array *A=pop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) _WERB(0, T, N, NULL) { array *B=pop(); array *A=peek(0); DO2(A, B, STMT); return 0; } // A B -> A?B

#define A_i A->vals[i%$A]
#define B_i B->vals[i%$B]

BINOP("+",   add,    A_i += B_i)
BINOP("*",   mult,   A_i *= B_i)
BINOP("==",  equals, A_i = (A_i == B_i))
WERB(",",   cat,
        array *B=pop();
        array *A=pop();
        INT An=totdim(A);
        INT Bn=totdim(B);
        INT n=An+Bn;
        array *ret=resize(A, 1, &n);
        copy(ret->vals+An, B->vals, Bn);
        push(ret))

UNOP(".",    print,  print(A))
UNOP("??",   check,  DO1(A, assert(A_i)))
UNOP("*/",   mult_reduce, int acc=1; DO($A, acc *= A_i); push(boxint(acc)))
UNOP("iota", iota, B=push(reshape(NULL, $A, A->vals)); DO1(B, *p=i))
UNOP("reshape", reshape, B=peek(0); reshape(B, $A, A->vals))

void main()
{
    f_ABORT(0);
    while (1) {
        werb *w = LOAD(werb *);
        assert(!w->func(w));
    }
}
