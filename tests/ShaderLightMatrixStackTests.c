/* Production lighting stack, with forced relocating allocations and guards.
   The old ten-entry implementation is also compiled offline against this
   fixture to reproduce the overwrite before accepting the fix. */
#include "Matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const hmatrix IdentityHMatrix = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static int checks, allocations, releases;
static unsigned char *block;
static size_t blockSize;
static hmatrix observed;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void guards(void)
{
    size_t i;
    if (!block) return;
    for (i=0;i<32;++i) CHECK(block[i]==0xA7 && block[32+blockSize+i]==0xA7);
}
static void *allocate(size_t size)
{
    unsigned char *next=(unsigned char *)malloc(size+64);
    CHECK(next!=NULL); memset(next,0xA7,size+64);
    block=next;blockSize=size;++allocations;
    return next+32;
}
static void *relocate(void *old,size_t size)
{
    unsigned char *previous=block;
    size_t previousSize=blockSize;
    void *next;
    CHECK(old==previous+32);guards();
    next=allocate(size);memcpy(next,old,previousSize);
    /* Allocate before freeing so every growth must invalidate old pointers. */
    CHECK(next!=old);free(previous);
    return next;
}
static void release(void *p)
{
    CHECK(block && p==block+32);guards();free(block);block=NULL;blockSize=0;++releases;
}
#define NonVolatile 0
#define memAlloc(size,name,flags) allocate(size)
#define memRealloc(ptr,size,name,flags) relocate(ptr,size)
#define memFree(ptr) release(ptr)
#define DBG_Loc 0
#define dbgFatalf(location,message) do { fprintf(stderr,"%s\n",message); exit(2); } while(0)
#define dbgAssertOrIgnore(x) ((void)0)
#define matrixdot(a,b,c,d,e,f) ((a)*(d)+(b)*(e)+(c)*(f))
static void shUpdateLightsFromMatrix(hmatrix *m) { observed=*m; }

#ifdef HW_REPRODUCE_FIXED_TEN_STACK
/* Match the captured executable's layout: normalList immediately follows
   entry 9; the piloting state starts 16 bytes beyond that allocation. */
static struct {
    hmatrix entries[10];
    unsigned char adjacent[512];
} legacy;
static long shLightMatStackPos;
#define shLightMatStack legacy.entries
#include "ShaderLightMatrixStack.before.inc"
int main(void)
{
    int i;
    memset(&legacy,0,sizeof(legacy));memset(legacy.adjacent,0xA7,sizeof(legacy.adjacent));
    shLightMatStack[0]=IdentityHMatrix;
    for(i=0;i<10;++i)shPushLightMatrix((hmatrix *)&IdentityHMatrix);
    CHECK(legacy.adjacent[0]==0xA7); /* Old production code fails here. */
    return 0;
}
#else
#include "ShaderLightMatrixStack.h"
#include "ShaderLightMatrixStack.inc"

/* Independent column-major 3x3 reference; integer quarter turns stay exact. */
static hmatrix product(const hmatrix *a,const hmatrix *b)
{
    hmatrix result=IdentityHMatrix;
    const float *x=(const float *)a,*y=(const float *)b;float *z=(float *)&result;
    int row,col,k;
    for(col=0;col<3;++col)for(row=0;row<3;++row) {
        z[col*4+row]=0;
        for(k=0;k<3;++k)z[col*4+row]+=x[k*4+row]*y[col*4+k];
    }
    return result;
}
static void equalRotation(const hmatrix *a,const hmatrix *b)
{
    const float *x=(const float *)a,*y=(const float *)b;int row,col;
    for(col=0;col<3;++col)for(row=0;row<3;++row)CHECK(x[col*4+row]==y[col*4+row]);
}
static void hierarchy(int depth)
{
    hmatrix turn[3] = {
        {0,1,0,0, -1,0,0,0, 0,0,1,0, 0,0,0,1},
        {1,0,0,0, 0,0,1,0, 0,-1,0,0, 0,0,0,1},
        {0,0,-1,0, 0,1,0,0, 1,0,0,0, 0,0,0,1}
    };
    hmatrix *reference=malloc((depth+1)*sizeof(hmatrix));int i;
    CHECK(reference!=NULL);reference[0]=IdentityHMatrix;
    for(i=1;i<=depth;++i) {
        reference[i]=product(&reference[i-1],&turn[i%3]);
        shPushLightMatrix(&turn[i%3]);guards();
        CHECK(shLightMatStackPos==i);equalRotation(&observed,&reference[i]);
    }
    for(i=depth-1;i>=0;--i) {
        shPopLightMatrix();guards();CHECK(shLightMatStackPos==i);
        equalRotation(&observed,&reference[i]);
    }
    free(reference);
}
int main(void)
{
    int i,warm;
    shLightMatrixStackReset();guards();
    hierarchy(9);hierarchy(10);hierarchy(11);hierarchy(65);hierarchy(4096);
    warm=allocations;
    /* Repeated world/preview rendering must reuse storage, restore its parent,
       and tolerate an unmatched pop without writing before the allocation. */
    for(i=0;i<100;++i) { hierarchy(32);shPopLightMatrix();CHECK(shLightMatStackPos==0); }
    CHECK(allocations==warm);
    shLightMatrixStackReset();CHECK(allocations==warm);hierarchy(12);
    shLightMatrixStackRelease();CHECK(!shLightMatStack && !shLightMatStackCapacity && !shLightMatStackPos && !block);
    shLightMatrixStackRelease();CHECK(releases==1);
    shLightMatrixStackReset();hierarchy(33);shLightMatrixStackRelease();CHECK(releases==2);
    printf("Lighting hierarchy stack passed: %d checks, depth 4096, moving allocations, no warm-draw allocation\n",checks);
    return 0;
}
#endif
