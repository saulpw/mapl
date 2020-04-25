#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef long long int i64;
#define alloc malloc
#define DO(n,x) {int i=0,_n=(n);for(;i<_n;++i){x;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))

typedef struct array {
    i64 *vals;
    int n, dim;
} array;

array *STACK[16];
array **SP = STACK;
#define DEPTH (SP-STACK)
array *push(array *a) { *SP++ = a; return a; }
array *pop(void) { assert(DEPTH > 0); return *--SP; }
#define peek(X) *(SP-X-1)

char PAD[80];
void cr(void) { printf("\n"); }


//
// dictionary
//
typedef int (verbfunc)(void);
typedef struct verb {
    const char *name;
    verbfunc *func;
} verb;

#define VERB(TOK, VERBNAME)                      \
    extern int v_##VERBNAME(void);            \
    verb verb_##VERBNAME                         \
        __attribute__((__section__("verbs")))    \
        __attribute__((__used__))                \
         = { TOK, v_##VERBNAME };       \
    int v_##VERBNAME(void)

#define A_i A->vals[i]
#define B_i B->vals[i]

extern verb __start_verbs[];
extern verb __stop_verbs[];

verb *find(const char *tok) {
    DO(__stop_verbs-__start_verbs, if(!strcasecmp(__start_verbs[i].name, PAD)) return &__start_verbs[i]);
    return NULL;
}

//
// arrays
//
array *redim(array *a, int dim) {
    if (!a) { a=alloc(sizeof(array)); a->n = a->dim = 0; }
    if (dim > a->dim) { a->dim=dim; a->vals=realloc(a->vals, sizeof(*a->vals)*a->dim); }
    return a;
}
void append(array *a, i64 v) { redim(a, a->n+1); a->vals[a->n++] = v; }
void print(array *a) { DO(a->n, printf("%lld ", a->vals[i])); cr(); }

//
// REPL
//

int parse(char *out, const char *input) {
    // return number of chars parsed into PAD
    const char *s = input;
    while (*s && isspace(*s)) ++s; // skip spaces
    while (*s && !isspace(*s)) *out++ = *s++; // copy non-spaces
    *out = 0;
    return s - input;
}

array *interpret(char *input) {
    while (*input) {
        int i = parse(PAD, input);
        if (!*PAD) break;
        char *endptr = NULL;
        i64 v = strtoll(PAD, &endptr, 0);
        if (endptr != PAD) { // some valid number
            append(peek(0), v);
        } else {
            verb *v = find(PAD);
            if (!v) { printf("unknown: %s\n", PAD); }
            else v->func();
        }
        input += i;
    }
    return peek(0);
}

int main()
{
    char s[128];
    while(fgets(s, sizeof(s), stdin)) print(interpret(s));
}

#define UNOP(T, N, STMT)  VERB(T, N) { array *A=pop(); array *B=NULL;    STMT; return 0; } // A -> 0
#define BINOP(T, N, STMT) VERB(T, N) { array *B=pop(); array *A=peek(0); STMT; return 0; } // A B -> A?B

VERB("dup", dup) { push(peek(0)); return 0; }
BINOP("+", add, DO(B->n, A_i += B_i))
VERB(".S", printstack) { DO(DEPTH, print(peek(i))); return 0; }
VERB("[", pusharray) { push(redim(NULL, 0)); return 0; }
UNOP("iota", iota, B=push(redim(NULL, A->vals[0])); DO(B->dim, B_i=i); B->n=B->dim)
UNOP(".", print, print(A))
