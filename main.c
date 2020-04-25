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
#define push(X) *SP++ = (X);
#define pop() (*--SP)
#define peek(X) *(SP-X-1)

char PAD[80];

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

extern verb __start_verbs[];
extern verb __stop_verbs[];

verb *find(const char *tok) {
    DO(__stop_verbs-__start_verbs, if(!strcmp(__start_verbs[i].name, PAD)) return &__start_verbs[i]);
    return NULL;
}

//
// arrays
//
array *newarray(void)
{
    array *a = alloc(sizeof(array));
    a->vals = NULL;
    a->n = 0;
    a->dim = 0;
    return a;
}

void append(array *a, i64 v) {
    if (a->n+1 > a->dim) {
        a->dim = a->n + 1;
        a->vals = realloc(a->vals, sizeof(*a->vals)*a->dim);
    }
    a->vals[a->n++] = v;
}

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
    return NULL;
}

int main()
{
    char s[128];
    while(fgets(s, sizeof(s), stdin)) interpret(s);
}

VERB("+", add) { array *a=pop(); array *b=peek(0); DO(a->n, b->vals[i] += a->vals[i]); return 0; }
VERB(".", print) { array *a=pop(); DO(a->n, printf("%lld ", a->vals[i])); return 0; }
VERB("[", pusharray) { push(newarray()); return 0; }
