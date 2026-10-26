#define HW_DIAGNOSTIC_IMPLEMENTATION
#include "Types.h"
#include "Memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

real32 taskTimeElapsed=0;
void dbgMessagef(char *format,...) { (void)format; }
udword ranRandomFn(sdword index) { (void)index;return 123456789; }
static unsigned allocated, released;
static void *grow(sdword size) { ++allocated; return malloc(size); }
static void release(void *ptr) { ++released; free(ptr); }
void dbgFatalf(char *file,sdword line,char *format,...) {
    va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);exit(1);
}
extern mempool *memNewGrowthPoolAlloc(sdword length);
extern sdword memNumberGrowthPools;
int main(void) {
    void *mainHeap=malloc(MEM_HeapSizeDefault+4*sizeof(memcookie));
    void *block;unsigned i;
    if(!mainHeap || memStartup(mainHeap,MEM_HeapSizeDefault,grow)!=OKAY)return 1;
    /* Reproduce the exact fatal boundary from the Mission 2 -> 3 dump. */
    for(i=0;i<17;++i)if(!memNewGrowthPoolAlloc(1024))return 2;
    if(memNumberGrowthPools!=17 || allocated!=17)return 3;
    block=memAlloc(1024,"Growth test",NonVolatile|MBF_ExtendedPoolOnly);
    if(!block || allocated!=17)return 4;
    memset(block,0xa5,1024);memFree(block);
    block=memAlloc(1024,"Growth reuse",NonVolatile|MBF_ExtendedPoolOnly);
    if(!block || allocated!=17)return 5;
    memFree(block);memClose(release);free(mainHeap);
    return released==17 ? 0 : 6;
}
