#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char ubyte;
typedef unsigned int udword;
typedef int filehandle;
enum { FF_ReturnNULLOnFail=64, FF_IgnoreBIG=256, FF_IgnorePrepend=512 };
static int opened, closed, position, inputLength, lastFlags, missing;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static filehandle fileOpen(char *name, udword flags)
{
    CHECK(name[0] != 0); CHECK(flags & FF_ReturnNULLOnFail);
    lastFlags=flags; position=0;
    if(missing)return 0;
    ++opened;return 1;
}
static void fileClose(filehandle handle) { CHECK(handle==1); ++closed; }
static int fileBlockReadNoError(filehandle handle, void *out, int count)
{
    int n=inputLength-position;
    CHECK(handle==1);
    if(n>count)n=count;
    if(n>7001)n=7001; /* short reads are not necessarily EOF */
    memset(out,0x93,n);position+=n;return n;
}
#include "../src/Game/LoadingArtwork.inc"
int main(void)
{
    int length=42,i;
    ubyte *bytes;
    CHECK(hrReadOptionalArtwork("",&length)==NULL && length==0 && opened==0);
    CHECK(hrReadOptionalArtwork(NULL,&length)==NULL && opened==0);
    missing=1;CHECK(hrReadOptionalArtwork("missing.jpg",&length)==NULL && closed==0);missing=0;
    inputLength=0;CHECK(hrReadOptionalArtwork("directory",&length)==NULL && closed==1);
    inputLength=190123;
    bytes=hrReadOptionalArtwork("ScreenShots/test.jpg",&length);
    CHECK(bytes && length==inputLength && bytes[length-1]==0x93);
    CHECK(!(lastFlags & FF_IgnorePrepend));free(bytes);
    bytes=hrReadOptionalArtwork("F:/OpenHW/ScreenShots/test.jpg",&length);
    CHECK(bytes && (lastFlags & FF_IgnorePrepend) && (lastFlags & FF_IgnoreBIG));free(bytes);
    for(i=0;i<200;i++){bytes=hrReadOptionalArtwork("test.png",&length);CHECK(bytes!=NULL);free(bytes);}
    CHECK(opened==closed);
    inputLength=64*1024*1024;CHECK(hrReadOptionalArtwork("oversized.png",&length)==NULL && length==0);
    CHECK(opened==closed);
    puts("Optional loading artwork: empty, missing, directory, archive, absolute path, repeated loads and size bounds passed.");
    return 0;
}
