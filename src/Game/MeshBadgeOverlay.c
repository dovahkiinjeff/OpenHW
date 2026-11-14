#include "Mesh.h"
#include "MeshBadgeOverlay.h"
#include "RemasteredShipHod.h"
#include "ModernGraphics.h"
#include "../Modern/BadgeLibrary.h"
#include "glinc.h"
#include "texreg.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef struct BadgeVertex {float p[3],n[3],uv[2];} BadgeVertex;
typedef struct BadgeObject {struct BadgeObject *next;polygonobject *owner;BadgeVertex *vertices;unsigned short *indices;HWModernTriangleSurface *surfaces;unsigned count;uintptr_t serial;} BadgeObject;
typedef struct BadgeTexture {struct BadgeTexture *next;const void *identity;unsigned texture,variant;} BadgeTexture;
static unsigned nextVariant=1;
static uintptr_t nextGeometry=1;
static unsigned long long motionOwner;
void meshBadgeSetMotionOwner(unsigned long long owner){motionOwner=owner;}
static BadgeObject *objects;static BadgeTexture *textures;
static BadgeVertex blend(BadgeVertex a,BadgeVertex b,float t){BadgeVertex r;int i;for(i=0;i<3;i++){r.p[i]=a.p[i]+(b.p[i]-a.p[i])*t;r.n[i]=a.n[i]+(b.n[i]-a.n[i])*t;}for(i=0;i<2;i++)r.uv[i]=a.uv[i]+(b.uv[i]-a.uv[i])*t;return r;}
static unsigned clip(BadgeVertex *in,unsigned count,BadgeVertex *out,unsigned axis,float boundary,int greater){unsigned i,n=0;for(i=0;i<count;i++){
 BadgeVertex a=in[i],b=in[(i+1)%count];int ai=greater?a.uv[axis]>=boundary:a.uv[axis]<=boundary,bi=greater?b.uv[axis]>=boundary:b.uv[axis]<=boundary;
 if(ai)out[n++]=a;if(ai!=bi){float d=b.uv[axis]-a.uv[axis];if(fabsf(d)>.0000001f)out[n++]=blend(a,b,(boundary-a.uv[axis])/d);}
 }return n;}
void meshBadgeRegister(void *owner,const void *source,int badge,float scale){polygonobject *object=owner;const HwRmHodPart *part=source;BadgeObject *entry;unsigned i,j;
 if(!badge||!object||!part||!(part->vertexMask&16)||part->vertexCount!=(unsigned)object->nVertices)return;
 entry=calloc(1,sizeof(*entry));if(!entry)return;entry->owner=object;entry->serial=nextGeometry++;
 for(i=0;i<part->indexCount;i+=3){BadgeVertex a[12],b[12];unsigned n=3,k;
  for(j=0;j<3;j++){unsigned id=part->indices[i+j];const HwHodVertex*v=&part->vertices[id];normalentry *normal=&object->pNormalList[object->pVertexList[id].iVertexNormal];
   const float*p=&object->pVertexList[id].x,*nv=&normal->x;float length=sqrtf(nv[0]*nv[0]+nv[1]*nv[1]+nv[2]*nv[2]);for(k=0;k<3;k++){a[j].n[k]=length>.000001f?nv[k]/length:0;a[j].p[k]=p[k]+a[j].n[k]*fmaxf(.03f,.025f*scale);}memcpy(a[j].uv,v->badgeUv,2*sizeof(float));
  }
  {float area=(a[1].uv[0]-a[0].uv[0])*(a[2].uv[1]-a[0].uv[1])-(a[1].uv[1]-a[0].uv[1])*(a[2].uv[0]-a[0].uv[0]);if(!isfinite(area)||fabsf(area)<.0000001f)continue;}
  for(k=0;k<4&&n;k++){n=clip(a,n,b,k/2,(k%2)?1:0,(k%2)==0);memcpy(a,b,n*sizeof(*a));}
  for(j=2;j<n;j++){BadgeVertex *grown;if(entry->count+3>65535)break;grown=realloc(entry->vertices,(entry->count+3)*sizeof(*grown));if(!grown)break;entry->vertices=grown;entry->vertices[entry->count++]=a[0];entry->vertices[entry->count++]=a[j-1];entry->vertices[entry->count++]=a[j];}
 }
 if(!entry->count){free(entry);return;}
 entry->indices=malloc(entry->count*sizeof(*entry->indices));entry->surfaces=calloc(entry->count/3,sizeof(*entry->surfaces));if(!entry->indices||!entry->surfaces){free(entry->indices);free(entry->surfaces);free(entry->vertices);free(entry);return;}
 for(i=0;i<entry->count;i++)entry->indices[i]=(unsigned short)i;
 for(i=0;i<entry->count/3;i++){HWModernTriangleSurface*s=&entry->surfaces[i];s->baseColor[0]=s->baseColor[1]=s->baseColor[2]=s->opacity=1;for(j=0;j<3;j++)memcpy(s->uv+j*2,entry->vertices[i*3+j].uv,2*sizeof(float));}
 entry->next=objects;objects=entry;
}
void meshBadgeForget(void *source){meshdata*mesh=source;BadgeObject **p=&objects;while(*p){BadgeObject *e=*p;unsigned i;int owned=0;for(i=0;i<(unsigned)mesh->nPolygonObjects;i++)if(e->owner==&mesh->object[i])owned=1;if(!owned){p=&e->next;continue;}*p=e->next;free(e->vertices);free(e->indices);free(e->surfaces);free(e);}}
void meshBadgeShutdown(void){while(objects){BadgeObject *e=objects;objects=e->next;free(e->vertices);free(e->indices);free(e->surfaces);free(e);}while(textures){BadgeTexture*t=textures;textures=t->next;hwModernGraphicsUnregisterSurfaceTexture(t->identity);trRGBTextureDelete(t->texture);free(t);}motionOwner=0;}
void meshBadgeDraw(void *owner,unsigned player,int local){BadgeObject *e;BadgeTexture *t;const void *identity=NULL;unsigned w=0,h=0,i;const unsigned char*rgba;float matrix[16],white[3]={1,1,1};
 for(e=objects;e&&e->owner!=owner;e=e->next);if(!e)return;rgba=hwBadgePlayerPixels(player,local,&w,&h,&identity);if(!rgba||!identity||!w||!h)return;
 for(t=textures;t&&t->identity!=identity;t=t->next);if(!t){unsigned count=w*h,*zero=calloc(count,sizeof(unsigned)),*normal=malloc(count*sizeof(unsigned)),*orm=malloc(count*sizeof(unsigned));char key[96];if(!zero||!normal||!orm){free(zero);free(normal);free(orm);return;}
  t=calloc(1,sizeof(*t));if(!t){free(zero);free(normal);free(orm);return;}t->identity=identity;t->variant=nextVariant++;t->texture=trRGBTextureCreate((color*)rgba,w,h,TRUE);for(i=0;i<count;i++){normal[i]=0xffff8080u;orm[i]=0xff8080ffu;}
  snprintf(key,sizeof(key),"badges/rmnative/%p|rm-material",identity);hwModernGraphicsRegisterSurfaceTextureNamed(identity,key,w,h,(const unsigned*)rgba,zero,normal,orm);free(zero);free(normal);free(orm);t->next=textures;textures=t;
 }
 for(i=0;i<e->count/3;i++)e->surfaces[i].materialIdentity=identity;
 if(hwModernGraphicsIsRaytracingSceneOpen()){glGetFloatv(GL_MODELVIEW_MATRIX,matrix);hwModernGraphicsSubmitTriangleGeometryMaskedIdentified((const void*)(e->serial*16+1),e->vertices[0].p,e->count,sizeof(BadgeVertex),e->indices,e->count/3,3*sizeof(unsigned short),t->variant,e->surfaces,sizeof(*e->surfaces),matrix,white,1,motionOwner,(unsigned long long)e->serial);}
 glPushAttrib(GL_ALL_ATTRIB_BITS);
#ifdef HW_ENABLE_D3D12_BACKEND
 hwglCaptureEngineIdle(-1.0f);hwglSurfaceMaterial(identity);hwglSurfaceEmission(0);
#endif
 glEnable(GL_TEXTURE_2D);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.05f);trRGBTextureMakeCurrent(t->texture);glColor4ub(255,255,255,255);glBegin(GL_TRIANGLES);
 for(i=0;i<e->count;i++){BadgeVertex*v=&e->vertices[i];glTexCoord2f(v->uv[0],v->uv[1]);glNormal3fv(v->n);glVertex3fv(v->p);}glEnd();glPopAttrib();
}
