#include "../Modern/DiagnosticPolicy.h"
/* Versioned local scene cache. Never interpret HOD strides inside the game. */
typedef struct RMSceneDraw
{
    unsigned int program, flags, count, frameCount, textures[10];
    float cycle, expansion, parameters[512];
    int timeSlot, fadeSlot, matrixSlot;
    float *frames;
    HWBackgroundVertex *vertices;
} RMSceneDraw;
static RMSceneDraw *sceneDraws;
static unsigned int sceneDrawCount, sceneTextureCount;
static GLuint *sceneTextures;
static Uint64 sceneStartTicks;
typedef struct RMFlare { unsigned int texture; float record[10]; } RMFlare;
static RMFlare *sceneFlares;
static GLuint *flareTextures;
static unsigned int flareCount,flareTextureCount;

static void releaseScene(void)
{
    unsigned int i;
    for (i=0; i<sceneDrawCount; ++i)
    { SDL_free(sceneDraws[i].frames); SDL_free(sceneDraws[i].vertices); }
    SDL_free(sceneDraws); sceneDraws=NULL; sceneDrawCount=0;
    if (sceneTextures) glDeleteTextures(sceneTextureCount,sceneTextures);
    SDL_free(sceneTextures); sceneTextures=NULL; sceneTextureCount=0;
    if(flareTextures)glDeleteTextures(flareTextureCount,flareTextures);
    SDL_free(flareTextures);SDL_free(sceneFlares);
    flareTextures=NULL;sceneFlares=NULL;flareCount=flareTextureCount=0;
}

typedef struct RMReader { const unsigned char *next; size_t left; int failed; } RMReader;
static const unsigned char *rmTake(RMReader *r, size_t n)
{
    const unsigned char *result=r->next;
    if (n>r->left) { r->failed=1; return NULL; }
    r->next+=n; r->left-=n; return result;
}
static unsigned int rmU32(RMReader *r)
{
    const unsigned char *p=rmTake(r,4); return p?readU32(p):0;
}
static float rmFloat(RMReader *r)
{
    float f=0; const void *p=rmTake(r,4); if(p)memcpy(&f,p,4);
    if(!isfinite(f))r->failed=1;return f;
}
static int rmCopyFloats(RMReader *r, float *destination, size_t count)
{
    size_t i; const void *p=rmTake(r,count*4);
    if(!p)return 0;memcpy(destination,p,count*4);
    for(i=0;i<count;++i)if(!isfinite(destination[i])){r->failed=1;return 0;}
    return 1;
}

static int loadFlares(sdword mission,const char *root)
{
    char path[1024];size_t size=0;unsigned char *bytes;RMReader r;unsigned int i;
    snprintf(path,sizeof(path),"%s/RemasteredBackgrounds/mission%02d_flares.bin",root,mission);
    bytes=SDL_LoadFile(path,&size);
    if(!bytes || size<16 || size>64u*1024u*1024u || memcmp(bytes,"RMLF1\0\0\0",8))
    {SDL_free(bytes);return 0;}
    r.next=bytes+8;r.left=size-8;r.failed=0;
    flareTextureCount=rmU32(&r);flareCount=rmU32(&r);
    if(flareTextureCount>128 || flareCount>4096){SDL_free(bytes);flareCount=flareTextureCount=0;return 0;}
    flareTextures=SDL_calloc(flareTextureCount?flareTextureCount:1,sizeof(GLuint));
    sceneFlares=SDL_calloc(flareCount?flareCount:1,sizeof(RMFlare));
    if(!flareTextures || !sceneFlares){SDL_free(bytes);return 0;}
    for(i=0;i<flareTextureCount && !r.failed;++i)
    {
        unsigned int w=rmU32(&r),h=rmU32(&r),n=rmU32(&r);const void *pixels;
        if(!w || !h || w>2048 || h>2048 || (size_t)w*h*4!=n){r.failed=1;break;}
        pixels=rmTake(&r,n);if(!pixels)break;
        glGenTextures(1,flareTextures+i);trClearCurrent();glBindTexture(GL_TEXTURE_2D,flareTextures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
    for(i=0;i<flareCount && !r.failed;++i)
    {
        sceneFlares[i].texture=rmU32(&r);
        if(sceneFlares[i].texture>=flareTextureCount || !rmCopyFloats(&r,sceneFlares[i].record,10))r.failed=1;
    }
    SDL_free(bytes);trClearCurrent();return !r.failed && !r.left;
}

static int loadScene(sdword mission, const char *root)
{
    char path[1024]; size_t size=0; unsigned int textures,programs,draws,i,j;
    unsigned int programIds[32]={0}; unsigned char *bytes; RMReader r;
    SDL_RWops *file;
    Sint64 length;
    snprintf(path,sizeof(path),"%s/RemasteredBackgrounds/mission%02d_scene.bin",root,mission);
    file=SDL_RWFromFile(path,"rb");
    if(!file)
    { HW_DIAG_FPRINTF(stderr,"[RemasteredScene] Mission %02d scene missing: %s (%s).\n",mission,path,SDL_GetError());return 0; }
    length=SDL_RWsize(file);
    if(length<20 || (Uint64)length>HW_RM_SCENE_MAX_BYTES)
    {
        HW_DIAG_FPRINTF(stderr,"[RemasteredScene] Mission %02d scene rejected: %lld bytes (limit %u): %s.\n",
            mission,(long long)length,HW_RM_SCENE_MAX_BYTES,path);
        SDL_RWclose(file);return 0;
    }
    bytes=SDL_LoadFile_RW(file,&size,1);
    if(!bytes || size<20 || size>HW_RM_SCENE_MAX_BYTES || memcmp(bytes,"RMSC3\0\0\0",8))
    { HW_DIAG_FPRINTF(stderr,"[RemasteredScene] Mission %02d scene read/header validation failed: %s.\n",mission,path);SDL_free(bytes);return 0; }
    r.next=bytes+8;r.left=size-8;r.failed=0;
    textures=rmU32(&r);programs=rmU32(&r);draws=rmU32(&r);
    if(textures>128 || programs>32 || draws>256){SDL_free(bytes);return 0;}
    sceneTextures=SDL_calloc(textures?textures:1,sizeof(GLuint));
    sceneDraws=SDL_calloc(draws?draws:1,sizeof(RMSceneDraw));
    if(!sceneTextures || !sceneDraws){releaseScene();SDL_free(bytes);return 0;}
    sceneTextureCount=textures;sceneDrawCount=draws;
    for(i=0;i<textures && !r.failed;++i)
    {
        unsigned int width=rmU32(&r),height=rmU32(&r),length=rmU32(&r);
        const void *pixels;
        if(!width || !height || width>8192 || height>8192 ||
            (size_t)width*height*4!=length){r.failed=1;break;}
        pixels=rmTake(&r,length);if(!pixels)break;
        glGenTextures(1,&sceneTextures[i]);trClearCurrent();
        glBindTexture(GL_TEXTURE_2D,sceneTextures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
    for(i=0;i<programs && !r.failed;++i)
    {
        unsigned int length=rmU32(&r);const void *code;
        if(length<32 || length>2u*1024u*1024u){r.failed=1;break;}
        code=rmTake(&r,length);if(!code)break;
        programIds[i]=hwModernGraphicsCreateBackgroundProgram(code,length);
        if(!programIds[i])r.failed=1;
    }
    for(i=0;i<draws && !r.failed;++i)
    {
        RMSceneDraw *d=sceneDraws+i;unsigned int program=rmU32(&r);
        d->flags=rmU32(&r);d->count=rmU32(&r);d->frameCount=rmU32(&r);
        d->cycle=rmFloat(&r);d->expansion=rmFloat(&r);
        d->timeSlot=(int)rmU32(&r);d->fadeSlot=(int)rmU32(&r);d->matrixSlot=(int)rmU32(&r);
        if(program>=programs || (d->flags&~15u) || (d->flags&12u)==12u ||
            !d->count || d->count>1000000 || d->count%3 || !d->frameCount || d->frameCount>1024 ||
            d->cycle<0 || d->timeSlot< -1 || d->timeSlot>127 || d->fadeSlot< -1 || d->fadeSlot>127 ||
            d->matrixSlot< -1 || d->matrixSlot>124){r.failed=1;break;}
        d->program=programIds[program];
        for(j=0;j<10;++j)
        {
            unsigned int t=rmU32(&r);
            if(t==0xfffffffeu)d->textures[j]=skyTexture;
            else if(t==0xffffffffu)d->textures[j]=0;
            else if(t<textures)d->textures[j]=sceneTextures[t];
            else r.failed=1;
        }
        if(!rmCopyFloats(&r,d->parameters,512))break;
        d->frames=SDL_malloc((size_t)d->frameCount*16*4);
        d->vertices=SDL_malloc((size_t)d->count*sizeof(HWBackgroundVertex));
        if(!d->frames || !d->vertices || !rmCopyFloats(&r,d->frames,(size_t)d->frameCount*16) ||
            !rmCopyFloats(&r,(float*)d->vertices,(size_t)d->count*20)){r.failed=1;break;}
    }
    SDL_free(bytes);trClearCurrent();
    if(r.failed || r.left || !loadFlares(mission,root))
    { HW_DIAG_FPRINTF(stderr,"[RemasteredScene] Mission %02d scene/material/flare validation failed: %s (remaining=%llu).\n",mission,path,(unsigned long long)r.left);releaseScene();return 0; }
    sceneStartTicks=SDL_GetTicks64();
    HW_DIAG_FPRINTF(stderr,"[RemasteredScene] Mission %02d: %u material draws, %u textures, %u programs.\n",
        mission,draws,textures,programs);
    return 1;
}

static void renderScene(float fade)
{
    unsigned int i,j;double elapsed=(SDL_GetTicks64()-sceneStartTicks)*.001;
    if(sceneDrawCount)glClear(GL_DEPTH_BUFFER_BIT);
    for(i=0;i<sceneDrawCount;++i)
    {
        RMSceneDraw *d=sceneDraws+i;float model[16];unsigned int a=0,b=0;float t=0;
        if(d->cycle>0 && d->frameCount>1)
        {
            double frame=fmod(elapsed,d->cycle)/d->cycle*(d->frameCount-1);
            a=(unsigned int)frame;b=a+1;t=(float)(frame-a);
        }
        for(j=0;j<16;++j)model[j]=d->frames[a*16+j]*(1-t)+d->frames[b*16+j]*t;
        if(d->timeSlot>=0)
        {
            float *v=d->parameters+d->timeSlot*4;
            v[0]=v[1]=v[2]=(float)elapsed;v[3]=1;
        }
        if(d->fadeSlot>=0)
        {float *v=d->parameters+d->fadeSlot*4;v[0]=v[1]=v[2]=v[3]=fade;}
        if(d->matrixSlot>=0)memcpy(d->parameters+d->matrixSlot*4,model,sizeof(model));
        hwModernGraphicsRenderBackgroundMesh(d->program,d->textures,d->vertices,d->count,
            d->parameters,model,d->flags,d->expansion);
    }
    /* Private background depth only: do not hand planet depths to the cloud
       compositor, antialiaser, or gameplay geometry. Coverage alpha stayed zero. */
    if(sceneDrawCount)glClear(GL_DEPTH_BUFFER_BIT);
    /* Optical flares are composited after opaque geometry/dust, with source
       visibility. Drawing ghosts into the sky here bypassed occlusion. */
}
