#include "../Modern/DiagnosticPolicy.h"
#include "RemasteredFxRenderer.h"
#include "RemasteredSubsystem.h"
#include "File.h"
#include "Memory.h"
#include "Vector.h"
#include "glinc.h"
#include "ModernGraphics.h"
#include "../ThirdParty/stb/stb_image.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FxImage {const char *name;unsigned char *rgba;int width,height,modern;GLuint texture;} FxImage;
typedef struct FxModel {const char *name;HwRmHodScene *scene;HwRmSubsystemLayer *layer;HwRmSubsystemInstance *instance;FxImage *materials;unsigned materialCount;} FxModel;
static HwRmFxResources *resourceBank;
static FxImage *images;static unsigned imageCount;
static FxModel *models;static unsigned modelCount;
static Camera *view;static vector cameraRight,cameraUp,cameraForward;
static const HwRmFxDraw *drawingModel;
static FxModel *drawingModelAsset;
static int drawingEmbeddedMaterial;
static float drawingModelUV[4];
static float drawingModelColour[4];
static unsigned errors;
static void freeImage(FxImage *image){
    if(image->texture)glDeleteTextures(1,&image->texture);
    if(image->rgba){if(image->modern)hwModernGraphicsFreeDecodedDds(image->rgba);else stbi_image_free(image->rgba);}
}
static void uploadImage(FxImage *image){
    if(image->texture||!image->rgba)return;
    glGenTextures(1,&image->texture);glBindTexture(GL_TEXTURE_2D,image->texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image->width,image->height,0,GL_RGBA,GL_UNSIGNED_BYTE,image->rgba);
}
/* Wrap an original embedded mip for the existing DDS decoder. No asset rewrite. */
static int loadMaterial(FxModel *model,unsigned material,char *error,size_t capacity){
    const HwRmHodMaterial *m=hwRmHodSceneMaterial(model->scene,material);
    HwRmHodTextureView mip;unsigned p,w,h,header[32]={0};unsigned char *dds;FxImage *image=&model->materials[material];
    for(p=0;p<m->parameterCount;++p)if(m->parameters[p].type==5&&!strcmp(m->parameters[p].name,"$diffuse"))break;
    if(p==m->parameterCount)return 1;
    if(!hwRmHodMaterialTexture(model->scene,material,"$diffuse",0,&mip,error,capacity))return 0;
    if(mip.size>128*1024*1024||!mip.width||!mip.height||mip.width>8192||mip.height>8192)return 0;
    header[0]=0x20534444;header[1]=124;header[2]=0x1007;header[3]=mip.height;header[4]=mip.width;
    header[19]=32;header[27]=0x1000;
    if(!strcmp(mip.format,"8888")){
        header[20]=0x41;header[22]=32;header[23]=0x000000ff;header[24]=0x0000ff00;header[25]=0x00ff0000;header[26]=0xff000000;
    }else{header[20]=4;memcpy(&header[21],mip.format,4);}
    dds=malloc(128+mip.size);if(!dds)return 0;
    memcpy(dds,header,128);memcpy(dds+128,mip.bytes,mip.size);
    p=hwModernGraphicsDecodeDds(dds,(unsigned)(128+mip.size),&w,&h,&image->rgba);free(dds);
    if(!p)return 0;image->width=(int)w;image->height=(int)h;image->modern=1;return 1;
}
static void report(const char *name,const char *reason){if(errors++<32)HW_DIAG_FPRINTF(stderr,"[NativeFX] %s: %s\n",name?name:"",reason);}
static void *readResource(const char *name,sdword *size){void *bytes=NULL;
    if(!fileExists((char*)name,0))return NULL;
    *size=fileSizeGet((char*)name,0);if(*size<=0||*size>128*1024*1024)return NULL;
    if(fileLoadAlloc((char*)name,&bytes,FF_ReturnNULLOnFail)!=*size){if(bytes)memFree(bytes);return NULL;}return bytes;}
static int imageIndex(const char *name){unsigned i;for(i=0;i<imageCount;++i)if(!strcmp(images[i].name,name))return (int)i;return -1;}
static int loadImage(const char *name){sdword size;void *bytes;FxImage *image;unsigned w=0,h=0;int channels;
    if(imageIndex(name)>=0)return 1;if(imageCount>=8192)return 0;
    bytes=readResource(name,&size);if(!bytes)return 0;image=&images[imageCount];image->name=name;
    if(strlen(name)>4&&!strcmp(name+strlen(name)-4,".dds")){
        if(hwModernGraphicsDecodeDds(bytes,(unsigned)size,&w,&h,&image->rgba)){image->width=(int)w;image->height=(int)h;image->modern=1;}
    }else image->rgba=stbi_load_from_memory(bytes,size,&image->width,&image->height,&channels,4);
    memFree(bytes);
    if(!image->rgba||image->width<=0||image->height<=0||image->width>8192||image->height>8192){
        if(image->rgba){if(image->modern)hwModernGraphicsFreeDecodedDds(image->rgba);else stbi_image_free(image->rgba);}memset(image,0,sizeof(*image));return 0;}
    ++imageCount;return 1;
}
static int loadModel(const char *name){unsigned i;sdword size;void *bytes;FxModel *model;char error[256];
    for(i=0;i<modelCount;++i)if(!strcmp(models[i].name,name))return 1;
    if(modelCount>=8192)return 0;bytes=readResource(name,&size);if(!bytes)return 0;
    model=&models[modelCount];model->name=name;
    if(!hwRmHodSceneRead(bytes,(size_t)size,&model->scene,error,sizeof(error))){memFree(bytes);report(name,error);return 0;}memFree(bytes);
    model->layer=hwRmSubsystemLayerCreate(model->scene,error,sizeof(error));
    if(model->layer)model->instance=hwRmSubsystemInstanceCreate(model->layer);
    if(!model->instance){hwRmSubsystemLayerFree(model->layer);hwRmHodSceneFree(model->scene);memset(model,0,sizeof(*model));return 0;}
    /* Include partially loaded models in shutdown cleanup if a material fails. */
    ++modelCount;model->materialCount=hwRmHodSceneMaterialCount(model->scene);
    model->materials=calloc(model->materialCount?model->materialCount:1,sizeof(*model->materials));
    if(!model->materials)return 0;
    for(i=0;i<model->materialCount;++i)if(!loadMaterial(model,i,error,sizeof(error))){report(name,"embedded FX diffuse decode failed");return 0;}
    return 1;
}
int rmFxRendererStartup(HwRmFxResources *resources){unsigned i,j;rmFxRendererShutdown();errors=0;
    if(!resources)return 0;resourceBank=resources;images=calloc(8192,sizeof(*images));models=calloc(8192,sizeof(*models));
    if(!images||!models){rmFxRendererShutdown();return 0;}
    for(i=0;i<hwRmFxBitmapCount(resources);++i){const HwRmFxBitmap *bitmap=hwRmFxBitmapAt(resources,i);
        for(j=0;j<bitmap->frameCount;++j)if(!loadImage(bitmap->frames[j].image)){report(bitmap->frames[j].image,"native bitmap precache failed");rmFxRendererShutdown();return 0;}}
    for(i=0;i<hwRmFxMeshCount(resources);++i){const HwRmFxMeshes *group=hwRmFxMeshAt(resources,i);
        for(j=0;j<group->count;++j)if(!loadModel(group->models[j])){report(group->models[j],"native mesh precache failed");rmFxRendererShutdown();return 0;}}
    return 1;
}
void rmFxRendererShutdown(void){unsigned i;
    for(i=0;i<imageCount;++i){if(images[i].texture)glDeleteTextures(1,&images[i].texture);
        if(images[i].rgba){if(images[i].modern)hwModernGraphicsFreeDecodedDds(images[i].rgba);else stbi_image_free(images[i].rgba);}}
    for(i=0;i<modelCount;++i){unsigned j;if(models[i].materials)for(j=0;j<models[i].materialCount;++j)freeImage(&models[i].materials[j]);free(models[i].materials);hwRmSubsystemInstanceFree(models[i].instance);hwRmSubsystemLayerFree(models[i].layer);hwRmHodSceneFree(models[i].scene);}
    free(images);free(models);images=NULL;models=NULL;imageCount=modelCount=0;resourceBank=NULL;view=NULL;
}
void rmFxRendererBegin(Camera *camera){unsigned i;view=camera;if(!camera||!resourceBank)return;
    vecSub(cameraForward,camera->lookatpoint,camera->eyeposition);vecNormalize(&cameraForward);
    vecCrossProduct(cameraRight,cameraForward,camera->upvector);vecNormalize(&cameraRight);vecCrossProduct(cameraUp,cameraRight,cameraForward);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_FOG);
#ifdef HW_ENABLE_D3D12_BACKEND
    /* FX textures must never blend with a preceding hull's idle engine map. */
    hwglCaptureEngineIdle(-1.0f);
#endif
    glEnable(GL_BLEND);glEnable(GL_TEXTURE_2D);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for(i=0;i<imageCount;++i)if(!images[i].texture){FxImage *image=&images[i];
        glGenTextures(1,&image->texture);glBindTexture(GL_TEXTURE_2D,image->texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image->width,image->height,0,GL_RGBA,GL_UNSIGNED_BYTE,image->rgba);
    }
}
void rmFxRendererEnd(void){if(view&&resourceBank)glPopAttrib();view=NULL;}
static float scalar(const HwRmFxNode *node,const char *name,float phase,float fallback){float v[4];return hwRmFxGraphSample(hwRmFxGraphProperty(node,name),phase,v)?v[0]:fallback;}
static void values(const HwRmFxNode *node,const char *name,float phase,float *v){hwRmFxGraphSample(hwRmFxGraphProperty(node,name),phase,v);}
static const char *text(const HwRmFxNode *node,const char *name){const HwRmFxProperty *p=hwRmFxGraphProperty(node,name);return p&&p->type==HW_RM_FX_STRING?p->text:"";}
static void point(const float *matrix,const float *local,vector *world){
    world->x=matrix[12]+matrix[0]*local[0]+matrix[4]*local[1]+matrix[8]*local[2];
    world->y=matrix[13]+matrix[1]*local[0]+matrix[5]*local[1]+matrix[9]*local[2];
    world->z=matrix[14]+matrix[2]*local[0]+matrix[6]*local[1]+matrix[10]*local[2];}
static void spinFrame(float matrix[16],const float spin[3]){unsigned axis,row;const unsigned a[]={1,2,0},b[]={2,0,1};
    for(axis=0;axis<3;++axis){float c=cosf(spin[axis]),s=sinf(spin[axis]);
        for(row=0;row<3;++row){float x=matrix[a[axis]*4+row],y=matrix[b[axis]*4+row];
            matrix[a[axis]*4+row]=c*x+s*y;matrix[b[axis]*4+row]=-s*x+c*y;}}}
static int bindFrame(const HwRmFxBitmapFrame *frame,float uv[4]){int index;FxImage *image;if(!frame||(index=imageIndex(frame->image))<0)return 0;
    image=&images[index];glBindTexture(GL_TEXTURE_2D,image->texture);
    /* Sprite atlas sampling clamps; mesh draws below explicitly opt into
       wrapping. Texture-object state must not leak into the next sprite. */
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    uv[0]=(float)frame->column/frame->columns;uv[1]=(float)frame->row/frame->rows;
    uv[2]=(float)(frame->column+1)/frame->columns;uv[3]=(float)(frame->row+1)/frame->rows;
    if(frame->rows>1||frame->columns>1){uv[0]+=.5f/image->width;uv[2]-=.5f/image->width;uv[1]+=.5f/image->height;uv[3]-=.5f/image->height;}
    return 1;
}
static void quad(const vector *center,const vector *u,const vector *v,const float uv[4]){int i;vector p;
    glBegin(GL_TRIANGLE_STRIP);for(i=0;i<4;++i){float x=(i&1)?1.f:-1.f,y=(i&2)?1.f:-1.f;
        p.x=center->x+x*u->x+y*v->x;p.y=center->y+x*u->y+y*v->y;p.z=center->z+x*u->z+y*v->z;
        glTexCoord2f((i&1)?uv[2]:uv[0],(i&2)?uv[3]:uv[1]);glVertex3fv((const GLfloat*)&p);}glEnd();}
static void drawMesh(void *user,const HwRmSubsystemDraw *draw){unsigned i;const HwRmFxDraw *fx=drawingModel;(void)user;
    float uv[4];memcpy(uv,drawingModelUV,sizeof(uv));
    if(drawingEmbeddedMaterial){
        FxImage *image=draw->part->material<drawingModelAsset->materialCount?&drawingModelAsset->materials[draw->part->material]:NULL;
        if(image&&image->rgba){uploadImage(image);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,image->texture);uv[0]=uv[1]=0;uv[2]=uv[3]=1;}
        else {glDisable(GL_TEXTURE_2D);}
    }
    /* RM mesh UVs may deliberately cross multiple tiles (bullet_splash uses
       U=-.94..1.85). Clamping them paints the edge texel over whole polygons.
       Atlas subrectangles cannot use whole-image wrapping. */
    if(uv[0]==0&&uv[1]==0&&uv[2]==1&&uv[3]==1){
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    }
    glColor4f(drawingModelColour[0],drawingModelColour[1],drawingModelColour[2],drawingModelColour[3]);
    glBegin(GL_TRIANGLES);for(i=0;i<draw->part->indexCount;++i){unsigned index=draw->part->indices[i];const HwHodVertex *v=&draw->part->vertices[index];vector hw,world;float rm[3];
        point(draw->matrix,v->position,&hw);rm[0]=-hw.y*fx->scale;rm[1]=hw.x*fx->scale;rm[2]=hw.z*fx->scale;
        point(fx->matrix,rm,&world);
        if(draw->part->vertexColors){unsigned rgba=draw->part->vertexColors[index];
            glColor4f(drawingModelColour[0]*((rgba>>24)&255)/255.f,drawingModelColour[1]*((rgba>>16)&255)/255.f,
                drawingModelColour[2]*((rgba>>8)&255)/255.f,drawingModelColour[3]*(rgba&255)/255.f);}
        glTexCoord2f(uv[0]+v->uv[0]*(uv[2]-uv[0]),uv[1]+v->uv[1]*(uv[3]-uv[1]));
        glVertex3fv((const GLfloat*)&world);}glEnd();
}
static void flare(const HwRmFxDraw *draw,const vector *origin,const float colour[4],float radius){char name[512];const HwRmFxFlare *f;unsigned i;vector delta,center,axisU,axisV;float depth;
    const char *key=text(draw->node,"lensflare");if(!*key)return;snprintf(name,sizeof(name),"effect/lensflare/%s.flare",key);f=hwRmFxFlareFind(resourceBank,name);if(!f)return;
    vecSub(delta,*origin,view->eyeposition);depth=vecDotProduct(delta,cameraForward);if(depth<=0)return;
    vecScalarMultiply(center,cameraForward,depth);vecAddTo(center,view->eyeposition);vecSub(delta,*origin,center);
    for(i=0;i<f->count;++i){const HwRmFxHotspot *hotspot=&f->hotspots[i];const HwRmFxBitmap *bitmap=hwRmFxBitmapFind(resourceBank,hotspot->image);float uv[4],fade=1;vector at;
        if(!bitmap||!bindFrame(bitmap->frames,uv))continue;
        if(f->fade[0]>0)fade=fminf(1,draw->age/f->fade[0]);
        glColor4f(colour[0]*hotspot->colour[0],colour[1]*hotspot->colour[1],colour[2]*hotspot->colour[2],colour[3]*hotspot->colour[3]*fade);
        vecScalarMultiply(at,delta,hotspot->offset);vecAddTo(at,center);
        vecScalarMultiply(axisU,cameraRight,radius*hotspot->scale);vecScalarMultiply(axisV,cameraUp,radius*hotspot->scale);quad(&at,&axisU,&axisV,uv);
    }
}
static void trail(const HwRmFxDraw *draw,const float uv[4]){
    const HwRmFxNode *node=draw->node;unsigned i,edge;float head[4]={1,1,1,1},tail[4]={1,1,1,0};
    float length=scalar(node,"length",0,.1f),width=scalar(node,"width",0,1)*draw->scale*.5f;
    float life=scalar(node,"duration",0,1),fade=scalar(node,"fadeouttime",0,0),alpha=1;
    if(!draw->trail||draw->trailCount<2||length<=0)return;
    values(node,"colourhead",0,head);values(node,"colourtail",0,tail);
    if(fade>0&&life>0)alpha=fmaxf(0,fminf(1,(life-draw->age)/(life*fade)));
    glBegin(GL_TRIANGLE_STRIP);
    for(i=0;i<draw->trailCount;++i){const HwRmFxTrailPoint *sample=&draw->trail[i];vector direction,side,at;
        unsigned before=i?i-1:i,after=i+1<draw->trailCount?i+1:i,c;
        float fraction=fmaxf(0,fminf(1,(draw->age-sample->age)/length)),colour[4];
        for(c=0;c<4;++c)colour[c]=head[c]+(tail[c]-head[c])*fraction;
        glColor4f(colour[0],colour[1],colour[2],colour[3]*alpha);
        direction.x=draw->trail[after].position[0]-draw->trail[before].position[0];
        direction.y=draw->trail[after].position[1]-draw->trail[before].position[1];
        direction.z=draw->trail[after].position[2]-draw->trail[before].position[2];
        if(scalar(node,"billboard",0,1)!=0){vecCrossProduct(side,direction,cameraForward);
            if(vecMagnitudeSquared(side)>.000001f)vecNormalize(&side);else side=cameraRight;
        }else {side.x=draw->matrix[0];side.y=draw->matrix[1];side.z=draw->matrix[2];}
        for(edge=0;edge<2;++edge){float sign=edge?1.f:-1.f;
            at.x=sample->position[0]+side.x*width*sign;at.y=sample->position[1]+side.y*width*sign;at.z=sample->position[2]+side.z*width*sign;
            glTexCoord2f(uv[0]+fraction*(uv[2]-uv[0]),edge?uv[3]:uv[1]);glVertex3fv((const GLfloat*)&at);}
    }
    glEnd();
}
void rmFxRendererDraw(void *user,const HwRmFxDraw *draw){const HwRmFxNode *node;float colour[4]={1,1,1,1},offset[4]={0},uv[4]={0,0,1,1},radius;vector origin,u,v,forward;unsigned blend;(void)user;
    if(!view||!resourceBank||!draw||!(node=draw->node))return;
    values(node,"colour",draw->phase,colour);values(node,"offset",draw->phase,offset);
    /* Ion beam radiance only: retain authored hue, alpha and fade timing.
       Do not brighten charge flashes, impacts, lights or other beam effects. */
    if(node->style==HW_RM_FX_BEAM && node->name && !strncmp(node->name,"ion_beam",8))
    { colour[0]*=2.0f;colour[1]*=2.0f;colour[2]*=2.0f; }
    offset[0]*=draw->scale;offset[1]*=draw->scale;offset[2]*=draw->scale;point(draw->matrix,offset,&origin);
    radius=scalar(node,"radius",draw->phase,1)*draw->scale*(draw->radiusMultiplier>0?draw->radiusMultiplier:1);
    blend=(unsigned)scalar(node,"blending",draw->phase,2);
    /* ONE/ONE blending does not consume source alpha. Apply the authored
       particle opacity to radiance, retaining RGB-authored additive sprites
       (many RM fire atlases have no alpha channel). Alpha/AdditiveAlpha already
       apply opacity through their blend factor and must not be faded twice. */
    if(blend==2 && node->style!=HW_RM_FX_LIGHT && node->style!=HW_RM_FX_LENSFLARE){
        float opacity=fminf(1.f,fmaxf(0.f,colour[3]));
        colour[0]*=opacity;colour[1]*=opacity;colour[2]*=opacity;
    }
    if(!blend)glDisable(GL_BLEND);else {glEnable(GL_BLEND);
        glBlendFunc(blend==2?GL_ONE:blend==3?GL_DST_COLOR:GL_SRC_ALPHA,blend==1?GL_ONE_MINUS_SRC_ALPHA:blend==3?GL_ZERO:GL_ONE);}
    /* Native FX are world-space geometry, never HUD overlays. RM's authored
       UseDepthTest=0 must not let flashes/explosions show through solid hulls.
       Keep depth writes off so translucent particles don't occlude each other. */
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);
    glColor4f(colour[0],colour[1],colour[2],colour[3]);
    if(node->style==HW_RM_FX_LIGHT){HWModernDynamicLightEmitter light;memset(&light,0,sizeof(light));light.source=HW_MODERN_LIGHT_NATIVE_FX;light.shape=HW_MODERN_LIGHT_POINT;
        light.position[0]=origin.x;light.position[1]=origin.y;light.position[2]=origin.z;memcpy(light.color,colour,sizeof(light.color));light.radius=radius;light.intensity=fmaxf(0,colour[3]);
        hwModernGraphicsSubmitDynamicLight(&light);return;}
    if(node->style==HW_RM_FX_LENSFLARE){flare(draw,&origin,colour,radius);return;}
    {
        const HwRmFxBitmap *bitmap=hwRmFxBitmapFind(resourceBank,text(node,"texture"));const HwRmFxBitmapFrame *frame;
        if(bitmap){frame=hwRmFxBitmapSample(bitmap,draw->age,draw->random,scalar(node,"randombitmap",draw->phase,0)!=0,1);
            if(!bindFrame(frame,uv))return;glEnable(GL_TEXTURE_2D);}else glDisable(GL_TEXTURE_2D);
    }
    if(node->style==HW_RM_FX_RING){
        if(scalar(node,"usemesh",draw->phase,0)!=0){const HwRmFxMeshes *group=hwRmFxMeshFind(resourceBank,text(node,"mesh"));unsigned i;HwRmFxDraw mesh=*draw;char error[256];
            if(!group)return;for(i=0;i<modelCount;++i)if(!strcmp(models[i].name,group->models[draw->random%group->count])){
                mesh.scale=radius;mesh.matrix[12]=origin.x;mesh.matrix[13]=origin.y;mesh.matrix[14]=origin.z;drawingModel=&mesh;
                {const HwRmFxBitmap *bitmap=hwRmFxBitmapFind(resourceBank,text(node,"texture"));
                /* point.tga is the editor placeholder for mesh-owned materials;
                   explicit animated FX textures retain their atlas sampling. */
                drawingEmbeddedMaterial=!bitmap||!strcmp(bitmap->name,"art/fx/point.tga");}
                drawingModelAsset=&models[i];
                memcpy(drawingModelUV,uv,sizeof(drawingModelUV));
                memcpy(drawingModelColour,colour,sizeof(drawingModelColour));
                spinFrame(mesh.matrix,draw->spin);
                if(!hwRmSubsystemVisit(models[i].instance,0,1,NULL,NULL,drawMesh,NULL,error,sizeof(error)))report(group->name,error);drawingModel=NULL;return;}return;
        }
        if(scalar(node,"billboard",draw->phase,1)!=0){vecScalarMultiply(u,cameraRight,radius);vecScalarMultiply(v,cameraUp,radius);}
        else {float matrix[16];memcpy(matrix,draw->matrix,sizeof(matrix));spinFrame(matrix,draw->spin);
            u.x=matrix[0]*radius;u.y=matrix[1]*radius;u.z=matrix[2]*radius;v.x=matrix[4]*radius;v.y=matrix[5]*radius;v.z=matrix[6]*radius;}
        if(scalar(node,"billboard",draw->phase,1)!=0){vector a=u,b=v;float c=cosf(draw->spin[2]),s=sinf(draw->spin[2]);
            u.x=a.x*c+b.x*s;u.y=a.y*c+b.y*s;u.z=a.z*c+b.z*s;
            v.x=b.x*c-a.x*s;v.y=b.y*c-a.y*s;v.z=b.z*c-a.z*s;}
        quad(&origin,&u,&v,uv);return;
    }
    if(node->style==HW_RM_FX_TRAIL){trail(draw,uv);return;}
    if(node->style==HW_RM_FX_BEAM){float width=scalar(node,"width",draw->phase,1)*draw->scale;
        float length=scalar(node,"length",draw->phase,1)*(draw->length>0?draw->length:draw->scale);vector side;
        forward.x=draw->matrix[8];forward.y=draw->matrix[9];forward.z=draw->matrix[10];vecNormalize(&forward);
        vecCrossProduct(side,forward,cameraForward);if(vecMagnitudeSquared(side)<.000001f)side=cameraRight;else vecNormalize(&side);
        vecScalarMultiply(u,forward,length*.5f);vecScalarMultiply(v,side,width*.5f);vecAddTo(origin,u);
        uv[0]=scalar(node,"texture_u_offset",draw->phase,0);uv[2]=uv[0]+scalar(node,"texture_u_repeat",draw->phase,1);quad(&origin,&u,&v,uv);return;
    }
    if(node->style==HW_RM_FX_HYPERSPACE){float width=scalar(node,"width",draw->phase,1)*draw->scale*.5f,height=scalar(node,"height",draw->phase,1)*draw->scale*.5f;
        u.x=draw->matrix[0]*width;u.y=draw->matrix[1]*width;u.z=draw->matrix[2]*width;v.x=draw->matrix[4]*height;v.y=draw->matrix[5]*height;v.z=draw->matrix[6]*height;quad(&origin,&u,&v,uv);}
}
