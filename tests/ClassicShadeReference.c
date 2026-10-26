/* Test-only adapter around the ACTUAL unchanged Shader.c CPU implementation. */
#include "../src/Game/Shader.c"
udword lightNumLights=0;
void hwTestSetClassicShade(const HWLegacyShadeState *s) {
    unsigned l;
    memcpy(shBaseColor,s->base,sizeof(shBaseColor));shBaseAlpha=s->base[3];
    lightNumLights=(udword)s->parameters[1];
    for(l=0;l<2;++l){memcpy(shLight[l].inverseworldposition,s->direction[l],16);memcpy(shLight[l].matdiffuse,s->diffuse[l],16);}
    shDockScalarRed=s->dock[0];shDockScalarGreen=s->dock[1];shDockScalarBlue=s->dock[2];shDockFactor=s->dock[3];
}
void hwTestEmitClassicShade(const float normal[3]) {
    vector n;n.x=normal[0];n.y=normal[1];n.z=normal[2];shColourSet0(&n);
}
int hwTestCaptureClassicShade(HWLegacyShadeState *s){return shCapturePersistentShade(s);}
/* Unused legacy-module dependencies: fail if the test accidentally reaches them. */
#include <stdlib.h>
bool32 bFade=FALSE;
real32 meshFadeAlpha=1.0f;
const hmatrix IdentityHMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void vecNormalize(vector *a){(void)a;abort();}
#if MEM_USE_NAMES
void *memAllocFunction(sdword length,char *name,udword flags){(void)length;(void)name;(void)flags;abort();return NULL;}
void *memReallocFunction(void *p,sdword length,char *name,udword flags){(void)p;(void)length;(void)name;(void)flags;abort();return NULL;}
#else
void *memAllocFunction(sdword length,udword flags){(void)length;(void)flags;abort();return NULL;}
void *memReallocFunction(void *p,sdword length,udword flags){(void)p;(void)length;(void)flags;abort();return NULL;}
#endif
void memFree(void *p){(void)p;abort();}
void dbgFatalf(char *file,sdword line,char *format,...)
{(void)file;(void)line;(void)format;abort();}
