
// needs:
//   typedef DAT for the primary data type and INT for 
//     - should be an int the size of a ptr, or a ptr to a specific struct

extern int print(DAT A);

#include <assert.h>
#include <string.h>
#include <stdio.h>

FILE *g_fp;

#define DO(N,STMT) {int i=0,_n=(N);for(;i<_n;++i){STMT;}}

#define RETIF(X) ({int _x=(X); if (_x) return _x; _x; })
struct werb;
typedef struct werb *XT; // execution token type

typedef int (werbfunc)(struct werb *);
typedef struct werb {
    char flags;
    char name[39];
    struct werb *prev;
    werbfunc *func;
    struct werb **args;
} werb;
extern werb __start_werbs[], __stop_werbs[];

#define SECTION(NAME) __attribute__((__section__(#NAME))) __attribute__((__used__))

// for werbs which need a whole function body
#define _WERB(FLAGS, NAME, CTOK, ARGS) \
  extern int f_##CTOK(werb *w); \
  werb w##CTOK SECTION(werbs) = { .flags=FLAGS, .name=NAME, .func=f_##CTOK, .args=ARGS }; \
  int f_##CTOK(werb *w)


#define WERB(NAME, CTOK, STMT) _WERB(0, NAME, CTOK, NULL) { STMT; return 0; }  // one-liner werbs
#define IWERB(NAME, CTOK, STMT) _WERB(1, NAME, CTOK, NULL) { STMT; return 0; } // "immediate" werbs

char PAD[128];

typedef XT *RET; // return stack type

DAT STACK[16];
RET RSTACK[16];
DAT *SP;
RET *RP;
XT *IP;

#define LOAD(TYPE) ((TYPE) *IP++)

#define DEPTH (SP-STACK)
DAT push(DAT a) { *SP++ = a; return a; }
DAT pop(void) { assert(DEPTH > 0); return *--SP; }
DAT peek(int n) { assert(DEPTH >= n); return *(SP-n-1); }
// --- return stack ---
extern RET RSTACK[16];
extern RET *RP;
#define RDEPTH (RP-RSTACK)
RET rpush(RET r) { *RP++ = r; return r; }
RET rpop(void) { assert(RDEPTH > 0); return *--RP; }

char dict[8192];
char *DP = dict;
werb *LATEST = NULL;

int COMPILING=0;

WERB("CR",       CR,     PR("\n"))
WERB("BYE",      BYE,    return -2)
WERB("DUP",      DUP,    push(peek(0)))
WERB("SWAP",     SWAP,   DAT x=pop(); DAT y=pop(); push(x); push(y))
WERB("ROT",      ROT,    DAT x=pop(); DAT y=pop(); DAT z=pop(); push(y); push(x); push(z))
WERB("DROP",     DROP,   pop())
WERB("ENTER",    ENTER,  rpush(IP); IP=w->args)
WERB("EXIT",     EXIT,   IP=rpop())
WERB("(BRANCH)", BRANCH, int i=LOAD(INT); IP += i)

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

int parse(int ch, char *_out)
{
    static char _TIB[128];
    static char *TIB = _TIB;
    static int NUMTIB = 0;

    char *out = _out;

    while (1) {
        while (NUMTIB && matches(ch, *TIB)) { --NUMTIB, ++TIB; }; // skip spaces
        while (NUMTIB && !matches(ch, *TIB)) { --NUMTIB; *out++ = *TIB++; } // copy non-spaces
        if (!NUMTIB) {
            RETIF(sysread(g_fp, _TIB, sizeof(_TIB)));
            NUMTIB = strlen(_TIB);
            TIB = _TIB;
        } else if (out-_out) {
            NUMTIB--; TIB++; // skip final character
            *out = 0;
            PR("%s ", PAD);
            return 0;
        }
    }
}

WERB("CREATE", CREATE,
        parse(0, PAD);
        w=allot(sizeof(werb));
        strncpy(w->name, PAD, sizeof(w->name));
        w->func=f_ENTER; w->args=(void *) DP;
        w->prev=LATEST; LATEST=w)

WERB(":", COLON, f_CREATE(0); COMPILING=1)
IWERB(";", SEMICOLON, compile(&wEXIT); COMPILING=0)
WERB("IMMEDIATE", IMMEDIATE, LATEST->flags=1)

WERB("PARSE", PARSE, RETIF(parse(unbox(INT, pop()), PAD)); push(box(ptr, PAD)))
WERB("DOLIT", DOLIT, push(box(INT, LOAD(INT))))
WERB(".S", printstack, DO(DEPTH, print(peek(i))))

// parse single token from input buffer, and either parse+append number, or find+execute word
// return -1 if more input needed.
_WERB(0, "INTERPRET", INTERPRET, NULL) {
    RETIF(parse(0, PAD));

    char *endptr = NULL;
    INT val = strtoll(PAD, &endptr, 0);
    if (endptr != PAD) { // some valid number
        if (COMPILING) {
            compile(&wDOLIT);
            compile((void *) val);
        } else {
            append(peek(0), boxint(val));
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

WERB("EXECUTE", execute, return w->func(w))

XT QUIT_args[] = { &wINTERPRET, &wBRANCH, (XT) -3 };
werb w_QUIT SECTION(werbs) = { .name="QUIT", .func=f_ENTER, .args=QUIT_args };

int interpret_file(FILE *fp) {
    f_ABORT(0);
    for (g_fp=fp; !feof(fp); ) RETIF(f_execute(LOAD(XT)));
    g_fp = NULL;
    return EOF;
}
