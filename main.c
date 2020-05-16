#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef long long int i64;
#define INT i64
#define alloc malloc

#define CAT(a,b) a##b
#define XCAT(a,b) CAT(a,b)
#define MAX(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x > _y ? _x : _y; })
#define MIN(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x < _y ? _x : _y; })
#define DO(N,STMT) {int i=0,_n=(N);for(;i<_n;++i){STMT;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))
#define PR(X...) do { printf(X); fflush(stdout); } while (0)
#define PINT(X) ({ i64 _x = (X); PR("["#X"=%lld] ", _x); _x; })

// loop over each cell in X or X|Y
#define DO1(X, STMT) DO(tr((X)->rank, (X)->dims), i64 *p=&(X)->vals[i]; STMT)
#define DO2(X, Y, STMT) int X_n=tr((X)->rank, (X)->dims), Y_n=tr((Y)->rank, (Y)->dims); DO(MAX(X_n, Y_n), i64 *p=&(X)->vals[i]; STMT)

typedef struct array {
    INT *vals;
    int rank;       // len(dims)
    INT dims[3];    // shape of matrix; [0] is innermost length
    INT strides[3]; // shape of vals in memory; [0] is innermost stride (maximum length of inner axis)
} array;

#define $A A->dims[0]
#define $B B->dims[0]
#define $$A A->rank
#define $$B B->rank

#define A_i A->vals[i%$A]
#define B_i B->vals[i%$B]

// --- dict ---
char PAD[128];

struct werb;
typedef int (werbfunc)(struct werb *);
typedef struct werb {
    char flags;
    char name[39];
    struct werb *prev;
    werbfunc *func;
    struct werb **args;
} werb;
extern werb __start_werbs[], __stop_werbs[];

werb *LATEST = NULL;

#define _WERB(FLAGS, NAME, CTOK, ARGS) \
  extern int f##CTOK(werb *w); \
  werb w##CTOK __attribute__((__section__("werbs"))) __attribute__((__used__)) = { .flags=FLAGS, .name=NAME, .func=f##CTOK, .args=ARGS }; \
  int f##CTOK(werb *w)

#define DEF(NAME, ARGS...) \
  XT XCAT(wargs, __LINE__)[] = { ARGS }; \
  werb XCAT(w, __LINE__) __attribute__((__section__("werbs"))) __attribute__((__used__)) = { .name=NAME, .func=fENTER, .args=XCAT(wargs, __LINE__) };

werb *find(const char *tok) {
    werb *w = LATEST;
    while (w) { if(!strcasecmp(w->name, tok)) return w; else w=w->prev; }
    DO(__stop_werbs-__start_werbs, if(!strcasecmp(__start_werbs[i].name, tok)) return &__start_werbs[i]);
    return NULL;
}

// --- stacks and forth internals ---
typedef array *S;

S STACK[16];
S *SP = STACK;
#define DEPTH (SP-STACK)
S push(S a) { *SP++ = a; return a; }
S pop(void) { assert(DEPTH > 0); return *--SP; }
S peek(int n) { assert(DEPTH >= n); return *(SP-n-1); }


// --- return stack ---
typedef werb *XT;
typedef XT *R;
R RSTACK[16];
R *RP = RSTACK;
#define RDEPTH (RP-RSTACK)
R rpush(R r) { *RP++ = r; return r; }
R rpop(void) { assert(RDEPTH > 0); return *--RP; }

XT *IP = NULL;

char _TIB[128];
char *TIB = _TIB;
int NUMTIB = 0;

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

_WERB(0, "PARSE", PARSE, NULL) { parse(unboxint(pop()), PAD); push(boxptr(PAD)); }
WERB("DOLIT", DOLIT, push(boxint((INT) *IP++)))
WERB("DUP", DUP, push(peek(0)))
WERB("DROP", DROP, pop())
WERB("ENTER", ENTER, rpush(IP); IP=w->args)
WERB("EXIT", EXIT,  IP = rpop())
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
    return 1;
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
        w->func(w);
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
