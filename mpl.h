#ifndef MPL_H_
#define MPL_H_

#include <assert.h>
#include <stdio.h>

typedef long long int i64;
#define INT i64
#define alloc malloc

#define CAT(a,b) a##b
#define XCAT(a,b) CAT(a,b)
#define MAX(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x > _y ? _x : _y; })
#define MIN(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x < _y ? _x : _y; })
#define DO(N,STMT) {int i=0,_n=(N);for(;i<_n;++i){STMT;}}
#define LEN(X) (sizeof(X)/sizeof((X)[0]))

#define SECTION(NAME) __attribute__((__section__(#NAME))) __attribute__((__used__))
#define PR(X...) do { printf(X); fflush(stdout); } while (0)
#define PINT(X) ({ i64 _x = (X); PR("["#X"=%lld] ", _x); _x; })

// ---

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

#define _WERB(FLAGS, NAME, CTOK, ARGS) \
  extern int f_##CTOK(werb *w); \
  werb w##CTOK SECTION(werbs) = { .flags=FLAGS, .name=NAME, .func=f_##CTOK, .args=ARGS }; \
  int f_##CTOK(werb *w)

#define WERB(T, N, STMT) _WERB(0, T, N, NULL) { STMT; return 0; }
#define IWERB(T, N, STMT) _WERB(1, T, N, NULL) { STMT; return 0; }

// ---

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

// loop over each cell in X or X|Y
#define DO1(X, STMT) DO(totdim(X), i64 *p=&(X)->vals[i]; STMT)
#define DO2(X, Y, STMT) int X_n=totdim(X), Y_n=totdim(Y); DO(MAX(X_n, Y_n), i64 *p=&(X)->vals[i]; STMT)

typedef werb *XT; // execution token

extern XT *IP;    // instruction pointer
extern char *DP;  // dictionary pointer

#define LOAD(TYPE) ((TYPE) *IP++)

// --- stack machine ---
typedef array *DAT;

extern DAT STACK[16];
extern DAT *SP;
#define DEPTH (SP-STACK)
DAT push(DAT a) { *SP++ = a; return a; }
DAT pop(void) { assert(DEPTH > 0); return *--SP; }
DAT peek(int n) { assert(DEPTH >= n); return *(SP-n-1); }

// --- return stack ---
typedef XT *RET;
extern RET RSTACK[16];
extern RET *RP;
#define RDEPTH (RP-RSTACK)
RET rpush(RET r) { *RP++ = r; return r; }
RET rpop(void) { assert(RDEPTH > 0); return *--RP; }

#endif
