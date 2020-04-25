
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef long long int i64;
#define alloc malloc
#define DO(n,x) {int i=0,_n=(n);for(;i<_n;++i){x;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))

#define push(X) *SP++ = (X);
#define pop() (*--SP)

i64 STACK[16];
i64 *SP = STACK;
char PAD[80];

// return number of chars parsed into PAD
int parse(char *out, const char *input) {
    const char *s = input;
    while (*s && isspace(*s)) ++s; // skip spaces
    while (*s && !isspace(*s)) *out++ = *s++; // copy non-spaces
    *out = 0;
    return s - input;
}

typedef int (verbfunc)(void);
typedef struct verb {
    const char *name;
    verbfunc *func;
} verb;

int add(void) { i64 v = pop(); STACK[0] += v; return 0; }
int print(void) { printf("%lld ", pop()); return 0; }


verb verbs[] = {
    (struct verb){.name="+", .func=add },
    (struct verb){.name=".", .func=print },
};

verb *find(const char *tok) {
    DO(LEN(verbs), if(!strcmp(verbs[i].name, PAD)) return &verbs[i]);
    return NULL;
}

int interpret(char *input) {
    while (*input) {
        int i = parse(PAD, input);
        if (!*PAD) break;
        char *endptr = NULL;
        i64 v = strtoll(PAD, &endptr, 0);
        if (endptr != PAD) { // some valid number
            push(v);
        } else {
            verb *v = find(PAD);
            if (!v) { printf("unknown: %s\n", PAD); }
            else v->func();
        }
        input += i;
    }
    return 0;
}

int main()
{
    char s[128];
    while(fgets(s, sizeof(s), stdin)) interpret(s);
}
