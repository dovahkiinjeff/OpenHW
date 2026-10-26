/* In-game authoring workspace; source HODs and gameplay static data are never
   mutated. Projects use portable, external assets under Modding/Ships. */
#include "ShipAuthoring.h"
#include "ShipAuthoringObj.h"
#include "stb_image.h"
#include <windows.h>
#include <commdlg.h>
static const char *speKinds[]={"JOINT","NAV LIGHT","DOCK","SALVAGE","REPAIR","WEAPON","TURRET","ENGINE GLOW"};
static struct {
 ShipAuthorDocument doc,history[20],clipboard;int historyAt,historyCount;
 HwAuthoringObj obj;int selected,scroll,edit,dirty,discard,material,kind,dropdown,dropScroll;
 char input[128],loadedName[48],status[180];MpeIdentity sources[MPE_MAX_IDENTITIES];int sourceCount,source;
 GLuint textures[SHIP_AUTHOR_MATERIALS];float extent;
 hmatrix rest[256];meshdata *mesh;
 float rotateTangentX,rotateTangentY,rotatePixels;
} spe;
static rectangle speButtons[16],speFields[14],speSource,speMaterial,speTextures[3],speKind;
static void speSubsystemsClear(void);
static void speSubsystemsCatalog(void);
static void speSubsystemsDraw(void);
static void speSubsystemsPicker(void);
static int speSubsystemsProcess(udword event,int x,int y);
static void speSubsystemsOpen(void);
static int speSubsystemsSaveDefinition(void);
static void speDrawRig(void);
static void speFileReset(void);
static int speFilePending(void);
static void speTreeReset(void);
static void speSubsystemsImport(void);
static int spePickGeometry(int x,int y);
static void speHighlightPart(int object,const float* matrix,int owner);
extern void meshRenderAuthoringPart(meshdata*,sdword);
extern void meshResolveModernSurfaceTextures(void);
static void speDrawObj(void);
static float *speValue(ShipAuthorPoint*p,int field){return field<3?&p->position[field]:field<6?&p->rotation[field-3]:field<9?&p->color[field-6]:&p->size;}
static void speStatus(const char*s){snprintf(spe.status,sizeof(spe.status),"%s",s);}
static ShipAuthorPoint *spePoint(void){return spe.selected>=0&&(unsigned)spe.selected<spe.doc.pointCount?&spe.doc.points[spe.selected]:NULL;}
static void speAutoSync(void);
static void speCheckpoint(void){if(spe.historyAt<19)spe.historyAt++;else memmove(spe.history,spe.history+1,19*sizeof(spe.history[0]));spe.history[spe.historyAt]=spe.doc;spe.historyCount=spe.historyAt+1;spe.dirty=1;spe.discard=0;speAutoSync();}
static void speReleaseTextures(void){int i;for(i=0;i<SHIP_AUTHOR_MATERIALS;i++){if(spe.textures[i])glDeleteTextures(1,&spe.textures[i]);spe.textures[i]=0;hwModernGraphicsUnregisterSurfaceTexture(&spe.doc.materials[i]);}}
static void speClose(void){mpeReleaseCamera();speFileReset();speTreeReset();speSubsystemsClear();speReleaseTextures();hwAuthoringObjFree(&spe.obj);memset(&spe,0,sizeof(spe));spe.selected=spe.edit=-1;}
static int speReadFile(const char*path,void**bytes,size_t*size){FILE*f;long n;*bytes=NULL;*size=0;f=fopen(path,"rb");if(!f)return 0;fseek(f,0,SEEK_END);n=ftell(f);if(n<=0||n>256*1024*1024){fclose(f);return 0;}rewind(f);*bytes=malloc(n);if(!*bytes||fread(*bytes,1,n,f)!=(size_t)n){free(*bytes);*bytes=NULL;fclose(f);return 0;}fclose(f);*size=n;return 1;}
static unsigned *speImage(const char*path,unsigned*w,unsigned*h){void*bytes;size_t n;unsigned char*p=NULL;int x,y,channels;
 if(!speReadFile(path,&bytes,&n))return NULL;
 if(n>=4&&!memcmp(bytes,"DDS ",4)){void *dds=NULL;if(hwModernGraphicsDecodeDds(bytes,(unsigned)n,w,h,&dds)){size_t count=(size_t)*w * *h;p=(unsigned char*)malloc(count*4);if(p)memcpy(p,dds,count*4);hwModernGraphicsFreeDecodedDds(dds);}}
 else {unsigned char*decoded=stbi_load_from_memory(bytes,(int)n,&x,&y,&channels,4);if(decoded&&x>0&&y>0&&x<=8192&&y<=8192){*w=x;*h=y;p=(unsigned char*)malloc((size_t)x*y*4);if(p)memcpy(p,decoded,(size_t)x*y*4);}stbi_image_free(decoded);}free(bytes);return (unsigned*)p;
}
static void speLoadTextures(void){unsigned i;char path[1024];speReleaseTextures();
 for(i=0;i<spe.doc.materialCount;i++){unsigned *maps[3]={NULL,NULL,NULL},w[3]={0},h[3]={0};int k;
  for(k=0;k<3;k++)if(spe.doc.materials[i].texture[k][0]&&shipAuthorProjectPath(spe.loadedName,spe.doc.materials[i].texture[k],path,sizeof(path),0))maps[k]=speImage(path,&w[k],&h[k]);
  if(maps[0]){glGenTextures(1,&spe.textures[i]);glBindTexture(GL_TEXTURE_2D,spe.textures[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w[0],h[0],0,GL_RGBA,GL_UNSIGNED_BYTE,maps[0]);
   hwModernGraphicsRegisterSurfaceTexture(&spe.doc.materials[i],w[0],h[0],maps[0],maps[2]&&w[2]==w[0]&&h[2]==h[0]?maps[2]:NULL,maps[1]&&w[1]==w[0]&&h[1]==h[0]?maps[1]:NULL,NULL);}
  for(k=0;k<3;k++)free(maps[k]);
 }
}
static void speLayout(void){int i,bw=(MAIN_WindowWidth-uiScaleSize(32))/16,top=uiScaleSize(108),step;mpeLayout();step=muiClamp((mpeInspector.y1-top-uiScaleSize(55))/19,uiScaleSize(20),uiScaleSize(30));
 speSource=muiRect(mpeList.x0,top,mpeList.x1,top+uiScaleSize(40));
 for(i=0;i<16;i++)speButtons[i]=muiRect(uiScaleSize(16)+i*bw,uiScaleSize(57),uiScaleSize(16)+(i+1)*bw-4,uiScaleSize(94));
 for(i=0;i<14;i++)speFields[i]=muiRect(mpeInspector.x0+6,top+uiScaleSize(32)+i*step,mpeInspector.x1-6,top+uiScaleSize(32)+(i+1)*step-3);
 speKind=muiRect(mpeList.x0,mpeList.y1+uiScaleSize(8),mpeList.x1,mpeList.y1+uiScaleSize(46));
 speMaterial=muiRect(mpeInspector.x0+6,top+uiScaleSize(32)+14*step,mpeInspector.x1-6,top+uiScaleSize(32)+15*step-3);
 for(i=0;i<3;i++)speTextures[i]=muiRect(mpeInspector.x0+6,top+uiScaleSize(32)+(15+i)*step,mpeInspector.x1-6,top+uiScaleSize(32)+(16+i)*step-3);
}
static void speFrame(void){float extent=fmaxf(spe.extent,1),zoom=fminf((mpeView.x1-mpeView.x0)*.42f/extent,(mpeView.y1-mpeView.y0)*.42f/extent);mpe.zoom=muiClampF(zoom,.001f,4);mpe.zoomTarget=1/mpe.zoom;mpe.panX=mpe.panY=0;}
static void speJointName(const polygonobject*o,char name[64]){int i,j,duplicate=0;const char*s=o->pName;
 if(s)for(i=0;i<spe.mesh->nPolygonObjects;i++)if(&spe.mesh->object[i]!=o&&spe.mesh->object[i].pName&&!strcmp(s,spe.mesh->object[i].pName))duplicate=1;
 if(!s||!*s||strlen(s)>48||duplicate){snprintf(name,64,"joint_%u",o->iObject);return;}
 snprintf(name,64,"%s",s);for(j=0;name[j];j++)if((unsigned char)name[j]<=32||name[j]=='/')name[j]='_';
}
static void speRestPose(const hmatrix*matrix,float position[3],float rotation[3]){const float*v=(const float*)matrix;float r[9];int axes[3]={1,2,0},a,b;
 position[0]=v[13];position[1]=v[14];position[2]=v[12];for(a=0;a<3;a++)for(b=0;b<3;b++)r[b*3+a]=v[axes[b]*4+axes[a]];
 rotation[1]=asinf(muiClampF(-r[2],-1,1))*57.29577951f;rotation[0]=atan2f(r[5],r[8])*57.29577951f;rotation[2]=atan2f(r[1],r[0])*57.29577951f;
}
static void speImportJoints(void){int i;meshdata*m=spe.mesh;if(!m)return;
 for(i=0;i<m->nPolygonObjects&&i<256;i++){polygonobject*o=&m->object[i];ShipAuthorPoint*p;
  spe.rest[i]=o->localMatrix;
  if(spe.doc.pointCount>=SHIP_AUTHOR_POINTS){speStatus("MODEL EXCEEDS EDITABLE POINT LIMIT");break;}
  p=&spe.doc.points[spe.doc.pointCount++];p->enabled=1;p->kind=SAP_JOINT;p->object=i;speJointName(o,p->name);if(o->pMother)speJointName(o->pMother,p->parent);else snprintf(p->parent,64,"-");
  speRestPose(&o->localMatrix,p->position,p->rotation);
  p->color[0]=p->color[1]=p->color[2]=.65f;p->size=10;
 }
 speSubsystemsImport();
}
static void speLoadSource(int importPoints){int i;char path[1024],error[200];void*data;size_t n;
 speSubsystemsClear();
 if(spe.mesh)for(i=0;i<spe.mesh->nPolygonObjects&&i<256;i++)spe.mesh->object[i].localMatrix=spe.rest[i];
 hwAuthoringObjFree(&spe.obj);spe.mesh=NULL;spe.extent=1;
 if(spe.doc.model[0]){if(!shipAuthorProjectPath(spe.loadedName,spe.doc.model,path,sizeof(path),0)||!speReadFile(path,&data,&n)){speStatus("MODEL FILE MISSING / ORIGINAL ASSETS UNCHANGED");return;}
  if(!hwAuthoringObjRead(data,n,&spe.obj,error,sizeof(error)))speStatus(error);free(data);
  for(i=0;i<(int)spe.obj.vertexCount;i++){int k;for(k=0;k<3;k++)spe.extent=fmaxf(spe.extent,fabsf(spe.obj.vertices[i].position[k])*spe.doc.scale);}
 }else if(paradeLayoutIdentityValid(spe.doc.race,spe.doc.type)){
  spe.mesh=mpeMesh(spe.doc.race,spe.doc.type);
  if(spe.mesh){int material,failed=0;
   for(material=0;material<spe.mesh->nLocalMaterials+spe.mesh->nPublicMaterials;material++){
    materialentry*m=&spe.mesh->localMaterial[material];
    if(m->bTexturesRegistered && m->texture && ((trhandle*)m->texture)[0]!=TR_Invalid)
     if(!trPreparePreviewTexture(((trhandle*)m->texture)[0]))failed++;
   }
   meshResolveModernSurfaceTextures();
   if(failed)speStatus("SOME PREVIEW TEXTURES COULD NOT BE LOADED / SOURCE FILES UNCHANGED");
   for(i=0;i<spe.mesh->nPolygonObjects&&i<256;i++){int j,k;spe.rest[i]=spe.mesh->object[i].localMatrix;
    for(j=0;j<spe.mesh->object[i].nVertices;j++)for(k=0;k<3;k++)spe.extent=fmaxf(spe.extent,fabsf(((float*)&spe.mesh->object[i].pVertexList[j])[k]));}
   if(importPoints)speImportJoints();}else speStatus("THIS SOURCE MODEL IS NOT AVAILABLE IN THE LOCAL INSTALL");
 }
 speLoadTextures();speFrame();
}
static void speTemplate(int source){if(source<0||source>=spe.sourceCount)return;speTreeReset();spe.source=source;memset(&spe.doc,0,sizeof(spe.doc));spe.doc.race=spe.sources[source].race;spe.doc.type=spe.sources[source].type;spe.doc.scale=1;
 snprintf(spe.doc.name,48,"%s_%s",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));snprintf(spe.loadedName,48,"%s",spe.doc.name);spe.selected=-1;spe.scroll=0;speLoadSource(1);spe.historyAt=0;spe.historyCount=1;spe.history[0]=spe.doc;spe.dirty=0;
}
static void speBegin(void){int r,t;speClose();mpeReleaseCamera();mpeFreeMeshes();mpe.stage=4;mpe.yaw=0;mpe.pitch=-35;mpe.drag=0;mpe.zoom=.1f;mpe.zoomTarget=10;mpe.cameraTick=SDL_GetTicks();spe.selected=spe.edit=-1;spe.kind=SAP_NAV;
 for(r=0;r<NUM_RACES;r++)for(t=FirstShipTypeOfRace[r];t<=LastShipTypeOfRace[r];t++)if(paradeLayoutIdentityValid(r,t)){spe.sources[spe.sourceCount].race=r;spe.sources[spe.sourceCount++].type=t;}
 speLayout();speTemplate(0);speSubsystemsCatalog();speStatus("SHIP AUTHORING / EXTERNAL PROJECTS / SOURCE HODS ARE READ ONLY");
}
static void speApplyJoints(int group){int i,j;meshdata*m=spe.mesh;if(!m)return;
 for(i=0;i<m->nPolygonObjects&&i<256;i++){polygonobject*o=&m->object[i];o->localMatrix=spe.rest[i];if(!o->pName)continue;
  for(j=0;j<(int)spe.doc.pointCount;j++){ShipAuthorPoint*p=&spe.doc.points[j];if(p->kind==SAP_JOINT&&p->enabled&&p->object==i&&p->clone==group){
   float *v=(float*)&o->localMatrix;float originalPosition[3],originalRotation[3];int col,k,changed=0;int axes[3]={1,2,0};
   speRestPose(&spe.rest[i],originalPosition,originalRotation);for(k=0;k<3;k++)if(fabsf(originalRotation[k]-p->rotation[k])>.00001f)changed=1;
   // Keep untouched source joint bases bit-for-bit: don't rebuild the whole
   // imported turret rig through Euler angles just to display the model.
   if(changed)for(col=0;col<3;col++){float a[3]={0},b[3],scale=0;for(k=0;k<3;k++)scale+=((float*)&spe.rest[i])[axes[col]*4+axes[k]]*((float*)&spe.rest[i])[axes[col]*4+axes[k]];a[col]=sqrtf(scale);paradeLayoutRotate(p->rotation,a,b);for(k=0;k<3;k++)v[axes[col]*4+axes[k]]=b[k];}
   v[12]=p->position[2];v[13]=p->position[0];v[14]=p->position[1];break;
  }}
 }
}
static void speRender(void){GLint viewport[4],mode;float fx,fy;hmatrix savedCamera=rndCameraMatrix,savedProjection=rndProjectionMatrix;ParadeShipLayout origin={0};
 static const GLfloat light[4]={-1,-2,3,0},diffuse[4]={.9f,.9f,.9f,1},ambient[4]={.25f,.25f,.25f,1};
 glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_MATRIX_MODE,&mode);fx=(float)viewport[2]/MAIN_WindowWidth;fy=(float)viewport[3]/MAIN_WindowHeight;
 primModeClear2();glPushAttrib(GL_ALL_ATTRIB_BITS);glViewport(mpeView.x0*fx,viewport[3]-mpeView.y1*fy,(mpeView.x1-mpeView.x0)*fx,(mpeView.y1-mpeView.y0)*fy);
 glEnable(GL_SCISSOR_TEST);glScissor(mpeView.x0*fx,viewport[3]-mpeView.y1*fy,(mpeView.x1-mpeView.x0)*fx,(mpeView.y1-mpeView.y0)*fy);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_FOG);glDisable(GL_CLIP_PLANE0);glDisable(GL_BLEND);
 rndTextureEnvironment(RTE_Modulate);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(-(mpeView.x1-mpeView.x0)*.5f/mpe.zoom,(mpeView.x1-mpeView.x0)*.5f/mpe.zoom,-(mpeView.y1-mpeView.y0)*.5f/mpe.zoom,(mpeView.y1-mpeView.y0)*.5f/mpe.zoom,-2000000,2000000);
 glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glTranslatef(mpe.panX/mpe.zoom,-mpe.panY/mpe.zoom,0);glLightfv(GL_LIGHT0,GL_POSITION,light);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0,GL_AMBIENT,ambient);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);rndLightingEnable(TRUE);
 glRotatef(mpe.pitch,1,0,0);glRotatef(mpe.yaw,0,0,1);glGetFloatv(GL_MODELVIEW_MATRIX,(float*)&rndCameraMatrix);glGetFloatv(GL_PROJECTION_MATRIX,(float*)&rndProjectionMatrix);
 if(spe.mesh){rndTextureEnable(TRUE);speDrawRig();}
 else if(spe.obj.vertices)speDrawObj();
 speSubsystemsDraw();
 hwglSurfaceMaterial(NULL);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);glPopAttrib();glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);rndCameraMatrix=savedCamera;rndProjectionMatrix=savedProjection;rndLightingEnable(FALSE);rndTextureEnable(FALSE);primModeSet2();
}
static void speWorldPoint(unsigned index,float out[3],unsigned depth){float m[16];shipAuthorWorld(&spe.doc,index,m);memcpy(out,m+12,3*sizeof(float));
}
static void speParentVector(unsigned index,const float input[3],float out[3],unsigned depth){unsigned i;ShipAuthorPoint*p=&spe.doc.points[index];memcpy(out,input,12);if(depth>spe.doc.pointCount)return;
 for(i=0;i<spe.doc.pointCount;i++)if(i!=index&&!strcmp(p->parent,spe.doc.points[i].name)){float local[3];paradeLayoutRotate(spe.doc.points[i].rotation,input,local);speParentVector(i,local,out,depth+1);break;}
}
/* Render each part once from the authored hierarchy. The cached source mesh's
   links never change; copied turrets can be reparented to new helper joints. */
static void speObjectWorld(int object,int group,float out[16],int depth){unsigned i;polygonobject*o=&spe.mesh->object[object];float local[16],parent[16];int a,b,k,axes[4]={1,2,0,3};
 for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].object==object&&spe.doc.points[i].clone==group){shipAuthorWorld(&spe.doc,i,out);return;}
 for(a=0;a<4;a++)for(b=0;b<4;b++)local[a*4+b]=((float*)&spe.rest[object])[axes[a]*4+axes[b]];
 if(!o->pMother||depth>=256){memcpy(out,local,sizeof(local));return;}speObjectWorld((int)(o->pMother-spe.mesh->object),group,parent,depth+1);
 for(a=0;a<4;a++)for(b=0;b<4;b++){out[a*4+b]=0;for(k=0;k<4;k++)out[a*4+b]+=parent[k*4+b]*local[a*4+k];}
}
static int speVisible(int point){unsigned depth=0;while(point>=0&&depth++<spe.doc.pointCount){if(!spe.doc.points[point].enabled)return 0;point=shipAuthorParent(&spe.doc,point);}return 1;}
static void speDrawPart(int object,const float*matrix){static const GLfloat basis[16]={0,0,1,0,1,0,0,0,0,1,0,0,0,0,0,1};
 if(!spe.mesh->object[object].nPolygons)return;
 glPushMatrix();glMultMatrixf(matrix);glMultMatrixf(basis);meshRenderAuthoringPart(spe.mesh,object);glPopMatrix();
}
static int speObjectOwner(int object,int group){unsigned i,depth=0;while(object>=0&&depth++<256){polygonobject*o=&spe.mesh->object[object];for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].object==object&&spe.doc.points[i].clone==group)return i;object=o->pMother?(int)(o->pMother-spe.mesh->object):-1;}return -1;}
static void speDrawRig(void){int object,owner;unsigned i,j;float matrix[16];
 for(object=0;object<spe.mesh->nPolygonObjects&&object<256;object++){
  owner=speObjectOwner(object,0);if(owner>=0&&!speVisible(owner))continue;speObjectWorld(object,0,matrix,0);speDrawPart(object,matrix);speHighlightPart(object,matrix,owner);
 }
 /* Older saved projects stored joints without their geometry children. Follow
    source children for those files too, while honoring authored parent edits. */
 for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].clone){int group=spe.doc.points[i].clone;for(j=0;j<i;j++)if(spe.doc.points[j].clone==group)break;if(j<i)continue;
  for(object=0;object<spe.mesh->nPolygonObjects&&object<256;object++){owner=speObjectOwner(object,group);if(owner<0||!speVisible(owner))continue;speObjectWorld(object,group,matrix,0);speDrawPart(object,matrix);speHighlightPart(object,matrix,owner);}
 }
}
static void speDrawObj(void){unsigned point,i,mat;glEnable(GL_LIGHTING);glDisable(GL_CULL_FACE);glColor4f(1,1,1,1);
 for(point=0;point<spe.doc.pointCount;point++){ShipAuthorPoint*p=&spe.doc.points[point];float world[3],matrix[16]={0};int j,k;if(!speVisible(point)||p->object!=0)continue;
  speWorldPoint(point,world,0);matrix[15]=1;for(j=0;j<3;j++){float a[3]={0},b[3],v[3];a[j]=1;paradeLayoutRotate(p->rotation,a,b);speParentVector(point,b,v,0);for(k=0;k<3;k++)matrix[j*4+k]=v[k];}
  glPushMatrix();glTranslatef(world[0],world[1],world[2]);glMultMatrixf(matrix);
  for(mat=0;mat<spe.obj.materialCount;mat++){if(spe.textures[mat]){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,spe.textures[mat]);}else glDisable(GL_TEXTURE_2D);hwglSurfaceMaterial(&spe.doc.materials[mat]);glBegin(GL_TRIANGLES);
   for(i=0;i<spe.obj.vertexCount;i++){const HwAuthoringVertex*v=&spe.obj.vertices[i];if(v->material!=mat)continue;glNormal3f(v->normal[0],-v->normal[2],v->normal[1]);glTexCoord2f(v->uv[0],1-v->uv[1]);glVertex3f(v->position[0]*spe.doc.scale,-v->position[2]*spe.doc.scale,v->position[1]*spe.doc.scale);}glEnd();}
  glPopMatrix();
 }
}
static void speAxis(unsigned index,int axis,float length,float out[3]){float local[3]={0};local[axis]=length;if(mpe.rotateMode){float rotated[3];paradeLayoutRotate(spe.doc.points[index].rotation,local,rotated);memcpy(local,rotated,12);}speParentVector(index,local,out,0);}
static void speRingPoint(int axis,float angle,float*x,float*y){float m[16],local[3]={0},world[3],radius=uiScaleSize(72)/mpe.zoom;int a;shipAuthorWorld(&spe.doc,spe.selected,m);local[(axis+1)%3]=cosf(angle)*radius;local[(axis+2)%3]=sinf(angle)*radius;for(a=0;a<3;a++)world[a]=m[12+a]+m[a]*local[0]+m[4+a]*local[1]+m[8+a]*local[2];mpeProject(world,x,y);}
static int speRings(int draw,int x,int y){int axis,segment,bestAxis=-1;float best=uiScaleSize(10)*uiScaleSize(10);
 for(axis=0;axis<3;axis++){float px,py;speRingPoint(axis,0,&px,&py);for(segment=1;segment<=64;segment++){float qx,qy,dx,dy,length,t,ex,ey;speRingPoint(axis,segment*.0981747704f,&qx,&qy);dx=qx-px;dy=qy-py;length=dx*dx+dy*dy;
  if(draw)primLineThick2(px,py,qx,qy,uiScaleSize(2),mpeAxisColor[axis]);else if(length>.01f){t=muiClampF(((x-px)*dx+(y-py)*dy)/length,0,1);ex=x-px-t*dx;ey=y-py-t*dy;if(ex*ex+ey*ey<best){best=ex*ex+ey*ey;bestAxis=axis;spe.rotateTangentX=dx/sqrtf(length);spe.rotateTangentY=dy/sqrtf(length);spe.rotatePixels=fmaxf(sqrtf(length)/.0981747704f,uiScaleSize(18));}}px=qx;py=qy;
 }}return bestAxis;
}
#include "ShipEditorTree.inc"
#include "ShipEditorFile.inc"
#include "ShipEditorSubsystem.inc"
#include "ShipEditorEvents.inc"
static void speDraw(void){static const char*buttons[]={"BACK","SAVE","LOAD","UNDO","REDO","MOVE / ROTATE","FRAME","ADD POINT","REMOVE","IMPORT OBJ","NEW / SAVE AS","SHIP FILE","SUBSYSTEM","EVENT FILE","SAVE MOD","APPEND TO MOD"};static const char*fields[]={"POSITION X","POSITION Y","POSITION Z","ROTATION X","ROTATION Y","ROTATION Z","RED","GREEN","BLUE","SIZE"};
 rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight),row;char text[180];int i;ShipAuthorPoint*p=spePoint();
 {Uint32 now=SDL_GetTicks();float dt=fminf((now-mpe.cameraTick)*.001f,.25f),distance=1/mpe.zoom;mpe.cameraTick=now;distance+=(mpe.zoomTarget-distance)*(1-expf(-fmaxf(1,CAMERA_WHEEL_ZOOM_RESPONSE)*dt));mpe.zoom=1/muiClampF(distance,.25f,1000);if(mpe.cameraCaptured&&!mouseRightButton())mpeReleaseCamera();}
 speLayout();muiFill(&whole,colRGB(5,12,18));muiText(uiScaleSize(20),uiScaleSize(20),muiPaper,"MODDING / SHIP EDITOR / AUTHORING PREVIEW",muiHeadingFont);for(i=0;i<16;i++)muiDrawButtonRect(&speButtons[i],i==5?(mpe.rotateMode?"ROTATE (R)":"MOVE (G)"):buttons[i],i==5&&mpe.rotateMode,muiPointIn(&speButtons[i],mouseCursorX(),mouseCursorY()));
 muiFill(&mpeList,muiSurface);muiFill(&mpeView,colRGB(9,15,24));muiFill(&mpeInspector,muiSurface);speRender();
 snprintf(text,sizeof(text),"SHIP: %s / %s v",ShipRaceToStr(spe.doc.race),ShipTypeToNiceStr(spe.doc.type));muiDrawButtonRect(&speSource,text,spe.dropdown,muiPointIn(&speSource,mouseCursorX(),mouseCursorY()));
 speTreeDraw();
 snprintf(text,sizeof(text),"ADD TYPE: %s  >",speKinds[spe.kind]);muiDrawButtonRect(&speKind,text,TRUE,muiPointIn(&speKind,mouseCursorX(),mouseCursorY()));
 for(i=0;i<10;i++){snprintf(text,sizeof(text),"%s: %.3f",fields[i],p?*speValue(p,i):0);if(spe.edit==i)snprintf(text,sizeof(text),"%s: %s",fields[i],spe.input);muiDrawButtonRect(&speFields[i],text,spe.edit==i,muiPointIn(&speFields[i],mouseCursorX(),mouseCursorY()));}
 snprintf(text,sizeof(text),"NAME: %s",spe.edit==10?spe.input:p?p->name:"-");muiDrawButtonRect(&speFields[10],text,spe.edit==10,FALSE);
 snprintf(text,sizeof(text),"PARENT: %s",spe.edit==11?spe.input:p?p->parent:"-");muiDrawButtonRect(&speFields[11],text,spe.edit==11,FALSE);
 snprintf(text,sizeof(text),"PROJECT: %s",spe.edit==12?spe.input:spe.doc.name);muiDrawButtonRect(&speFields[12],text,spe.edit==12,FALSE);
 snprintf(text,sizeof(text),"OBJ SCALE: %s",spe.edit==13?spe.input:"");if(spe.edit!=13)snprintf(text,sizeof(text),"OBJ SCALE: %.3f",spe.doc.scale);muiDrawButtonRect(&speFields[13],text,spe.edit==13,FALSE);
 snprintf(text,sizeof(text),"MATERIAL: %s >",spe.doc.materialCount?spe.doc.materials[spe.material].name:"BUILTIN / AUTHORED");muiDrawButtonRect(&speMaterial,text,TRUE,FALSE);
 for(i=0;i<3;i++)muiDrawButtonRect(&speTextures[i],i==0?"SELECT DIFFUSE TEXTURE":i==1?"SELECT NORMAL MAP":"SELECT EMISSION MAP",spe.doc.materialCount!=0,FALSE);
 for(i=0;i<(int)spe.doc.pointCount;i++){ShipAuthorPoint*q=&spe.doc.points[i];float world[3],x,y;int c;if(!q->enabled)continue;speWorldPoint(i,world,0);mpeProject(world,&x,&y);if(!muiPointIn(&mpeView,x,y))continue;c=colRGB((ubyte)(fminf(q->color[0],1)*255),(ubyte)(fminf(q->color[1],1)*255),(ubyte)(fminf(q->color[2],1)*255));primCircleSolid2(x,y,uiScaleSize(i==spe.selected?6:3),12,c);if(i==spe.selected){int axis;muiText(x+8,y+8,muiSignal,q->name,muiBodyFont);
   if(mpe.rotateMode)speRings(1,0,0);else for(axis=0;axis<3;axis++){float v[3],tx,ty;speAxis(i,axis,uiScaleSize(80)/mpe.zoom,v);v[0]+=world[0];v[1]+=world[1];v[2]+=world[2];mpeProject(v,&tx,&ty);primLineThick2(x,y,tx,ty,uiScaleSize(2),mpeAxisColor[axis]);primCircleSolid2(tx,ty,uiScaleSize(5),12,mpeAxisColor[axis]);}
  }}
 muiText(mpeView.x0+8,mpeView.y0+8,muiMuted,"RIGHT ORBIT / BOTH BUTTONS ZOOM / WHEEL ZOOM / MIDDLE PAN",muiBodyFont);muiText(mpeView.x0+8,mpeView.y0+uiScaleSize(30),muiMuted,"CTRL+C / V COPY CHAIN / CTRL+D DUPLICATE / DRAG TREE TO REPARENT",muiBodyFont);muiText(uiScaleSize(18),MAIN_WindowHeight-uiScaleSize(32),muiPaper,spe.status,muiBodyFont);
 if(spe.dropdown)for(i=spe.dropScroll;i<spe.sourceCount&&i<spe.dropScroll+12;i++){row=muiRect(speSource.x0,speSource.y1+uiScaleSize((i-spe.dropScroll)*30),mpeView.x0+uiScaleSize(200),speSource.y1+uiScaleSize((i-spe.dropScroll+1)*30));snprintf(text,sizeof(text),"%s / %s",ShipRaceToStr(spe.sources[i].race),ShipTypeToNiceStr(spe.sources[i].type));muiDrawButtonRect(&row,text,FALSE,muiPointIn(&row,mouseCursorX(),mouseCursorY()));}
 speFileDraw();
 speSubsystemsPicker();
}
static void speCommit(void){ShipAuthorDocument candidate=spe.doc;ShipAuthorPoint*p=spe.selected>=0&&(unsigned)spe.selected<candidate.pointCount?&candidate.points[spe.selected]:NULL;char*end;float value;
 if(spe.edit<0)return;
 if(spe.edit<10&&p){value=strtof(spe.input,&end);if(end==spe.input||*end){speStatus("INVALID NUMBER / NO CHANGE");spe.edit=-1;return;}*speValue(p,spe.edit)=value;}
 else if(spe.edit==10&&p){unsigned i;char old[64];snprintf(old,64,"%s",p->name);snprintf(p->name,64,"%.63s",spe.input);for(i=0;i<candidate.pointCount;i++)if(!strcmp(candidate.points[i].parent,old))snprintf(candidate.points[i].parent,64,"%s",p->name);}
 else if(spe.edit==11&&p){unsigned j;for(j=0;j<candidate.pointCount;j++)if(!strcmp(spe.input,candidate.points[j].name))break;if((j==candidate.pointCount&&strcmp(spe.input,"-"))||!shipAuthorReparent(&candidate,spe.selected,j==candidate.pointCount?-1:(int)j)){speStatus("INVALID PARENT / NO CHANGE");spe.edit=-1;return;}}
 else if(spe.edit==12)snprintf(candidate.name,48,"%.47s",spe.input);
 else if(spe.edit==13){value=strtof(spe.input,&end);if(end==spe.input||*end){spe.edit=-1;return;}candidate.scale=value;}
 if(shipAuthorDocumentValid(&candidate)){spe.doc=candidate;speCheckpoint();if(spe.edit==13)speLoadSource(0);}else speStatus("INVALID NAME, TRANSFORM OR PARENT / NO CHANGE");spe.edit=-1;
}
static int speBrowse(char*path,const char*filter){OPENFILENAMEA dialog;memset(&dialog,0,sizeof(dialog));path[0]=0;dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();dialog.lpstrFilter=filter;dialog.lpstrFile=path;dialog.nMaxFile=1024;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;return GetOpenFileNameA(&dialog)!=0;}
static void speImportObj(void){char path[1024],relative[128],error[200];void*data;size_t bytes;HwAuthoringObj candidate={0};unsigned i;
 if(!speBrowse(path,"Wavefront OBJ\0*.obj\0\0"))return;
 if(!speReadFile(path,&data,&bytes)){speStatus("COULD NOT READ OBJ");return;}
 if(!hwAuthoringObjRead(data,bytes,&candidate,error,sizeof(error))){free(data);speStatus(error);return;}free(data);
 snprintf(relative,128,"assets/model-%llu.obj",(unsigned long long)SDL_GetPerformanceCounter());
 if(!shipAuthorCopyAssets(&spe.doc,spe.loadedName)||!shipAuthorImportFile(spe.doc.name,path,relative)){hwAuthoringObjFree(&candidate);speStatus("IMPORT COPY FAILED / ORIGINAL OBJ UNCHANGED");return;}
 hwAuthoringObjFree(&candidate);speReleaseTextures();spe.doc.materialCount=0;
 snprintf(spe.loadedName,48,"%s",spe.doc.name);snprintf(spe.doc.model,128,"%s",relative);spe.doc.pointCount=0;spe.selected=-1;speLoadSource(0);
 spe.doc.materialCount=spe.obj.materialCount;memset(spe.doc.materials,0,sizeof(spe.doc.materials));for(i=0;i<spe.obj.materialCount;i++)snprintf(spe.doc.materials[i].name,64,"%s",spe.obj.materials[i]);spe.material=spe.obj.vertexCount?spe.obj.vertices[0].material:0;speCheckpoint();speFrame();speStatus("OBJ IMPORTED / Y UP, -Z FORWARD / SELECT TEXTURES BY MATERIAL / SAVE PROJECT");
 if(spe.obj.vertexCount){ShipAuthorPoint*p=&spe.doc.points[0];memset(p,0,sizeof(*p));strcpy(p->name,"OBJ_Hull");strcpy(p->parent,"-");p->kind=SAP_JOINT;p->object=0;p->enabled=1;p->size=10;p->color[0]=p->color[1]=p->color[2]=.65f;spe.doc.pointCount=1;spe.selected=0;spe.history[spe.historyAt]=spe.doc;}
}
static void speTexture(int slot){char path[1024],relative[128];const char*extension;unsigned w=0,h=0;unsigned*p;
 if(!spe.doc.materialCount){speStatus("TEXTURE SELECTION CURRENTLY TARGETS IMPORTED OBJ MATERIALS");return;}
 if(!speBrowse(path,"Images\0*.dds;*.png;*.tga;*.jpg;*.bmp\0\0"))return;
 p=speImage(path,&w,&h);if(!p){speStatus("UNSUPPORTED OR INVALID IMAGE / NO CHANGE");return;}free(p);extension=strrchr(path,'.');
 if(!extension||strlen(extension)>5){speStatus("UNSUPPORTED IMAGE EXTENSION");return;}
 snprintf(relative,128,"assets/material-%d-%d-%llu%s",spe.material,slot,(unsigned long long)SDL_GetPerformanceCounter(),extension);
 if(!shipAuthorCopyAssets(&spe.doc,spe.loadedName)||!shipAuthorImportFile(spe.doc.name,path,relative)){speStatus("TEXTURE COPY FAILED / SOURCE IMAGE UNCHANGED");return;}
 snprintf(spe.loadedName,48,"%s",spe.doc.name);snprintf(spe.doc.materials[spe.material].texture[slot],128,"%s",relative);speLoadTextures();speCheckpoint();speStatus("TEXTURE COPIED INTO PROJECT / NORMAL-EMISSION MAPS MUST MATCH DIFFUSE SIZE");
}
static void speAction(int action){ShipAuthorPoint*p;unsigned i;ShipAuthorDocument loaded;speCommit();
 switch(action){
 case 0:if(spe.dirty||speFilePending()){speStatus("SAVE PROJECT AND SHIP FILE FIRST, OR ESC TWICE TO DISCARD");return;}speClose();mpeFreeMeshes();mpe.stage=1;return;
 case 1:if(!shipAuthorDocumentValid(&spe.doc)){speStatus("INVALID PROJECT / NOTHING SAVED");return;}if(speSubsystemsSyncV2(2)==0)return;if(!shipAuthorCopyAssets(&spe.doc,spe.loadedName)||!shipAuthorSave(&spe.doc)||!speSubsystemsSaveDefinition()){speStatus("SAVE INCOMPLETE / CHECK PROJECT BACKUPS BEFORE RELOADING");return;}if(strcmp(spe.loadedName,spe.doc.name)){spe.historyAt=0;spe.historyCount=1;spe.history[0]=spe.doc;}snprintf(spe.loadedName,48,"%s",spe.doc.name);spe.dirty=0;speStatus("SAVED PROJECT AND SUBSYSTEM DECLARATIONS / NOT INSTALLED INTO GAMEPLAY");break;
 case 2:if(!shipAuthorLoad(spe.doc.name,&loaded)){speStatus("NO VALID PROJECT FOUND / NO CHANGE");return;}spe.doc=loaded;snprintf(spe.loadedName,48,"%s",spe.doc.name);spe.selected=-1;spe.material=0;speLoadSource(0);spe.historyAt=0;spe.historyCount=1;spe.history[0]=spe.doc;spe.dirty=0;speStatus("PROJECT LOADED");break;
 case 3:case 4:{int next=spe.historyAt+(action==3?-1:1);if(next>=0&&next<spe.historyCount){spe.historyAt=next;spe.doc=spe.history[next];spe.selected=-1;spe.dirty=1;speLoadSource(0);speAutoSync();}break;}
 case 5:mpe.rotateMode=!mpe.rotateMode;break;case 6:speFrame();break;
 case 7:if(spe.doc.pointCount>=SHIP_AUTHOR_POINTS){speStatus("POINT LIMIT REACHED");return;}p=&spe.doc.points[spe.doc.pointCount];memset(p,0,sizeof(*p));p->kind=spe.kind;p->enabled=1;p->object=-1;p->size=20;p->color[0]=1;p->color[1]=.75f;p->color[2]=.2f;snprintf(p->parent,64,"%s",spePoint()?spePoint()->name:"-");
  for(i=0;i<10000;i++){unsigned j;snprintf(p->name,64,"point_%u",i);for(j=0;j<spe.doc.pointCount;j++)if(!strcmp(p->name,spe.doc.points[j].name))break;if(j==spe.doc.pointCount)break;}
  spe.selected=spe.doc.pointCount++;speCheckpoint();break;
 case 8:p=spePoint();if(p){unsigned j;for(j=0;j<spe.doc.pointCount;j++)if(!strcmp(spe.doc.points[j].parent,p->name)){speStatus("REPARENT THE CHILD POINTS BEFORE REMOVING THIS POINT");return;}memmove(p,p+1,(spe.doc.pointCount-spe.selected-1)*sizeof(*p));spe.doc.pointCount--;spe.selected=-1;speCheckpoint();}break;
 case 9:speImportObj();break;
 case 10:spe.edit=12;snprintf(spe.input,sizeof(spe.input),"%.24s_%lu",spe.doc.name,(unsigned long)SDL_GetTicks());speStatus("NAME THE NEW PROJECT / ENTER THEN SAVE / ORIGINAL PROJECT IS PRESERVED");break;
 case 11:if(spe.selected<0||!shipAuthorCopySelection(&spe.doc,spe.selected,&spe.clipboard))speStatus("SELECT A VALID POINT OR RIG OBJECT FIRST");else speStatus("COPIED OBJECT AND CHILD POINTS / CTRL+V TO PASTE");break;
 case 12:if(!shipAuthorPaste(&spe.doc,&spe.clipboard,fmaxf(spe.extent*.25f,20),&spe.selected))speStatus("PASTE FAILED / RIG COPIES REQUIRE THE SAME SOURCE SHIP / POINT LIMIT REACHED");else {speCheckpoint();speSubsystemsSyncV2(0);speStatus("PASTED COMPONENT / NEW HARDPOINT DECLARATION / SAVE TO KEEP CHANGES");}break;
 }
}
static udword speProcess(smemsize ID,udword event){int x=mouseCursorX(),y=mouseCursorY(),dx=x-mpe.lastX,dy=y-mpe.lastY,i;ShipAuthorPoint*p=spePoint();speLayout();speTreeBuild();
 if(speFileProcess(ID,event,x,y))return RPR_Redraw;
 if(speSubsystemsProcess(event,x,y))return RPR_Redraw;
 if(event==RPE_KeyDown){
  if(spe.edit>=0){size_t n=strlen(spe.input);char c=0;
   if(ID==RETURNKEY){speCommit();return RPR_Redraw;}if(ID==ESCKEY){spe.edit=-1;return RPR_Redraw;}if(ID==BACKSPACEKEY){if(n)spe.input[n-1]=0;return RPR_Redraw;}
   if(ID>=SDL_SCANCODE_1&&ID<=SDL_SCANCODE_9)c='1'+ID-SDL_SCANCODE_1;if(ID==ZEROKEY)c='0';if(ID==MINUSKEY)c=keyIsHit(SHIFTKEY)?'_':'-';if(ID==GREATERTHAN)c='.';
   if(spe.edit>=10&&spe.edit<=12&&ID>=AKEY&&ID<=ZKEY)c=(keyIsHit(SHIFTKEY)?'A':'a')+ID-AKEY;
   if(c&&n<(spe.edit==12?47:spe.edit==10||spe.edit==11?63:35)){spe.input[n]=c;spe.input[n+1]=0;}return RPR_Redraw;
  }
  if(ID==ESCKEY){if((spe.dirty||speFilePending())&&!spe.discard){spe.discard=1;speStatus("ESC AGAIN TO DISCARD PROJECT AND SHIP FILE / OR SAVE");return RPR_Redraw;}speClose();mpeFreeMeshes();mpe.stage=1;return RPR_Redraw;}
  if(keyIsHit(CONTROLKEY)){if(ID==SKEY)speAction(1);if(ID==ZKEY)speAction(3);if(ID==YKEY)speAction(4);if(ID==CKEY)speAction(11);if(ID==VKEY)speAction(12);if(ID==DKEY&&spe.selected>=0){speAction(11);speAction(12);}}else if(ID==GKEY)mpe.rotateMode=0;else if(ID==TKEY||ID==RKEY)mpe.rotateMode=1;else if(ID==FKEY)speFrame();else if(ID==SPACEKEY&&p){p->enabled=!p->enabled;speCheckpoint();}return RPR_Redraw;
 }
 if(event==RPE_WheelUp||event==RPE_WheelDown){int d=event==RPE_WheelUp?-1:1;if(spe.dropdown)spe.dropScroll=muiClamp(spe.dropScroll+d,0,spe.sourceCount>12?spe.sourceCount-12:0);else if(muiPointIn(&mpeList,x,y))spe.scroll=muiClamp(spe.scroll+d,0,speTree.count?speTree.count-1:0);else if(muiPointIn(&mpeView,x,y))mpe.zoomTarget=muiClampF(mpe.zoomTarget*cameraWheelZoomFactor(d<0),.25f,1000);return RPR_Redraw;}
 if(event==RPE_PressRight||event==RPE_PressCentre){if(muiPointIn(&mpeView,x,y)){speCommit();if(event==RPE_PressRight){mpe.drag=3;mpe.cameraCaptured=1;mpe.cursorX=x;mpe.cursorY=y;mouseCursorHide();mouseClipToRect(&mpeView);x=(mpeView.x0+mpeView.x1)/2;y=(mpeView.y0+mpeView.y1)/2;mousePositionSet(x,y);}else mpe.drag=2;mpe.lastX=x;mpe.lastY=y;}return RPR_Redraw;}
 if(mpe.cameraCaptured&&(event==RPE_PressLeft||event==RPE_ReleaseLeft))return RPR_Redraw;
 if(event==RPE_PressLeft){speCommit();
  if(spe.dropdown){rectangle drop=muiRect(speSource.x0,speSource.y1,mpeView.x0+uiScaleSize(200),speSource.y1+uiScaleSize(360));if(muiPointIn(&drop,x,y)){int source=spe.dropScroll+(y-drop.y0)/uiScaleSize(30);if(spe.dirty)speStatus("SAVE OR DISCARD THIS PROJECT BEFORE SWITCHING SOURCE");else speTemplate(source);}spe.dropdown=0;return RPR_Redraw;}
  for(i=0;i<16;i++)if(muiPointIn(&speButtons[i],x,y)){if(i==15)modUIOpen(2);else if(i==14)modUIOpen(1);else if(i==13)speEventsOpen();else if(i==12)speSubsystemsOpen();else if(i==11)speFileOpen();else speAction(i);return RPR_Redraw;}
  if(muiPointIn(&speSource,x,y)){spe.dropdown=1;spe.dropScroll=0;return RPR_Redraw;}
  if(muiPointIn(&speKind,x,y)){spe.kind=spe.kind==SAP_ENGINE?SAP_JOINT:spe.kind+1;return RPR_Redraw;}
  if(muiPointIn(&speMaterial,x,y)&&spe.doc.materialCount){spe.material=(spe.material+1)%spe.doc.materialCount;return RPR_Redraw;}
  for(i=0;i<3;i++)if(muiPointIn(&speTextures[i],x,y)){speTexture(i);return RPR_Redraw;}
  for(i=0;i<14;i++)if(muiPointIn(&speFields[i],x,y)){if(i<12&&!p)return RPR_Redraw;spe.edit=i;if(i<10)snprintf(spe.input,128,"%.3f",*speValue(p,i));else if(i==10||i==11)snprintf(spe.input,128,"%s",i==10?p->name:p->parent);else if(i==12)snprintf(spe.input,128,"%s",spe.doc.name);else snprintf(spe.input,128,"%.3f",spe.doc.scale);return RPR_Redraw;}
  {int row=speTreeRow(x,y);if(row>=0){int node=speTree.node[row];if(node<0)speTree.folders[-1-node]^=1;else {spe.selected=node;if(x<mpeList.x0+uiScaleSize(32+12*muiClamp(speTree.depth[row],0,6)))speTree.folded[node]^=1;else {speTree.drag=node+1;speTree.startX=x;speTree.startY=y;}}return RPR_Redraw;}}
  if(muiPointIn(&mpeView,x,y)){float best=uiScaleSize(12)*uiScaleSize(12);int chosen=-1;
   if(p&&mpe.rotateMode)chosen=speRings(0,x,y);else if(p){float w[3];speWorldPoint(spe.selected,w,0);for(i=0;i<3;i++){float a[3],px,py;speAxis(spe.selected,i,uiScaleSize(80)/mpe.zoom,a);a[0]+=w[0];a[1]+=w[1];a[2]+=w[2];mpeProject(a,&px,&py);if((px-x)*(px-x)+(py-y)*(py-y)<best){best=(px-x)*(px-x)+(py-y)*(py-y);chosen=i;}}}
   if(chosen>=0){mpe.axis=chosen;mpe.drag=mpe.rotateMode?5:4;}else {int hit=spePickGeometry(x,y);if(hit>=0)spe.selected=hit;else for(i=0;i<(int)spe.doc.pointCount;i++){float w[3],px,py;speWorldPoint(i,w,0);mpeProject(w,&px,&py);if((px-x)*(px-x)+(py-y)*(py-y)<best){best=(px-x)*(px-x)+(py-y)*(py-y);spe.selected=i;}}}
  }
 }
 if(mpe.drag&&(event==RPE_HoldLeft||event==RPE_HoldRight||event==RPE_HoldCentre)){
  if(mpe.drag==2){mpe.panX+=dx;mpe.panY+=dy;}
  if(mpe.drag==3){Camera camera={0};int cx=(mpeView.x0+mpeView.x1)/2,cy=(mpeView.y0+mpeView.y1)/2;camera.angle=(-90-mpe.yaw)*.01745329252f;camera.declination=(-90-mpe.pitch)*.01745329252f;camera.distance=1/mpe.zoom;camera.closestZoom=.25f;camera.farthestZoom=1000;cameraMouseDrag(&camera,cx-x,cy-y,mouseLeftButton(),FALSE);mpe.yaw=-90-camera.angle*57.29577951f;mpe.pitch=-90-camera.declination*57.29577951f;if(mouseLeftButton()){mpe.zoom=1/camera.distance;mpe.zoomTarget=camera.distance;}mousePositionSet(cx,cy);x=cx;y=cy;}
  if(p&&mpe.drag==4){float a[3],v[3],denom;speAxis(spe.selected,mpe.axis,1,a);mpeViewRotate(a,v);denom=v[0]*v[0]+v[1]*v[1];if(denom>.0001f)p->position[mpe.axis]=muiClampF(p->position[mpe.axis]+(dx*v[0]-dy*v[1])/(mpe.zoom*denom),-1000000,1000000);spe.dirty=1;}
  if(p&&mpe.drag==5){shipAuthorRotateLocal(&spe.doc,spe.selected,mpe.axis,(dx*spe.rotateTangentX+dy*spe.rotateTangentY)*57.29577951f/spe.rotatePixels);spe.dirty=1;}
 }
 if(event==RPE_ReleaseLeft&&speTree.drag)speTreeDrop(x,y);
 if(event==RPE_ReleaseRight&&mpe.cameraCaptured)mpeReleaseCamera();else if(event==RPE_ReleaseLeft||event==RPE_ReleaseRight||event==RPE_ReleaseCentre){if(mpe.drag==4||mpe.drag==5)speCheckpoint();if(!mpe.cameraCaptured)mpe.drag=0;}
 mpe.lastX=x;mpe.lastY=y;return RPR_Redraw;
}
