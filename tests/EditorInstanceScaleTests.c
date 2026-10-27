#include "../src/Game/EditorInstanceScale.h"
#include <stdio.h>
#include <stdlib.h>
static EditorInstanceScaleStore scales;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    int objects[256],i;const void *owner=&objects[0];
    CHECK(editorInstanceScaleGet(&scales,owner,10)==1);
    CHECK(editorInstanceScaleSet(&scales,owner,10,2));
    CHECK(editorInstanceScaleGet(&scales,owner,10)==2);
    CHECK(editorInstanceScaleGet(&scales,owner,11)==1); /* reused address, new identity */
    CHECK(editorInstanceScaleGet(&scales,&objects[1],10)==1); /* shared model, different owner */
    for(i=1;i<256;++i)CHECK(editorInstanceScaleSet(&scales,&objects[i],i,3));
    for(i=1;i<256;i+=2)CHECK(editorInstanceScaleSet(&scales,&objects[i],i,1));
    for(i=1;i<256;++i)CHECK(editorInstanceScaleGet(&scales,&objects[i],i)==(i%2?1:3));
    CHECK(!editorInstanceScaleSet(&scales,owner,10,NAN));
    CHECK(!editorInstanceScaleSet(&scales,owner,10,INFINITY));
    CHECK(!editorInstanceScaleSet(&scales,owner,10,0));
    CHECK(!editorInstanceScaleSet(&scales,owner,10,51));
    CHECK(editorInstanceScaleGet(&scales,owner,10)==2);
    CHECK(editorInstanceScaleSet(&scales,owner,10,.05f));
    CHECK(editorInstanceScaleGet(&scales,owner,10)==.05f);
    CHECK(editorInstanceScaleSet(&scales,owner,10,50));
    CHECK(editorInstanceScaleGet(&scales,owner,10)==50);
    editorInstanceScaleReset(&scales);CHECK(scales.count==0 && editorInstanceScaleGet(&scales,owner,10)==1);
    puts("Instance scales: per-object isolation, deletion, address reuse, bounds and universe reset passed.");return 0;
}
