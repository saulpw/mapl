
#include <assert.h>
#include <stdlib.h>

#define LEN(X) (sizeof(X)/sizeof((X)[0]))
#define MAX(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x > _y ? _x : _y; })
#define MIN(X,Y) ({ __typeof__ (X) _x = (X); __typeof__ (Y) _y = (Y); _x < _y ? _x : _y; })
#define DO(N,STMT) {int i=0,_n=(N);for(;i<_n;++i){STMT;}}

#define INCB(PTR, N)  ((void *) (((char *)(PTR)) + (N)))

#define ijk i,j,k

#define FORE(X, STMT) ({ \
     INT *px = (X)->vals; \
     for(int k=0,_nk=(X)->dims[3]; k<_nk; ++k) { \
     for(int j=0,_nj=(X)->dims[2]; j<_nj; ++j) { \
     for(int i=0,_ni=(X)->dims[1]; i<_ni; ++i) { \
       STMT; \
       px = INCB(px, (X)->strides[0]); \
     } px = INCB(px, (X)->strides[1]); \
     } px = INCB(px, (X)->strides[2]); \
     } \
})

#define FORE2(X, Y, STMT) ({ \
     INT *px = (X)->vals; \
     INT *py = (Y)->vals; \
     for(int k=0,_nk=MIN((X)->dims[3], (Y)->dims[3]); k<_nk; ++k) { \
     for(int j=0,_nj=MIN((X)->dims[2], (Y)->dims[2]); j<_nj; ++j) { \
     for(int i=0,_ni=MIN((X)->dims[1], (Y)->dims[1]); i<_ni; ++i) { \
       STMT; \
       px = INCB(px, (X)->strides[0]); \
       py = INCB(py, (Y)->strides[0]); \
     } px = INCB(px, (X)->strides[1]); \
       py = INCB(py, (Y)->strides[1]); \
     } px = INCB(px, (X)->strides[2]); \
       py = INCB(py, (Y)->strides[2]); \
     } \
})

#define TYPE(T) ((T) << 4)
#define WIDTH(R) ((R) << 0)
#define RANK(R) ((R) << 0)
    enum {
    T_SCALAR=RANK(0), T_VECTOR=RANK(1), T_MATRIX=RANK(2), T_TENSOR=RANK(3),
    T_8=WIDTH(0), T_16=WIDTH(1), T_32=WIDTH(2), T_64=WIDTH(3),
    T_PTR=TYPE(0), T_FLOAT=TYPE(1), T_INT=TYPE(2), T_UINT=TYPE(3)
    };

typedef struct array {
    union {
        INT *vals;
    };

#define T_INT (T_INT|T_64)
#define T_FLOAT (T_INT|T_INT|T_64)

    struct {
        unsigned type : 4;  // 0:ptr    1:float  2:int    3:uint  4-7: reserved
        unsigned width: 2;  // 0:8bit   1:16bit  2:32bit  3:64bit
        unsigned rank : 2;  // 0:scalar 1:vector 2:matrix 3:tensor
    };

    INT dims[4];    // shape of tensor: [0] #bytes in value, [1] is #cols, [2] is #rows, [3] is #sheets
    INT strides[4]; // shape of memory: each stride is bytes between element ptrs at that level
} array;

#define $$A A->rank
#define $$B B->rank

INT *pcell(array *A, INT i, INT j, INT k);

// loop over each cell in X or X|Y
#define DO1(X, STMT) FORE(XDO((X)->dims[1], DOj((X)->dims[2], INT *out=pcell(X,i,j); STMT))
#define DO2(X, Y, STMT) int X_n=totdim(X), Y_n=totdim(Y); DO(MAX(X_n, Y_n), INT *p=&(X)->vals[i]; STMT)

void    copy(INT *d, INT *s, int n) { DO(n, d[i]=s[i]); }
void revcopy(INT *d, INT *s, int n) { DO(n, d[n-i-1]=s[i]); }
void    zero(INT *d, int n) { DO(n, d[i]=0); }

// --- arrays ---

int nbytes(int rank, INT dims[rank]) { INT z=1; DO(rank, z*=dims[i]); return z; }
int ncells(array *A) { return nbytes(LEN(A->dims)-1, A->dims+1); }
int totdim(array *A) { return nbytes(LEN(A->dims), A->dims); }
int totcap(array *A) { return nbytes(LEN(A->strides), A->strides); }

INT *pcell(array *A, INT x, INT y, INT z)
{
    INT i=y*A->strides[1] + x*A->strides[0];
//    PR("[%lld,%lld:%lld]=%lld  ", x, y, i, A->vals[i]);
    assert(x < A->dims[1]);
    assert(y < A->dims[2]);
    return &A->vals[i];
}

#define CELL(A, X, Y) *pcell(A, X, Y)


array *check(array *A) {
    assert(A->strides[0] == sizeof(INT));
    assert(A->dims[0] == sizeof(INT));
    DO(A->rank, assert(A->dims[i] > 0));
    DO(A->rank, assert(A->strides[i] > 0));
    return A;
}

array *arrptr(int x, int y, int z, void *ptr) {
    assert(x > 0); assert(y > 0); assert(z > 0);
    array *A = malloc(sizeof(array));
    bzero(A, sizeof(array));
    A->strides[0] = sizeof(INT);
    A->strides[1] = x;
    A->strides[2] = y;
    A->strides[3] = z;
    A->dims[0] = sizeof(INT);
    A->dims[1] = x;
    A->dims[2] = y;
    A->dims[3] = z;
    A->vals = ptr ? ptr : malloc(totcap(A));
    return check(A);
}

array *arr(int x, int y, int z) { return arrptr(x,y,z,0); }

#define vec(N) arr(N, 1, 1)
#define mat(N, M) arr(N, M)

#define $Aw  A->dims[0]
#define $Ax  A->dims[1]
#define $Ay  A->dims[2]
#define $Az  A->dims[3]

#define $Aw_ A->strides[0]
#define $Ax_ A->strides[1]/$Aw_
#define $Ay_ A->strides[2]/$Aw_
#define $Az_ A->strides[3]/$Aw_

#define $Bx B->dims[1]
#define $By B->dims[2]
#define $Bz B->dims[3]

array *redim(array *A, int x, int y, int z) {
    if (x <= $Ax_ && y <= $Ay_ && z <= $Az_) { $Ax=x; $Ay=y; $Az=z; return A; }
    INT n = $Aw_*x*y*z;
    array *r=arr(x,y,z);
    FORE2(A, r, *px = *pcell(r, ijk));
    return check(r);
}

void append(array *A, array *B) {
    array *r=redim(A, $Ax+$Bx, MAX($Ay, $By), MAX($Az, $Bz));
    FORE(A, *pcell(r, ijk) = *px);
}

INT unboxint(array *A) { assert($$A == 1 && $Ax == 1 && $Ay == 1 && $Az == 1); return A->vals[0]; }
array *boxint(INT v) { array *A=vec(1); *A->vals=v; return A; }
#define boxptr(P) boxint ((INT) (P))

int print(array *A) {
    if (!A) { PR("(null) "); return 0; }
    FORE(A, PR("%lld ", *pcell(A,ijk)));
}

