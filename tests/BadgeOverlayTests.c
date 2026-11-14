#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Mesh.h"
#include "MeshBadgeOverlay.h"
#include "BadgePresentation.h"
#include "RemasteredShipHod.h"
#include "ModernGraphics.h"
#include "glinc.h"
static unsigned uploads,submits,draws,vertices,pushes,variant,which;static int identities[2];static unsigned char rgba[4*4*4];
static unsigned deleted,unregistered;
void trRGBTextureDelete(udword texture){assert(texture);++deleted;}
void hwModernGraphicsUnregisterSurfaceTexture(const void*id){assert(id==&identities[0]||id==&identities[1]);++unregistered;}
const unsigned char*hwBadgePlayerPixels(unsigned slot,int local,unsigned*w,unsigned*h,const void**id){(void)slot;(void)local;*w=*h=4;*id=&identities[which];return rgba;}
udword trRGBTextureCreate(color*p,sdword w,sdword h,bool32 alpha){assert(p==(color*)rgba&&w==4&&h==4&&alpha);return ++uploads;}
void trRGBTextureMakeCurrent(udword t){assert(t);}
void hwModernGraphicsRegisterSurfaceTextureNamed(const void*id,const char*name,unsigned w,unsigned h,const unsigned*p,const unsigned*emission,const unsigned*normal,const unsigned*orm){unsigned i;assert(id==&identities[which]&&strstr(name,"rmnative")&&w==4&&h==4&&p==(unsigned*)rgba&&normal&&orm);for(i=0;i<16;i++)assert(emission[i]==0);}
int hwModernGraphicsIsRaytracingSceneOpen(void){return 1;}
void hwModernGraphicsSubmitTriangleGeometryMaskedIdentified(const void*id,const float*p,unsigned nv,unsigned stride,const unsigned short*indices,unsigned nt,unsigned indexStride,unsigned v,const HWModernTriangleSurface*s,unsigned surfaceStride,const float m[16],const float color[3],unsigned mask,unsigned long long owner,unsigned long long part){unsigned i,j;(void)id;(void)indices;(void)indexStride;(void)surfaceStride;(void)m;(void)color;assert(owner==123&&part>0);assert(mask==1&&nv==nt*3&&v);variant=v;
 for(i=0;i<nv;i++){const float*at=(const float*)((const char*)p+i*stride);assert(at[0]>=-.001f&&at[0]<=100.001f&&at[1]>=-.001f&&at[1]<=100.001f&&at[2]>.02f);assert(fabsf(s[i/3].uv[(i%3)*2]-at[0]/100)<.00001f);assert(fabsf(s[i/3].uv[(i%3)*2+1]-(1-at[1]/100))<.00001f);}
 for(i=0;i<nt;i++){assert(s[i].materialIdentity==&identities[which]&&s[i].opacity==1);for(j=0;j<6;j++)assert(s[i].uv[j]>=-.00001f&&s[i].uv[j]<=1.00001f);}
 ++submits;}
void hwglCaptureEngineIdle(float b){assert(b<0);}
void hwglSurfaceMaterial(const void*id){assert(id==&identities[which]);}
void hwglSurfaceEmission(float b){assert(b==0);}
void hwglGetFloatv(GLenum e,GLfloat*p){unsigned i;assert(e==GL_MODELVIEW_MATRIX);for(i=0;i<16;i++)p[i]=i%5==0?1:0;}
void hwglPushAttrib(GLbitfield b){(void)b;++pushes;}
void hwglPopAttrib(void){assert(pushes);--pushes;}
void hwglEnable(GLenum e){(void)e;}
void hwglDisable(GLenum e){(void)e;}
void hwglDepthFunc(GLenum e){assert(e==GL_LEQUAL);}
void hwglDepthMask(GLboolean b){assert(!b);}
void hwglBlendFunc(GLenum a,GLenum b){assert(a==GL_SRC_ALPHA&&b==GL_ONE_MINUS_SRC_ALPHA);}
void hwglAlphaFunc(GLenum a,GLclampf b){assert(a==GL_GREATER&&b>0);}
void hwglColor4ub(GLubyte a,GLubyte b,GLubyte c,GLubyte d){assert(a==255&&b==255&&c==255&&d==255);}
void hwglBegin(GLenum e){assert(pushes&&e==GL_TRIANGLES);++draws;}
void hwglEnd(void){}
static float currentU,currentV;
void hwglTexCoord2f(GLfloat u,GLfloat v){assert(u>=-.00001f&&u<=1.00001f&&v>=-.00001f&&v<=1.00001f);currentU=u;currentV=v;}
void hwglNormal3fv(const GLfloat*p){assert(p[2]>.99f);}
void hwglVertex3fv(const GLfloat*p){assert(p[2]>.02f);assert(fabsf(currentU-p[0]/100)<.00001f&&fabsf(currentV-(1-p[1]/100))<.00001f);++vertices;}
int main(void){meshdata mesh={0};polygonobject*o=&mesh.object[0];vertexentry p[3]={0};normalentry normals[3]={0};HwHodVertex v[3]={0};unsigned short indices[]={0,1,2};HwRmHodPart part={0};unsigned i,firstVariant;
 {HWBadgeImageRect tall=hwBadgeFitSquare(4,4,146,326),wide=hwBadgeFitSquare(10,20,330,120),square=hwBadgeFitSquare(5,9,105,109);assert(tall.x0==4&&tall.x1==146&&tall.y0==94&&tall.y1==236);assert(wide.x0==120&&wide.x1==220&&wide.y0==20&&wide.y1==120);assert(square.x0==5&&square.y0==9&&square.x1==105&&square.y1==109);}
 memset(rgba,255,sizeof(rgba));meshBadgeSetMotionOwner(123);mesh.nPolygonObjects=1;o->nVertices=3;o->pVertexList=p;o->pNormalList=normals;
 for(i=0;i<3;i++){float x=i==1?2:-1,y=i==2?2:-1;p[i].x=x*100;p[i].y=y*100;p[i].iVertexNormal=i;normals[i].z=1;v[i].badgeUv[0]=x;v[i].badgeUv[1]=y;}
 part.vertexMask=16;part.vertexCount=3;part.vertices=v;part.indexCount=3;part.indices=indices;
 meshBadgeRegister(o,&part,0,1);meshBadgeDraw(o,0,1);assert(!submits);meshBadgeRegister(o,&part,1,1);meshBadgeDraw(o,0,1);assert(submits==1&&uploads==1&&draws==1&&vertices>=3&&!pushes);firstVariant=variant;
 meshBadgeDraw(o,0,1);assert(submits==2&&uploads==1);which=1;meshBadgeDraw(o,1,0);assert(submits==3&&uploads==2&&variant!=firstVariant);meshBadgeForget(&mesh);meshBadgeDraw(o,0,1);assert(submits==3);
 meshBadgeShutdown();assert(deleted==2&&unregistered==2);meshBadgeShutdown();assert(deleted==2);
 puts("Badge presentation: full-image selector fit, upright raster/DXR UVs, authored UV clipping, material replacement and cleanup passed.");return 0;}
