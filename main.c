#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef long long int i64;
#define INT i64
#define alloc malloc

#define MAX(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x > _y ? _x : _y; })
#define MIN(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x < _y ? _x : _y; })
#define DO(N,STMT) {int i=0,_n=(N);for(;i<_n;++i){STMT;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))
#define PR(X...) do { printf(X); fflush(stdout); } while (0)
#define PINT(X) ({ i64 _x = (X); PR("["#X"=%lld] ", _x); _x; })

// loop over each cell in X
#define DO1(X, STMT) DO(tr((X)->rank, (X)->dims), i64 *p=&(X)->vals[i]; STMT)
// loop over each cell in X|Y
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

struct verb;
typedef int (verbfunc)(struct verb *);
typedef struct verb {
    char flags;
    char name[39];
    struct verb *prev;
    verbfunc *func;
    struct verb **args;
} verb;
extern verb __start_verbs[], __stop_verbs[];

verb *LATEST = NULL;

#define _VERB(FLAGS, TOK, VERBNAME, ARGS) extern int v_##VERBNAME(verb *v); \
  verb verb_##VERBNAME __attribute__((__section__("verbs"))) __attribute__((__used__)) = { .flags=FLAGS, .name=TOK, .func=v_##VERBNAME, .args=ARGS }; \
  int v_##VERBNAME(verb *v)

verb *find(const char *tok) {
    verb *v = LATEST;
    while (v) { if(!strcasecmp(v->name, tok)) return v; else v=v->prev; }
    DO(__stop_verbs-__start_verbs, if(!strcasecmp(__start_verbs[i].name, tok)) return &__start_verbs[i]);
    return NULL;
}

// --- stacks and forth internals ---
array *STACK[16];
array **SP = STACK;
#define DEPTH (SP-STACK)
array *push(array *a) { *SP++ = a; return a; }
array *pop(void) { assert(DEPTH > 0); return *--SP; }
array *peek(int n) { assert(DEPTH >= n); return *(SP-n-1); }

void cr(void) { PR("\n"); }

// --- return stack ---
verb **RSTACK[16];
verb ***RP = RSTACK;
#define RDEPTH (RP-RSTACK)
verb **rpush(verb **ptr) { *RP++ = ptr; return ptr; }
verb **rpop(void) { assert(RDEPTH > 0); return *--RP; }

verb **IP = NULL;

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
INT popi(void) { assert(DEPTH > 0); return unboxint(pop()); }

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

void *allot(int n) { void *ptr=DP; DP += n; return ptr; }
int compile(void *ptr) { void **r = allot(sizeof(ptr)); *r = ptr; return 0; }

#define CAT(a,b) a##b
#define XCAT(a,b) CAT(a,b)
#define DEF(TOK, ARGS...) \
  verb *XCAT(verb_args, __LINE__)[] = { ARGS }; \
  verb XCAT(verb_, __LINE__) __attribute__((__section__("verbs"))) __attribute__((__used__)) = { .name=TOK, .func=v_enter, .args=XCAT(verb_args, __LINE__) };

// --- mapl ---
#define WORD(T, N, STMT) _VERB(0, T, N, NULL) { STMT; return 0; }
#define IMMEDWORD(T, N, STMT) _VERB(1, T, N, NULL) { STMT; return 0; }
#define VERB(T, N, STMT) _VERB(0, T, N, NULL) { array *A=NULL, *B=NULL; STMT; return 0; }
#define UNOP(T, N, STMT)  _VERB(0, T, N, NULL) { array *A=pop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) _VERB(0, T, N, NULL) { array *B=pop(); array *A=peek(0); DO2(A, B, STMT); return 0; } // A B -> A?B

WORD("DOLITERAL", DOLITERAL, push(boxint((INT) *IP++)))
WORD("dup", dup, push(peek(0)))
WORD("drop", drop, pop())
WORD("ENTER", enter, rpush(IP); IP = v->args)
WORD("EXIT", exit,  IP = rpop())
WORD("(BRANCH)", branch, INT i=(INT) *IP++; IP += i)

BINOP("+", add, A_i += B_i)
BINOP("*", mult, A_i *= B_i)
BINOP("==", equals, A_i = (A_i == B_i))
WORD(".S", printstack, DO(DEPTH, print(peek(i))))
WORD("[", pusharray, INT x=0; push(redim(NULL, 1, &x)))
UNOP("iota", iota, B=push(reshape(NULL, $A, A->vals)); DO1(B, *p=i))
UNOP(".", print, print(A))
UNOP("??", check, DO1(A, assert(A_i)))
VERB("reshape", reshape, B=pop(); A=peek(0); reshape(A, $B, B->vals))
UNOP("*/", mult_reduce, int acc=1; DO($A, acc *= A_i); push(boxint(acc)))

WORD("BYE", BYE, exit(0))

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
            if (NUMTIB <= 0) v_BYE(0);
            TIB = _TIB;
        } else if (out-_out) {
            *out = 0;
            PR("%s ", PAD);
            return;
        }
    }
}

_VERB(0, "PARSE", PARSE, NULL) { parse(popi(), PAD); push(boxptr(PAD)); }

WORD("CREATE", create,
        parse(0, PAD);
        v=allot(sizeof(verb));
        strncpy(v->name, PAD, sizeof(v->name));
        v->prev=LATEST;
        v->func=v_enter;
        v->args=(void *) DP;
        LATEST=v
        )


WORD(":", COLON, v_create(0); STATE=1)
IMMEDWORD(";", SEMICOLON, compile(&verb_exit); STATE=0)
WORD("IMMEDIATE", IMMEDIATE, LATEST->flags=1)

int main()
{
    IP = find("QUIT")->args;

    while (1) {
        verb *v = *IP++;
        v->func(v);
    }
}
// parse single token from input buffer, and either parse+append number, or find+execute word
// return -1 if more input needed.
_VERB(0, "interpret_word", interpret_word, NULL) {
    parse(0, PAD);

    char *endptr = NULL;
    i64 val = strtoll(PAD, &endptr, 0);
    if (endptr != PAD) { // some valid number
        if (STATE) {
            compile(&verb_DOLITERAL);
            compile((void *) val);
        } else {
            append(peek(0), val);
        }
    } else {
        verb *v = find(PAD);
        if (!v) { PR("unknown: %s\n", PAD); }
        else { if (STATE && !v->flags) compile(v); else v->func(v); }
    }
    return 1;
}

DEF("QUIT", &verb_interpret_word, &verb_branch, (verb *) -3);
