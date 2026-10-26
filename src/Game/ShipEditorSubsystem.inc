/* Shared native components: independently placed, never independently damaged. */
#include "NativeVisualRegistry.h"
#include "OpenHWShipDocument.h"
static struct {int count,open,scroll;char names[256][64];meshdata* meshes[256];unsigned char attempted[256];} speSubs;
static int spePickLibraryGeometry(int x,int y,float*nearest,int hit);
static int speSubsystemOwner(int point){unsigned depth=0;while(point>=0&&depth++<spe.doc.pointCount){if(spe.doc.points[point].subsystem[0])return point;point=shipAuthorParent(&spe.doc,point);}return -1;}
static int speSubsystemHasGeometry(int point){unsigned i;for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].object>=0&&spe.doc.points[i].object<256&&speSubsystemOwner(i)==point)return 1;return 0;}
static void speSubsystemsImport(void){char path[256],*text,*line,*mounts;void*raw=NULL;sdword n;unsigned count=spe.doc.pointCount,i;
 snprintf(path,sizeof(path),"%s/%s/RMNative/ship.owshipdef",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 if(!fileExists(path,0)||(n=fileSizeGet(path,0))<=0||n>1024*1024)return;n=fileLoadAlloc(path,&raw,0);text=malloc(n+1);if(!text){memFree(raw);return;}memcpy(text,raw,n);text[n]=0;memFree(raw);
 {void*normalized=NULL;size_t length;char error[256];if(openhwShipDocumentNormalize(text,n,&normalized,&length,error,sizeof(error))){free(text);text=normalized;}}
 mounts=_strdup(text);if(!mounts){free(text);return;}
 for(line=strtok(text,"\r\n");line;line=strtok(NULL,"\r\n")){char slot[64],anchor[64],role[64],type[64];int root=-1,parent=-1;ShipAuthorPoint*p;
  if(sscanf(line,"StartShipHardPointConfig(NewShipType, \"%63[^\"]\", \"%63[^\"]\", \"%63[^\"]\", \"Innate\", \"Ship\", \"%63[^\"]\"",slot,anchor,role,type)!=4)continue;
  for(i=0;i<count;i++){const polygonobject*o;if(spe.doc.points[i].object<0||spe.doc.points[i].object>=spe.mesh->nPolygonObjects)continue;o=&spe.mesh->object[spe.doc.points[i].object];if(o->pName&&!strncmp(o->pName,slot,strlen(slot))&&o->pName[strlen(slot)]=='.'&&(!o->pMother||!o->pMother->pName||strncmp(o->pMother->pName,slot,strlen(slot)))){root=i;break;}}
  if(root<0||spe.doc.pointCount>=SHIP_AUTHOR_POINTS)continue;parent=shipAuthorParent(&spe.doc,root);
  p=&spe.doc.points[spe.doc.pointCount];memset(p,0,sizeof(*p));p->object=-1;p->enabled=1;p->kind=SAP_SUBSYSTEM;p->size=20;p->color[1]=1;
  snprintf(p->name,64,"%s",slot);snprintf(p->parent,64,"%s",parent>=0?spe.doc.points[parent].name:"-");snprintf(p->subsystem,64,"%s",type);
  p->subsystemRole=!strcmp(role,"Weapon")?1:!strcmp(role,"Engine")?2:3;
  /* Factor the authored mount back out of the native component's root.
     Reparent preserves its world pose, avoiding a doubled transform on save. */
  {const char*row=mounts;while(row&&*row){char name[64],base[64];float v[6];
   if(sscanf(row,"hardpoint %63s %63s %f %f %f %f %f %f",name,base,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5])==8&&!strcmp(name,anchor)){
    float x=v[3]*.01745329252f,y=v[4]*.01745329252f,z=v[5]*.01745329252f,cx=cosf(x),sx=sinf(x),cy=cosf(y),sy=sinf(y),cz=cosf(z),sz=sinf(z),m[16]={0},scale=1;unsigned lod;
    meshNativeVisualParameters(spe.mesh,&scale,&lod);m[0]=cz*cy;m[1]=sz*cy;m[2]=-sy;m[4]=cz*sy*sx-sz*cx;m[5]=sz*sy*sx+cz*cx;m[6]=cy*sx;m[8]=cz*sy*cx+sz*sx;m[9]=sz*sy*cx-cz*sx;m[10]=cy*cx;m[12]=v[0]*scale;m[13]=v[1]*scale;m[14]=v[2]*scale;m[15]=1;
    speRestPose((hmatrix*)m,p->position,p->rotation);break;
   }row=strchr(row,'\n');if(row)row++;
  }}
  spe.doc.pointCount++;if(!shipAuthorReparent(&spe.doc,root,spe.doc.pointCount-1)){spe.doc.pointCount--;speStatus("SUBSYSTEM HIERARCHY IMPORT FAILED");}
 }free(mounts);free(text);
}
static void speGeometryVertex(const float*matrix,const vertexentry*v,float*out){float p[3]={v->y,v->z,v->x};int a;for(a=0;a<3;a++)out[a]=matrix[12+a]+matrix[a]*p[0]+matrix[4+a]*p[1]+matrix[8+a]*p[2];}
static int spePickGeometry(int x,int y){unsigned groupIndex;int hit=-1;float nearest=-1e30f;if(!spe.mesh)return -1;
 for(groupIndex=0;groupIndex<=spe.doc.pointCount;groupIndex++){int group=groupIndex?spe.doc.points[groupIndex-1].clone:0,object;unsigned prior;if(groupIndex){if(!group)continue;for(prior=0;prior+1<groupIndex;prior++)if(spe.doc.points[prior].clone==group)break;if(prior+1<groupIndex)continue;}
  for(object=0;object<spe.mesh->nPolygonObjects;object++){polygonobject*o=&spe.mesh->object[object];float matrix[16];int owner=speObjectOwner(object,group),poly;if(owner<0||!speVisible(owner))continue;speObjectWorld(object,group,matrix,0);
   for(poly=0;poly<o->nPolygons;poly++){polyentry*p=&o->pPolygonList[poly];unsigned ids[3]={p->iV0,p->iV1,p->iV2};float sx[3],sy[3],z[3],den,a,b,c;int k;
    for(k=0;k<3;k++){float world[3],view[3];speGeometryVertex(matrix,&o->pVertexList[ids[k]],world);mpeProject(world,&sx[k],&sy[k]);mpeViewRotate(world,view);z[k]=view[2];}
    den=(sy[1]-sy[2])*(sx[0]-sx[2])+(sx[2]-sx[1])*(sy[0]-sy[2]);if(fabsf(den)<.0001f)continue;
    a=((sy[1]-sy[2])*(x-sx[2])+(sx[2]-sx[1])*(y-sy[2]))/den;b=((sy[2]-sy[0])*(x-sx[2])+(sx[0]-sx[2])*(y-sy[2]))/den;c=1-a-b;
    if(a>=0&&b>=0&&c>=0&&a*z[0]+b*z[1]+c*z[2]>nearest){int subsystem=speSubsystemOwner(owner);nearest=a*z[0]+b*z[1]+c*z[2];hit=subsystem>=0?subsystem:owner;}
   }
  }
 }return spePickLibraryGeometry(x,y,&nearest,hit);
}
static void speHighlightMeshPart(meshdata*mesh,int object,const float*matrix,int owner){polygonobject*o;int poly,selected=speSubsystemOwner(spe.selected);if(selected<0||speSubsystemOwner(owner)!=selected)return;o=&mesh->object[object];if(!o->nPolygons)return;
 glPushAttrib(GL_ALL_ATTRIB_BITS);rndTextureEnable(FALSE);glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);hwglSurfaceMaterial(NULL);glColor4f(.1f,1,.15f,.45f);glBegin(GL_TRIANGLES);
 for(poly=0;poly<o->nPolygons;poly++){polyentry*p=&o->pPolygonList[poly];unsigned ids[3]={p->iV0,p->iV1,p->iV2};int k;for(k=0;k<3;k++){float v[3];speGeometryVertex(matrix,&o->pVertexList[ids[k]],v);glVertex3fv(v);}}glEnd();glPopAttrib();rndTextureEnable(TRUE);
}
static void speHighlightPart(int object,const float*matrix,int owner){speHighlightMeshPart(spe.mesh,object,matrix,owner);}
static void speSubsystemsClear(void){int i;for(i=0;i<256;i++){if(speSubs.meshes[i])meshFree(speSubs.meshes[i]);speSubs.meshes[i]=NULL;speSubs.attempted[i]=0;}speSubs.open=0;}
static int speSubsystemIndex(const char*name){int i;for(i=0;i<speSubs.count;i++)if(!strcmp(name,speSubs.names[i]))return i;return -1;}
static void speSubsystemCatalogAdd(const char*name){char path[256];size_t n=strlen(name);unsigned i;if(!n||n>63||speSubs.count>=256||speSubsystemIndex(name)>=0)return;
 for(i=0;i<n;i++)if(!isalnum((unsigned char)name[i])&&name[i]!='_')return;snprintf(path,sizeof(path),"Subsystems/%s/subsystem.owsubsystem",name);
 if(fileExists(path,0))snprintf(speSubs.names[speSubs.count++],64,"%s",name);
}
static void speSubsystemsCatalog(void){void*data;char*line,*next;speSubs.count=0;
 if(fileExists("Subsystems/catalog.txt",0)&&fileSizeGet("Subsystems/catalog.txt",0)<=65536) {int n=fileLoadAlloc("Subsystems/catalog.txt",&data,0);char*text=malloc(n+1);if(!text){memFree(data);return;}memcpy(text,data,n);text[n]=0;memFree(data);line=text;
  while(line&&speSubs.count<256){char path[256];size_t size;unsigned i;next=strchr(line,'\n');if(next)*next++=0;size=strcspn(line,"\r");line[size]=0;
   if(size&&size<64){for(i=0;i<size;i++)if(!isalnum((unsigned char)line[i])&&line[i]!='_')break;
    snprintf(path,sizeof(path),"Subsystems/%s/subsystem.owsubsystem",line);if(i==size&&speSubsystemIndex(line)<0&&fileExists(path,0))speSubsystemCatalogAdd(line);}
   line=next;
  }free(text);
 }
 {char*base=SDL_GetBasePath();char path[1024];WIN32_FIND_DATAA found;HANDLE search;if(!base)return;snprintf(path,sizeof(path),"%sData\\Subsystems\\*",base);SDL_free(base);search=FindFirstFileA(path,&found);
  if(search!=INVALID_HANDLE_VALUE){do {if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)speSubsystemCatalogAdd(found.cFileName);}while(FindNextFileA(search,&found));FindClose(search);}}
}
static float speSubsystemScale(void){float scale=1;unsigned lod;if(spe.mesh)meshNativeVisualParameters(spe.mesh,&scale,&lod);return scale;}
static void speSubsystemMatrix(meshdata*mesh,int object,float*out,int depth){float local[16],parent[16];int a,b,k,axes[4]={1,2,0,3};polygonobject*o=&mesh->object[object];
 for(a=0;a<4;a++)for(b=0;b<4;b++)local[a*4+b]=((float*)&o->localMatrix)[axes[a]*4+axes[b]];
 if(!o->pMother||depth>=256){memcpy(out,local,sizeof(local));return;}speSubsystemMatrix(mesh,(int)(o->pMother-mesh->object),parent,depth+1);
 for(a=0;a<4;a++)for(b=0;b<4;b++){out[a*4+b]=0;for(k=0;k<4;k++)out[a*4+b]+=parent[k*4+b]*local[a*4+k];}
}
static void speSubsystemsDraw(void){unsigned i;static const GLfloat basis[16]={0,0,1,0,1,0,0,0,0,1,0,0,0,0,0,1};
 for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].subsystem[0]&&speVisible(i)&&!speSubsystemHasGeometry(i)){float mount[16],local[16];int index=speSubsystemIndex(spe.doc.points[i].subsystem),j;meshdata*mesh;
  if(index<0)continue;if(!speSubs.attempted[index]){speSubs.attempted[index]=1;speSubs.meshes[index]=meshLoadOpenHWSubsystem(speSubs.names[index],speSubsystemScale());}
  mesh=speSubs.meshes[index];if(!mesh)continue;shipAuthorWorld(&spe.doc,i,mount);
  for(j=0;j<mesh->nPolygonObjects;j++)if(mesh->object[j].nPolygons){unsigned point;int a,b,k;float world[16];
   for(point=0;point<spe.doc.pointCount;point++)if(spe.doc.points[point].object==256+j&&speSubsystemOwner(point)==(int)i)break;
   if(point<spe.doc.pointCount){if(!speVisible(point))continue;shipAuthorWorld(&spe.doc,point,world);}
   else {speSubsystemMatrix(mesh,j,local,0);for(a=0;a<4;a++)for(b=0;b<4;b++){world[a*4+b]=0;for(k=0;k<4;k++)world[a*4+b]+=mount[k*4+b]*local[a*4+k];}}
   glPushMatrix();glMultMatrixf(world);glMultMatrixf(basis);meshRenderAuthoringPart(mesh,j);glPopMatrix();speHighlightMeshPart(mesh,j,world,i);
  }
 }
}
static int spePickLibraryGeometry(int x,int y,float*nearest,int hit){unsigned i;
 for(i=0;i<spe.doc.pointCount;i++){int owner,index,object,poly;meshdata*mesh;polygonobject*o;float matrix[16];
  if(spe.doc.points[i].object<256||!speVisible(i))continue;owner=speSubsystemOwner(i);if(owner<0)continue;index=speSubsystemIndex(spe.doc.points[owner].subsystem);
  if(index<0||!(mesh=speSubs.meshes[index]))continue;object=spe.doc.points[i].object-256;if(object>=mesh->nPolygonObjects)continue;o=&mesh->object[object];shipAuthorWorld(&spe.doc,i,matrix);
  for(poly=0;poly<o->nPolygons;poly++){polyentry*p=&o->pPolygonList[poly];unsigned ids[3]={p->iV0,p->iV1,p->iV2};float sx[3],sy[3],z[3],den,a,b,c,depth;int k;
   for(k=0;k<3;k++){float world[3],view[3];speGeometryVertex(matrix,&o->pVertexList[ids[k]],world);mpeProject(world,&sx[k],&sy[k]);mpeViewRotate(world,view);z[k]=view[2];}
   den=(sy[1]-sy[2])*(sx[0]-sx[2])+(sx[2]-sx[1])*(sy[0]-sy[2]);if(fabsf(den)<.0001f)continue;
   a=((sy[1]-sy[2])*(x-sx[2])+(sx[2]-sx[1])*(y-sy[2]))/den;b=((sy[2]-sy[0])*(x-sx[2])+(sx[0]-sx[2])*(y-sy[2]))/den;c=1-a-b;depth=a*z[0]+b*z[1]+c*z[2];
   if(a>=0&&b>=0&&c>=0&&depth>*nearest){*nearest=depth;hit=owner;}
  }
 }return hit;
}
static rectangle speSubsystemsBox(void){return muiRect(MAIN_WindowWidth/4,MAIN_WindowHeight/5,MAIN_WindowWidth*3/4,MAIN_WindowHeight*4/5);}
static void speSubsystemsOpen(void){speSubsystemsCatalog();speSubs.open=1;speSubs.scroll=0;mpeReleaseCamera();}
static void speSubsystemsPicker(void){rectangle box;int i,rows;char text[160];if(!speSubs.open)return;box=speSubsystemsBox();muiFill(&box,colRGB(15,30,40));
 muiText(box.x0+12,box.y0+12,muiPaper,"AVAILABLE SUBSYSTEMS / CLICK TO PLACE",muiHeadingFont);rows=(box.y1-box.y0-uiScaleSize(85))/uiScaleSize(30);
 for(i=speSubs.scroll;i<speSubs.count&&i<speSubs.scroll+rows;i++){rectangle row=muiRect(box.x0+8,box.y0+uiScaleSize(45+(i-speSubs.scroll)*30),box.x1-8,box.y0+uiScaleSize(73+(i-speSubs.scroll)*30));muiDrawButtonRect(&row,speSubs.names[i],FALSE,muiPointIn(&row,mouseCursorX(),mouseCursorY()));}
 snprintf(text,sizeof(text),"%d available / click outside to close%s",speSubs.count,speSubs.count?"":" / library not installed");muiText(box.x0+12,box.y1-uiScaleSize(27),muiMuted,text,muiBodyFont);
}
static int speSubsystemsProcess(udword event,int x,int y){rectangle box;int index,rows,parent;ShipAuthorPoint*p;unsigned i,j;if(!speSubs.open)return 0;box=speSubsystemsBox();rows=(box.y1-box.y0-uiScaleSize(85))/uiScaleSize(30);
 if(event==RPE_WheelUp||event==RPE_WheelDown){speSubs.scroll=muiClamp(speSubs.scroll+(event==RPE_WheelUp?-1:1),0,speSubs.count>rows?speSubs.count-rows:0);return 1;}
 if(event!=RPE_PressLeft)return 1;if(!muiPointIn(&box,x,y)){speSubs.open=0;return 1;}
 index=speSubs.scroll+(y-box.y0-uiScaleSize(45))/uiScaleSize(30);if(y<box.y0+uiScaleSize(45)||index<0||index>=speSubs.count||index>=speSubs.scroll+rows)return 1;
 if(!strstr(speSubs.names[index],"engine")){
  char definitionPath[256];void*raw=NULL;char*text=NULL;sdword length;int armed=0;
  snprintf(definitionPath,sizeof(definitionPath),"Subsystems/%s/subsystem.owsubsystem",speSubs.names[index]);
  if(fileSizeGet(definitionPath,0)<=SHIP_TEXT_LIMIT){length=fileLoadAlloc(definitionPath,&raw,0);if(length>0){text=malloc(length+1);if(text){memcpy(text,raw,length);text[length]=0;armed=strstr(text,"[Weapons]")&&strstr(text,"GUN ");free(text);}memFree(raw);}}
  if(!armed){speStatus("VISUAL-ONLY TEMPLATE / SELECT AN ARMED SUBSYSTEM WITH WEAPON SETTINGS");return 1;}
 }
 if(!speSubs.attempted[index]){speSubs.attempted[index]=1;speSubs.meshes[index]=meshLoadOpenHWSubsystem(speSubs.names[index],speSubsystemScale());}
 if(!speSubs.meshes[index]){speStatus("SUBSYSTEM MODEL COULD NOT BE LOADED");return 1;}
 if(spe.doc.pointCount+1+speSubs.meshes[index]->nPolygonObjects>SHIP_AUTHOR_POINTS){speStatus("POINT LIMIT REACHED / NO SUBSYSTEM ADDED");return 1;}
 parent=spe.selected;if(parent<0){for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].object>=0&&!strcmp(spe.doc.points[i].parent,"-")){parent=i;break;}}
 if(parent<0){speStatus("SELECT OR CREATE A MOUNT JOINT FIRST");return 1;}
 p=&spe.doc.points[spe.doc.pointCount];memset(p,0,sizeof(*p));p->kind=SAP_SUBSYSTEM;p->enabled=1;p->object=-1;p->size=20;p->color[0]=1;p->color[1]=.75f;p->color[2]=.2f;
 snprintf(p->parent,64,"%s",spe.doc.points[parent].name);snprintf(p->subsystem,64,"%s",speSubs.names[index]);
 p->subsystemRole=strstr(p->subsystem,"engine")?2:1;
 for(i=0;i<10000;i++){snprintf(p->name,64,"Subsystem_%u",i);for(j=0;j<spe.doc.pointCount;j++)if(!strcmp(p->name,spe.doc.points[j].name))break;if(j==spe.doc.pointCount)break;}
 spe.selected=spe.doc.pointCount++;
 {meshdata*mesh=speSubs.meshes[index];unsigned first=spe.doc.pointCount;
  for(j=0;j<(unsigned)mesh->nPolygonObjects;j++){ShipAuthorPoint*child=&spe.doc.points[spe.doc.pointCount++];memset(child,0,sizeof(*child));child->kind=SAP_JOINT;child->enabled=1;child->object=256+j;child->size=10;child->color[0]=child->color[1]=child->color[2]=.65f;snprintf(child->name,64,"%.40s_J%u",p->name,j);speRestPose(&mesh->object[j].localMatrix,child->position,child->rotation);}
  for(j=0;j<(unsigned)mesh->nPolygonObjects;j++){polygonobject*o=&mesh->object[j];snprintf(spe.doc.points[first+j].parent,64,"%s",o->pMother?spe.doc.points[first+(o->pMother-mesh->object)].name:p->name);}
 }
 speSubs.open=0;speCheckpoint();speStatus("SUBSYSTEM PLACED / MOVE OR ROTATE / CHILD JOINTS FOLLOW THE COMPONENT");return 1;
}
static const char*speSubsystemJointName(int point){int owner=speSubsystemOwner(point),object,index;meshdata*mesh;const char*name;
 if(owner<0||point==owner)return NULL;object=spe.doc.points[point].object;if(object<0)return NULL;
 if(object>=256){index=speSubsystemIndex(spe.doc.points[owner].subsystem);if(index<0)return NULL;mesh=speSubs.meshes[index];object-=256;}
 else mesh=spe.mesh;
 if(!mesh||object>=mesh->nPolygonObjects||mesh->object[object].nPolygons)return NULL;name=mesh->object[object].pName;
 if(spe.doc.points[point].object<256&&name){const char*dot=strchr(name,'.');if(dot)name=dot+1;}return name;
}
static int speSubsystemsSyncV2(int persist){char*source,*out,*line,*next;size_t used=0;unsigned i;char path[1024];int wasOpen=speFile.open;float scale=speSubsystemScale();
 speFileOpen();speFile.open=wasOpen;if(!speFile.text.text||strncmp(speFile.text.text,"OPENHW_SHIP_2",13))return -1;
 {void*normalized=NULL;size_t length;char error[256];if(!openhwShipDocumentNormalize(speFile.text.text,strlen(speFile.text.text),&normalized,&length,error,sizeof(error))){speStatus(error);return 0;}source=normalized;}
 out=malloc(strlen(source)+spe.doc.pointCount*1024+1024);if(!out){free(source);return 0;}
 line=source;while(line){char slot[64];next=strchr(line,'\n');if(next)*next++=0;
  if(!strcmp(line,"[Ship]"))break;
  if(!strncmp(line,"StartShipHardPointConfig(",25)||!strncmp(line,"hardpoint ",10)){line=next;continue;}
  if(sscanf(line,"joint %63s",slot)==1&&strcmp(slot,"Hull")){line=next;continue;}
  if(sscanf(line,"weaponSlot %63s",slot)==1){for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].subsystem[0]&&spe.doc.points[i].enabled&&!strcmp(spe.doc.points[i].name,slot))break;if(i==spe.doc.pointCount){line=next;continue;}}
  used+=sprintf(out+used,"%s\n",line);line=next;
 }
 if(!line||strcmp(line,"[Ship]")){free(source);free(out);return 0;}
 for(i=0;i<spe.doc.pointCount;i++){const ShipAuthorPoint*p=&spe.doc.points[i];int anchor=shipAuthorParent(&spe.doc,i),a,b,k;float world[16],parent[16],local[16]={0},hw[16],rotation[3];int axes[4]={2,0,1,3};const char*name;
  if(!p->subsystem[0]||!p->enabled)continue;
  while(anchor>=0&&spe.doc.points[anchor].object<0)anchor=shipAuthorParent(&spe.doc,anchor);
  if(!spe.mesh||anchor<0||spe.doc.points[anchor].object>=spe.mesh->nPolygonObjects){free(source);free(out);speStatus("PARENT THE SUBSYSTEM TO A HULL JOINT BEFORE SAVING");return 0;}
  name=spe.mesh->object[spe.doc.points[anchor].object].pName;if(!name||!*name){free(source);free(out);return 0;}
  shipAuthorWorld(&spe.doc,i,world);shipAuthorWorld(&spe.doc,anchor,parent);local[15]=1;
  for(a=0;a<3;a++){for(b=0;b<3;b++)for(k=0;k<3;k++)local[a*4+b]+=parent[b*4+k]*world[a*4+k];for(k=0;k<3;k++)local[12+a]+=parent[a*4+k]*(world[12+k]-parent[12+k]);}
  for(a=0;a<4;a++)for(b=0;b<4;b++)hw[a*4+b]=local[axes[a]*4+axes[b]];
  rotation[1]=asinf(muiClampF(-hw[2],-1,1))*57.29577951f;if(fabsf(hw[2])<.999999f){rotation[0]=atan2f(hw[6],hw[10])*57.29577951f;rotation[2]=atan2f(hw[1],hw[0])*57.29577951f;}else{rotation[0]=atan2f(-hw[9],hw[5])*57.29577951f;rotation[2]=0;}
  used+=sprintf(out+used,"hardpoint HP_%s %s %.9g %.9g %.9g %.9g %.9g %.9g\nStartShipHardPointConfig(NewShipType, \"%s\", \"HP_%s\", \"%s\", \"Innate\", \"Ship\", \"%s\");\n",p->name,name,hw[12]/scale,hw[13]/scale,hw[14]/scale,rotation[0],rotation[1],rotation[2],p->name,p->name,p->subsystemRole==2?"Engine":p->subsystemRole==3?"Utility":"Weapon",p->subsystem);
 }
 for(i=0;i<spe.doc.pointCount;i++){int owner=speSubsystemOwner(i),parentIndex,a,b,k;const char*name,*parentName="-";float world[16],parent[16],local[16]={0},hw[16],rotation[3];int axes[4]={2,0,1,3};
  if(owner<0||!speVisible(i)||!(name=speSubsystemJointName(i)))continue;
  parentIndex=shipAuthorParent(&spe.doc,i);while(parentIndex>=0&&parentIndex!=owner&&!speSubsystemJointName(parentIndex))parentIndex=shipAuthorParent(&spe.doc,parentIndex);
  if(parentIndex<0||speSubsystemOwner(parentIndex)!=owner){free(source);free(out);speStatus("KEEP SUBSYSTEM JOINTS WITHIN THEIR OWN COMPONENT");return 0;}
  if(parentIndex!=owner)parentName=speSubsystemJointName(parentIndex);
  shipAuthorWorld(&spe.doc,i,world);shipAuthorWorld(&spe.doc,parentIndex,parent);local[15]=1;
  for(a=0;a<3;a++){for(b=0;b<3;b++)for(k=0;k<3;k++)local[a*4+b]+=parent[b*4+k]*world[a*4+k];for(k=0;k<3;k++)local[12+a]+=parent[a*4+k]*(world[12+k]-parent[12+k]);}
  for(a=0;a<4;a++)for(b=0;b<4;b++)hw[a*4+b]=local[axes[a]*4+axes[b]];
  rotation[1]=asinf(muiClampF(-hw[2],-1,1))*57.29577951f;if(fabsf(hw[2])<.999999f){rotation[0]=atan2f(hw[6],hw[10])*57.29577951f;rotation[2]=atan2f(hw[1],hw[0])*57.29577951f;}else{rotation[0]=atan2f(-hw[9],hw[5])*57.29577951f;rotation[2]=0;}
  used+=sprintf(out+used,"joint %s %s %s %.9g %.9g %.9g %.9g %.9g %.9g\n",spe.doc.points[owner].name,name,parentName,hw[12]/scale,hw[13]/scale,hw[14]/scale,rotation[0],rotation[1],rotation[2]);
 }
 used+=sprintf(out+used,"\n[Ship]\n%s",next?next:"");free(source);
 {char root[256],error[256];void*complete=NULL;size_t length=0;
  snprintf(root,sizeof(root),"%s/%s/RMNative/",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
  if(!meshCompleteOpenHWDefinition(root,out,used,&complete,&length,error,sizeof(error))){free(out);speStatus(error);return 0;}
  free(out);out=complete;used=length;
 }
 if(!speFileParse(out,used)){free(out);return 0;}free(out);speFile.dirty=1;
 if(!persist)return 1;
 {char root[256],identity[80],error[256];snprintf(root,sizeof(root),"%s/%s/RMNative/",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));snprintf(identity,sizeof(identity),"%s%s",spe.doc.race==R2?"Taiidan_":"",ShipTypeToStr(spe.doc.type));
  if(!meshValidateOpenHWDefinition(root,identity,speFile.text.text,strlen(speFile.text.text),error,sizeof(error))){speStatus(error);return 0;}}
 if(persist==2)return 1; /* Preflight before any project file is replaced. */
 if(!shipAuthorProjectPath(spe.doc.name,"ship.owshipdef",path,sizeof(path),1)||!speFileWrite(path))return 0;speFile.dirty=0;return 1;
}
static int speSubsystemsSaveDefinition(void){char path[1024],*source,*out;void*bytes=NULL;size_t size=0,used;unsigned i;int count=0;float scale=speSubsystemScale();
 int v2=speSubsystemsSyncV2(1);if(v2>=0)return v2;
 for(i=0;i<spe.doc.pointCount;i++)if(spe.doc.points[i].subsystem[0])count++;
 if(speFile.dirty&&count){speStatus("SAVE THE OPEN TEXT FILE BEFORE SAVING SUBSYSTEM DECLARATIONS");return 0;}
 if(!shipAuthorProjectPath(spe.doc.name,"ship.owshipdef",path,sizeof(path),1))return 0;
 if(!count&&GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)return 1;
 if(!speReadFile(path,&bytes,&size)){snprintf(path,sizeof(path),"%s/%s/RMNative/ship.owshipdef",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));if(!fileExists(path,0)||fileSizeGet(path,0)>128*1024)return 0;size=fileLoadAlloc(path,&bytes,0);source=malloc(size+1);if(source)memcpy(source,bytes,size);memFree(bytes);}else {source=malloc(size+1);if(source)memcpy(source,bytes,size);free(bytes);}
 if(!source)return 0;source[size]=0;{size_t at,to=0;for(at=0;at<size;at++)if(source[at]!='\r')source[to++]=source[at];source[to]=0;}if(strncmp(source,"OPENHW_SHIP_1",13)){free(source);return 0;}
 if(!count&&!strstr(source,"# BEGIN EDITOR SUBSYSTEMS\n")){free(source);return 1;}if(speFile.dirty){free(source);return 0;}
 {char*begin=strstr(source,"# BEGIN EDITOR SUBSYSTEMS\n");if(begin){char*end=strstr(begin,"# END EDITOR SUBSYSTEMS");if(!end){free(source);return 0;}end=strchr(end,'\n');memmove(begin,end?end+1:source+strlen(source),strlen(end?end+1:"")+1);}}
 out=malloc(strlen(source)+count*1024+128);if(!out){free(source);return 0;}strcpy(out,source);free(source);used=strlen(out);used+=sprintf(out+used,"\n# BEGIN EDITOR SUBSYSTEMS\n");
 for(i=0;i<spe.doc.pointCount;i++){const ShipAuthorPoint*p=&spe.doc.points[i];int anchor=shipAuthorParent(&spe.doc,i),a,b,k;float world[16],parent[16],local[16],hw[16],rotation[3];int axes[4]={2,0,1,3};const char*name;
  if(!p->subsystem[0]||!p->enabled)continue;
  while(anchor>=0&&spe.doc.points[anchor].object<0)anchor=shipAuthorParent(&spe.doc,anchor);
  if(!spe.mesh||anchor<0||spe.doc.points[anchor].object>=spe.mesh->nPolygonObjects){free(out);return 0;}
  name=spe.mesh->object[spe.doc.points[anchor].object].pName;if(!name||!*name||strlen(name)>255){free(out);return 0;}
  shipAuthorWorld(&spe.doc,i,world);shipAuthorWorld(&spe.doc,anchor,parent);memset(local,0,sizeof(local));local[15]=1;
  for(a=0;a<3;a++){for(b=0;b<3;b++)for(k=0;k<3;k++)local[a*4+b]+=parent[b*4+k]*world[a*4+k];for(k=0;k<3;k++)local[12+a]+=parent[a*4+k]*(world[12+k]-parent[12+k]);}
  for(a=0;a<4;a++)for(b=0;b<4;b++)hw[a*4+b]=local[axes[a]*4+axes[b]];
  rotation[1]=asinf(muiClampF(-hw[2],-1,1))*57.29577951f;if(fabsf(hw[2])<.999999f){rotation[0]=atan2f(hw[6],hw[10])*57.29577951f;rotation[2]=atan2f(hw[1],hw[0])*57.29577951f;}else {rotation[0]=atan2f(-hw[9],hw[5])*57.29577951f;rotation[2]=0;}
  used+=sprintf(out+used,"subsystem %s %s %d %s\nmount %s %.9g %.9g %.9g %.9g %.9g %.9g\n",p->name,name,strstr(p->subsystem,"engine")?2:3,p->subsystem,p->name,hw[12]/scale,hw[13]/scale,hw[14]/scale,rotation[0],rotation[1],rotation[2]);
 }
 strcpy(out+used,"# END EDITOR SUBSYSTEMS\n");
 {char root[256],identity[80],error[256];snprintf(root,sizeof(root),"%s/%s/RMNative/",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));snprintf(identity,sizeof(identity),"%s%s",spe.doc.race==R2?"Taiidan_":"",ShipTypeToStr(spe.doc.type));
  if(!meshValidateOpenHWDefinition(root,identity,out,strlen(out),error,sizeof(error))){speStatus(error);free(out);return 0;}}
 if(!speFileParse(out,strlen(out))){free(out);return 0;}free(out);if(!shipAuthorProjectPath(spe.doc.name,"ship.owshipdef",path,sizeof(path),1)||!speFileWrite(path))return 0;
 speFile.ready=0;return 1;
}
static void speAutoSync(void){
 /* A completed drag/numeric edit/copy is one transaction, not a disk write per mouse pixel. */
 if(!spe.mesh||!spe.doc.name[0])return;
 if(speSubsystemsSyncV2(2)!=1)return;
 if(!shipAuthorSave(&spe.doc)||!speSubsystemsSaveDefinition())speStatus("AUTO-SAVE FAILED / PROJECT CHANGES REMAIN IN THE EDITOR");
}
static void spePublishMod(const char*name,int append){
 char path[256],error[256],*root;int result;
 if(speSubsystemsSyncV2(2)!=1){snprintf(modUI.notice,sizeof(modUI.notice),"%s",spe.status);return;}
 if(!shipAuthorSave(&spe.doc)||!speSubsystemsSaveDefinition()){snprintf(modUI.notice,sizeof(modUI.notice),"Project save failed; mod was not published.");return;}
 snprintf(path,sizeof(path),"%s/%s/RMNative/ship.owshipdef",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 root=SDL_GetBasePath();if(!root)return;
 result=openhwModPublish(root,name,append,path,speFile.text.text,strlen(speFile.text.text),0,error,sizeof(error));
 if(result==2&&MessageBoxA(GetActiveWindow(),error,"Append mod: replace conflicting files?",MB_YESNO|MB_ICONWARNING)==IDYES)
  result=openhwModPublish(root,name,append,path,speFile.text.text,strlen(speFile.text.text),1,error,sizeof(error));
 SDL_free(root);snprintf(modUI.notice,sizeof(modUI.notice),"%s",error);
 if(result==1){spe.dirty=0;speStatus(error);mpe.stage=4;}
}
