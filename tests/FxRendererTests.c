#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "RemasteredFxRenderer.h"
#include "File.h"
#include "Memory.h"
#include "glinc.h"
#include "ModernGraphics.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../src/ThirdParty/stb/stb_image.h"
static const char *root;
static unsigned loads,vertices,uploads,deleted,pushes,lights,embeddedDecodes;
static int inPrimitive;
static int depthEnabled;
static GLenum depthFunction;
static float lastColour[4];
static float engineBlend=-1, savedEngineBlend;
void hwglCaptureEngineIdle(float blend){engineBlend=blend;}
static FILE *openResource(const char *name){char path[2048];snprintf(path,sizeof(path),"%s/%s",root?root:"",name);return fopen(path,"rb");}
bool32 fileExists(char *name,udword flags){FILE *f;(void)flags;f=openResource(name);if(!f)return FALSE;fclose(f);return TRUE;}
sdword fileSizeGet(char *name,udword flags){FILE *f=openResource(name);long size;(void)flags;if(!f)return -1;fseek(f,0,SEEK_END);size=ftell(f);fclose(f);return (sdword)size;}
sdword fileLoadAlloc(char *name,void **data,udword flags){FILE *f;sdword size=fileSizeGet(name,flags);*data=NULL;if(size<=0)return -1;
    f=openResource(name);if(!f)return -1;*data=malloc(size);assert(*data);assert(fread(*data,1,size,f)==(size_t)size);fclose(f);++loads;return size;}
void memFree(void *p){free(p);}
void vecNormalize(vector *v){float size=sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);assert(size>0);v->x/=size;v->y/=size;v->z/=size;}
/* Renderer contract test, not a substitute for DirectXTex pixel decoding. */
int hwModernGraphicsDecodeDds(const void *p,unsigned size,unsigned *w,unsigned *h,unsigned char **rgba){
    unsigned header[32];assert(size>=128);memcpy(header,p,128);
    assert(header[0]==0x20534444&&header[1]==124&&header[19]==32);
    *w=header[4];*h=header[3];assert(*w&&*h&&*w<=8192&&*h<=8192);
    if(header[20]==0x41){assert(header[23]==0xff&&header[25]==0xff0000);assert(size==128+*w**h*4);}
    else {unsigned block;assert(header[20]==4);assert(header[21]==0x31545844||header[21]==0x33545844||header[21]==0x35545844);
        block=header[21]==0x31545844?8:16;assert(size==128+((*w+3)/4)*((*h+3)/4)*block);}
    *rgba=calloc((size_t)*w**h,4);assert(*rgba);++embeddedDecodes;return 1;
}
void hwModernGraphicsFreeDecodedDds(void *p){free(p);}
void hwModernGraphicsSubmitDynamicLight(const HWModernDynamicLightEmitter *light){assert(isfinite(light->radius+light->intensity));++lights;}
void hwglPushAttrib(GLbitfield mask){(void)mask;savedEngineBlend=engineBlend;++pushes;}
void hwglPopAttrib(void){assert(pushes);engineBlend=savedEngineBlend;--pushes;}
void hwglGenTextures(GLsizei n,GLuint *t){int i;for(i=0;i<n;++i)t[i]=uploads+i+1;}
void hwglDeleteTextures(GLsizei n,const GLuint *t){(void)t;deleted+=n;}
void hwglBindTexture(GLenum target,GLuint texture){(void)target;assert(texture);}
static unsigned repeatSettings,clampSettings;
void hwglTexParameteri(GLenum a,GLenum b,GLint c){(void)a;
    if(b==GL_TEXTURE_WRAP_S||b==GL_TEXTURE_WRAP_T){if(c==GL_REPEAT)++repeatSettings;if(c==GL_CLAMP_TO_EDGE)++clampSettings;}}
void hwglTexImage2D(GLenum target,GLint level,GLint format,GLsizei w,GLsizei h,GLint border,GLenum pf,GLenum type,const GLvoid *pixels){
    (void)target;(void)level;(void)format;(void)border;(void)pf;(void)type;assert(w>0&&h>0&&pixels);++uploads;}
void hwglTexEnvi(GLenum a,GLenum b,GLint c){(void)a;(void)b;(void)c;}
void hwglEnable(GLenum cap){if(cap==GL_DEPTH_TEST)depthEnabled=1;assert(!inPrimitive);}
void hwglDisable(GLenum cap){if(cap==GL_DEPTH_TEST)depthEnabled=0;assert(!inPrimitive);}
void hwglDepthFunc(GLenum function){depthFunction=function;}
void hwglDepthMask(GLboolean mask){assert(mask==GL_FALSE);}
void hwglBlendFunc(GLenum a,GLenum b){(void)a;(void)b;}
void hwglColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a){assert(isfinite(r+g+b+a));lastColour[0]=r;lastColour[1]=g;lastColour[2]=b;lastColour[3]=a;}
void hwglBegin(GLenum mode){assert(engineBlend<0);assert(depthEnabled&&depthFunction==GL_LEQUAL);assert(!inPrimitive&&(mode==GL_TRIANGLES||mode==GL_TRIANGLE_STRIP));inPrimitive=1;}
void hwglEnd(void){assert(inPrimitive);inPrimitive=0;}
void hwglTexCoord2f(GLfloat u,GLfloat v){assert(inPrimitive&&isfinite(u+v));}
void hwglVertex3f(GLfloat x,GLfloat y,GLfloat z){assert(inPrimitive&&isfinite(x+y+z));++vertices;}
void hwglVertex3fv(const GLfloat *v){hwglVertex3f(v[0],v[1],v[2]);}
static void *readFixture(const char *name,size_t *size){sdword n;void *bytes=NULL;n=fileLoadAlloc((char*)name,&bytes,0);*size=n>0?n:0;return bytes;}
int main(int argc,char **argv){
    unsigned char empty[37]={0};HwRmFxResources *resources;HwRmFxGraph *graph=NULL;char error[256];Camera camera={0};HwRmFxDraw draw={0};
    HwRmFxNode node={"test",HW_RM_FX_RING,0,NULL};HwRmFxTrailPoint trail[]={{{0,0,0},0},{{10,0,0},.1f},{{10,10,0},.2f}};
    memcpy(empty,"OWFXRS01",8);empty[32]=1;
    if(argc==2){size_t size;void *bytes;root=argv[1];bytes=readFixture("RemasteredFX/Native/effects.fxresources",&size);assert(bytes);
        resources=hwRmFxResourcesRead(bytes,size,error,sizeof(error));free(bytes);
        bytes=readFixture("RemasteredFX/Native/effects.fxbank",&size);assert(bytes);graph=hwRmFxGraphRead(bytes,size,error,sizeof(error));free(bytes);assert(graph);
    }else resources=hwRmFxResourcesRead(empty,sizeof(empty),error,sizeof(error));
    if(!resources){fprintf(stderr,"%s\n",error);return 1;}
    assert(rmFxRendererStartup(resources));camera.eyeposition.y=-100;camera.upvector.z=1;
    draw.matrix[0]=draw.matrix[5]=draw.matrix[10]=draw.matrix[15]=1;draw.scale=1;draw.age=.1f;draw.phase=.5f;draw.node=&node;
    {unsigned before=loads;engineBlend=.25f;rmFxRendererBegin(&camera);rmFxRendererDraw(NULL,&draw);assert(vertices==4);
        node.style=HW_RM_FX_BEAM;rmFxRendererDraw(NULL,&draw);assert(vertices==8);
        node.style=HW_RM_FX_TRAIL;draw.trail=trail;draw.trailCount=3;rmFxRendererDraw(NULL,&draw);assert(vertices==14);
        node.name="ion_beam_kus_simple";node.style=HW_RM_FX_BEAM;
        rmFxRendererDraw(NULL,&draw);assert(lastColour[0]==2 && lastColour[3]==1);
        node.style=HW_RM_FX_RING;rmFxRendererDraw(NULL,&draw);assert(lastColour[0]==1);
        node.name="test";node.style=HW_RM_FX_BEAM;
        rmFxRendererDraw(NULL,&draw);assert(lastColour[0]==1);
        {float colour[4]={1,.5f,.25f,0};HwRmFxProperty property={0};HwRmFxNode faded=node;
            property.name="colour";property.type=HW_RM_FX_COLOUR;property.count=4;property.values=colour;
            faded.style=HW_RM_FX_RING;faded.propertyCount=1;faded.properties=&property;draw.node=&faded;
            rmFxRendererDraw(NULL,&draw);assert(lastColour[0]==0&&lastColour[1]==0);
            colour[3]=.5f;rmFxRendererDraw(NULL,&draw);assert(lastColour[0]==.5f&&lastColour[1]==.25f);
            draw.node=&node;}
        {float disabled=0;HwRmFxProperty property={0};HwRmFxNode noDepth=node;
            property.name="usedepthtest";property.type=HW_RM_FX_BOOL;property.count=1;property.values=&disabled;
            noDepth.style=HW_RM_FX_RING;noDepth.propertyCount=1;noDepth.properties=&property;draw.node=&noDepth;
            depthEnabled=0;rmFxRendererDraw(NULL,&draw);assert(depthEnabled&&depthFunction==GL_LEQUAL);}
        if(graph){unsigned i;for(i=0;i<hwRmFxGraphCount(graph);++i){draw.node=hwRmFxGraphAt(graph,i);rmFxRendererDraw(NULL,&draw);}}
        rmFxRendererEnd();assert(!pushes&&!inPrimitive&&loads==before);
        rmFxRendererBegin(&camera);rmFxRendererEnd();assert(loads==before);}
    if(graph){assert(embeddedDecodes>0);assert(repeatSettings>0&&clampSettings>0);printf("Validated %u embedded DDS mip headers and payload sizes; mesh repeat and sprite clamp sampling\n",embeddedDecodes);}
    rmFxRendererShutdown();assert(uploads==deleted);hwRmFxResourcesFree(resources);hwRmFxGraphFree(graph);
    printf("Native FX renderer: %u vertices, %u original image uploads, %u lights; no draw-time file reads\n",vertices,uploads,lights);return 0;
}
