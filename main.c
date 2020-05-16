#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "mpl.h"

// --- dict ---
char PAD[128];

werb *LATEST = NULL;

werb *find(const char *tok) {
    werb *w = LATEST;
    while (w) { if(!strcasecmp(w->name, tok)) return w; else w=w->prev; }
    DO(__stop_werbs-__start_werbs, if(!strcasecmp(__start_werbs[i].name, tok)) return &__start_werbs[i]);
    return NULL;
}

DAT STACK[16];
RET RSTACK[16];
DAT *SP = STACK;
RET *RP = RSTACK;

XT *IP = NULL;

char dict[8192];
char *DP = dict;
int STATE=0;

// --- arrays ---
void copy(INT *d, INT *s, int n) { DO(n, d[i]=s[i]); }
void revcopy(INT *d, INT *s, int n) { DO(n, d[n-i-1]=s[i]); }
int tr(int rank, INT dims[rank]) { i64 z=1; DO(rank, z*=dims[i]); return z; }
array *redim(array *A, int rank, INT strides[rank]) {
    if (!A) { A=alloc(sizeof(array)); bzero(A, sizeof(*A)); A->rank=1; }
    INT total = tr(rank, strides);
    if (total > tr(A->rank, A->strides)) { A->vals=realloc(A->vals, sizeof(*A->vals)*total); assert(A->vals); }
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

// --- REPL ---

void *allot(int n) { void *ptr=DP; DP += n; return ptr; }
int compile(void *ptr) { void **r = allot(sizeof(ptr)); *r = ptr; return 0; }

// --- mapl ---
#define WERB(T, N, STMT) _WERB(0, T, N, NULL) { STMT; return 0; }
#define IWERB(T, N, STMT) _WERB(1, T, N, NULL) { STMT; return 0; }
#define UNOP(T, N, STMT)  _WERB(0, T, N, NULL) { array *A=pop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) _WERB(0, T, N, NULL) { array *B=pop(); array *A=peek(0); DO2(A, B, STMT); return 0; } // A B -> A?B

WERB("BYE", BYE, exit(0))

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
            if (NUMTIB <= 0) fBYE(0);
            TIB = _TIB;
        } else if (out-_out) {
            *out = 0;
            PR("%s ", PAD);
            return;
        }
    }
}

WERB("PARSE", PARSE, parse(unboxint(pop()), PAD); push(boxptr(PAD)))
WERB("DOLIT", DOLIT, push(boxint((INT) *IP++)))
WERB("DUP", DUP, push(peek(0)))
WERB("DROP", DROP, pop())
WERB("ENTER", ENTER, rpush(IP); IP=w->args)
WERB("EXIT", EXIT,  IP=rpop())
WERB("(BRANCH)", BRANCH, INT i=(INT) *IP++; IP += i)
WERB(".S", printstack, DO(DEPTH, print(peek(i))))
WERB("[", pusharray, INT x=0; push(redim(NULL, 1, &x)))

WERB("CREATE", CREATE,
        parse(0, PAD);
        w=allot(sizeof(werb));
        strncpy(w->name, PAD, sizeof(w->name));
        w->prev=LATEST;
        w->func=fENTER;
        w->args=(void *) DP;
        LATEST=w
        )

// parse single token from input buffer, and either parse+append number, or find+execute word
// return -1 if more input needed.
_WERB(0, "INTERPRET", INTERPRET, NULL) {
    parse(0, PAD);

    char *endptr = NULL;
    i64 val = strtoll(PAD, &endptr, 0);
    if (endptr != PAD) { // some valid number
        if (STATE) {
            compile(&wDOLIT);
            compile((void *) val);
        } else {
            append(peek(0), val);
        }
    } else {
        werb *w = find(PAD);
        if (!w) { PR("unknown: %s\n", PAD); }
        else { if (STATE && !w->flags) compile(w); else w->func(w); }
    }
    return 0;
}

WERB(":", COLON, fCREATE(0); STATE=1)
IWERB(";", SEMICOLON, compile(&wEXIT); STATE=0)
WERB("IMMEDIATE", IMMEDIATE, LATEST->flags=1)

DEF("QUIT", &wINTERPRET, &wBRANCH, (XT) -3);

int main()
{
    IP = find("QUIT")->args;

    while (1) {
        werb *w = *IP++;
        assert(!w->func(w));
    }
}

BINOP("+", add, A_i += B_i)
BINOP("*", mult, A_i *= B_i)
BINOP("==", equals, A_i = (A_i == B_i))
UNOP("iota", iota, B=push(reshape(NULL, $A, A->vals)); DO1(B, *p=i))
UNOP(".", print, print(A))
UNOP("??", check, DO1(A, assert(A_i)))
UNOP("reshape", reshape, B=peek(0); reshape(B, $A, A->vals))
UNOP("*/", mult_reduce, int acc=1; DO($A, acc *= A_i); push(boxint(acc)))
