#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef long long int i64;
#define INT i64
#define alloc malloc

#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define DO(n,x) {int i=0,_n=(n);for(;i<_n;++i){x;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))
#define PR(X...) do { printf(X); fflush(stdout); } while (0)
#define PINT(X) ({ i64 _x = (X); PR("["#X"=%lld] ", _x); _x; })

// loop over each cell in A
#define DO1(A, EXPR) DO(tr(A->rank, A->dims), i64 *p=&A->vals[i]; EXPR) // *A->strides[0]+sum(A->rank-1, A->strides+1)]; EXPR)
// loop over each cell in A|B
#define DO2(A, B, EXPR) int A_n=tr(A->rank, A->dims), B_n=tr(B->rank, B->dims); DO(MAX(A_n, B_n), i64 *p=&A->vals[i]; EXPR)

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

// --- stack ---
array *STACK[16];
array **SP = STACK;
#define DEPTH (SP-STACK)
array *push(array *a) { *SP++ = a; return a; }
array *pop(void) { assert(DEPTH > 0); return *--SP; }
#define peek(X) *(SP-(X)-1)

char PAD[80];
void cr(void) { PR("\n"); }

// --- dict ---
typedef int (verbfunc)(void);
typedef struct verb {
    const char *name;
    verbfunc *func;
} verb;
extern verb __start_verbs[], __stop_verbs[];

#define _VERB(TOK, VERBNAME) extern int v_##VERBNAME(void); \
  verb verb_##VERBNAME __attribute__((__section__("verbs"))) __attribute__((__used__)) = { TOK, v_##VERBNAME }; \
  int v_##VERBNAME(void)

verb *find(const char *tok) {
    DO(__stop_verbs-__start_verbs, if(!strcasecmp(__start_verbs[i].name, PAD)) return &__start_verbs[i]);
    return NULL;
}

// --- arrays ---
void copy(INT *d, INT *s, int n) { DO(n, d[i]=s[i]); }
void revcopy(INT *d, INT *s, int n) { DO(n, d[n-i-1]=s[i]); }
int tr(int rank, INT dims[rank]) { i64 z=1; DO(rank, z*=dims[i]); return z; }
array *redim(array *a, int rank, INT strides[rank]) {
    if (!a) { a=alloc(sizeof(array)); bzero(a, sizeof(*a)); a->rank=1; }
    INT total = tr(rank, strides);
    if (total > tr(a->rank, a->strides)) { a->vals=realloc(a->vals, sizeof(*a->vals)*total); assert(a->vals); }
    a->rank=rank;
    copy(a->strides, strides, rank);
    return a;
}
array *reshape(array *a, int rank, INT dims[rank]) {
    INT revdims[rank];
    revcopy(revdims, dims, rank);
    a=redim(a, rank, revdims);
    copy(a->dims, revdims, rank);
    return a;
}
void append(array *A, i64 v) { INT s[A->rank]; copy(s, A->strides, A->rank); s[0]++; redim(A, A->rank, s); A->vals[$A++] = v; }
void print_rank(array *A, int n) {
    if (!A) { PR("(null) "); return; }
    if (n == 1) { DO1(A, PR("%lld ", *p)); }
    else {
        array inner = *A;
        inner.rank = n-1;
        DO(A->dims[n-1], inner.vals=&A->vals[i*A->strides[n-2]]; print_rank(&inner, n-1));
    }
    cr();
}
void print(array *A) { print_rank(A, A->rank); }

// --- REPL ---
int parse(char *out, const char *input) { // return number of chars parsed into out
    const char *s = input;
    while (*s && isspace(*s)) ++s; // skip spaces
    while (*s && !isspace(*s)) *out++ = *s++; // copy non-spaces
    *out = 0;
    return s - input;
}

void interpret(char *input) {
    while (*input) {
        int i = parse(PAD, input);
        if (!*PAD) break;
        char *endptr = NULL;
        i64 v = strtoll(PAD, &endptr, 0);
        if (endptr != PAD) { // some valid number
            append(peek(0), v);
        } else {
            verb *v = find(PAD);
            if (!v) { PR("unknown: %s\n", PAD); }
            else { v->func(); }
        }
        input += i;
    }
}

int main()
{
    char s[128];
    while(fgets(s, sizeof(s), stdin)) {
        PR("   DEPTH=%ld >>>  %s", DEPTH, s);
        interpret(s);
        if (DEPTH) print(peek(0));
    }
}

// --- mapl ---
#define VERB(T, N, STMT) _VERB(T, N) { array *A=NULL, *B=NULL; STMT; return 0; }
#define UNOP(T, N, STMT)  _VERB(T, N) { array *A=pop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) _VERB(T, N) { array *B=pop(); array *A=peek(0); DO2(A, B, STMT); return 0; } // A B -> A?B

VERB("dup", dup, push(peek(0)))
VERB("drop", drop, pop())
BINOP("+", add, A_i += B_i)
BINOP("*", mult, A_i *= B_i)
BINOP("==", equals, A_i = (A_i == B_i))
VERB(".S", printstack, DO(DEPTH, print(peek(i))))
VERB("[", pusharray, push(redim(NULL, 0, 0)))
UNOP("iota", iota, B=push(reshape(NULL, $A, A->vals)); DO1(B, *p=i))
UNOP(".", print, print(A))
UNOP("??", check, DO1(A, assert(A_i)))
VERB("reshape", reshape, B=pop(); A=peek(0); reshape(A, $B, B->vals))
