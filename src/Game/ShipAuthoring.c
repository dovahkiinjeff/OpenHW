#include "ShipAuthoring.h"
#include "RaceDefs.h"
#include "../Ships/ShipDefs.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <windows.h>
int shipAuthorParent(const ShipAuthorDocument*d,unsigned index){unsigned j;if(index>=d->pointCount)return -1;for(j=0;j<d->pointCount;j++)if(!strcmp(d->points[index].parent,d->points[j].name))return j;return -1;}
int shipAuthorCategory(const ShipAuthorPoint*p){char name[64];unsigned i;
 if(p->kind!=SAP_JOINT)return p->kind;
 for(i=0;i<63&&p->name[i];i++)name[i]=(char)tolower((unsigned char)p->name[i]);name[i]=0;
 if(strstr(name,"turret")||strstr(name,"latitude")||strstr(name,"longitude"))return SAP_TURRET;
 if(strstr(name,"muzzle")||strstr(name,"weapon")||strstr(name,"gun"))return SAP_WEAPON;
 if(strstr(name,"salvage"))return SAP_SALVAGE;
 if(strstr(name,"repair"))return SAP_REPAIR;
 if(strstr(name,"dock")||strstr(name,"launch"))return SAP_DOCK;
 if(strstr(name,"engine")||strstr(name,"glow")||strstr(name,"burn")||strstr(name,"trail"))return SAP_ENGINE;
 if(strstr(name,"nav")||strstr(name,"light"))return SAP_NAV;
 return SAP_JOINT;
}
static void authorLocal(const ShipAuthorPoint*p,float*m){float x=p->rotation[0]*.01745329252f,y=p->rotation[1]*.01745329252f,z=p->rotation[2]*.01745329252f;
 float cx=cosf(x),sx=sinf(x),cy=cosf(y),sy=sinf(y),cz=cosf(z),sz=sinf(z);memset(m,0,16*sizeof(float));
 m[0]=cz*cy;m[1]=sz*cy;m[2]=-sy;m[4]=cz*sy*sx-sz*cx;m[5]=sz*sy*sx+cz*cx;m[6]=cy*sx;m[8]=cz*sy*cx+sz*sx;m[9]=sz*sy*cx-cz*sx;m[10]=cy*cx;
 memcpy(m+12,p->position,3*sizeof(float));m[15]=1;
}
static void authorWorld(const ShipAuthorDocument*d,unsigned i,float*m,unsigned depth){float local[16],parent[16];int p,a,b,k;authorLocal(&d->points[i],local);p=shipAuthorParent(d,i);
 if(p<0||depth>=d->pointCount){memcpy(m,local,sizeof(local));return;}authorWorld(d,p,parent,depth+1);
 for(a=0;a<4;a++)for(b=0;b<4;b++){m[a*4+b]=0;for(k=0;k<4;k++)m[a*4+b]+=parent[k*4+b]*local[a*4+k];}
}
void shipAuthorWorld(const ShipAuthorDocument*d,unsigned i,float*m){memset(m,0,16*sizeof(float));m[0]=m[5]=m[10]=m[15]=1;if(d&&i<d->pointCount)authorWorld(d,i,m,0);}
int shipAuthorReparent(ShipAuthorDocument*d,unsigned i,int parent){float world[16],base[16],local[16];ShipAuthorPoint old;int a,b,k,p=parent;unsigned depth=0;
 if(!d||i>=d->pointCount||parent< -1||parent>=(int)d->pointCount)return 0;
 while(p>=0){if(p==(int)i||depth++>=d->pointCount)return 0;p=shipAuthorParent(d,p);}
 shipAuthorWorld(d,i,world);shipAuthorWorld(d,parent<0?d->pointCount:(unsigned)parent,base);memset(local,0,sizeof(local));
 for(a=0;a<3;a++){for(b=0;b<3;b++)for(k=0;k<3;k++)local[a*4+b]+=base[b*4+k]*world[a*4+k];for(k=0;k<3;k++)local[12+a]+=base[a*4+k]*(world[12+k]-base[12+k]);}
 old=d->points[i];memcpy(d->points[i].position,local+12,3*sizeof(float));
 d->points[i].rotation[1]=asinf(fmaxf(-1,fminf(1,-local[2])))*57.29577951f;
 if(fabsf(local[2])<.999999f){d->points[i].rotation[0]=atan2f(local[6],local[10])*57.29577951f;d->points[i].rotation[2]=atan2f(local[1],local[0])*57.29577951f;}
 else {d->points[i].rotation[0]=atan2f(-local[9],local[5])*57.29577951f;d->points[i].rotation[2]=0;}
 snprintf(d->points[i].parent,64,"%s",parent<0?"-":d->points[parent].name);
 if(!shipAuthorDocumentValid(d)){d->points[i]=old;return 0;}return 1;
}
int shipAuthorRotateLocal(ShipAuthorDocument*d,unsigned i,int axis,float degrees){float m[16],r[16],out[16];ShipAuthorPoint turn={0};int a,b,k;
 if(!d||i>=d->pointCount||axis<0||axis>2||!isfinite(degrees))return 0;turn.rotation[axis]=degrees;authorLocal(&d->points[i],m);authorLocal(&turn,r);
 for(a=0;a<3;a++)for(b=0;b<3;b++){out[a*4+b]=0;for(k=0;k<3;k++)out[a*4+b]+=m[k*4+b]*r[a*4+k];}
 d->points[i].rotation[1]=asinf(fmaxf(-1,fminf(1,-out[2])))*57.29577951f;
 if(fabsf(out[2])<.999999f){d->points[i].rotation[0]=atan2f(out[6],out[10])*57.29577951f;d->points[i].rotation[2]=atan2f(out[1],out[0])*57.29577951f;}
 else {d->points[i].rotation[0]=atan2f(-out[9],out[5])*57.29577951f;d->points[i].rotation[2]=0;}return 1;
}
int shipAuthorNameValid(const char *s){
 size_t i,n;if(!s || !(n=strlen(s)) || n>47)return 0;
 for(i=0;i<n;i++)if(!isalnum((unsigned char)s[i]) && s[i]!='_' && s[i]!='-')return 0;
 if(!_stricmp(s,"CON")||!_stricmp(s,"PRN")||!_stricmp(s,"AUX")||!_stricmp(s,"NUL"))return 0;
 if(n==4 && (!_strnicmp(s,"COM",3)||!_strnicmp(s,"LPT",3)) && s[3]>='0' && s[3]<='9')return 0;
 return 1;
}
static int token(const char*s,size_t max){size_t i,n;if(!s || !(n=strnlen(s,max)) || n>=max)return 0;for(i=0;i<n;i++)if((unsigned char)s[i]<=32 || (unsigned char)s[i]>126)return 0;return 1;}
static int relativeValid(const char*s){size_t i,n;if(!s || !(n=strnlen(s,128)) || n>=128 || s[0]=='/' || s[0]=='\\' || strstr(s,".."))return 0;
 for(i=0;i<n;i++)if(!isalnum((unsigned char)s[i]) && s[i]!='_' && s[i]!='-' && s[i]!='.' && s[i]!='/')return 0;return 1;}
int shipAuthorDocumentValid(const ShipAuthorDocument*d){
 unsigned i,j;int k;if(!d || d->race<0||d->race>=NUM_RACES||d->type<0||d->type>=TOTAL_NUM_SHIPS||strnlen(d->name,48)>=48||!shipAuthorNameValid(d->name)||d->pointCount>SHIP_AUTHOR_POINTS||d->materialCount>SHIP_AUTHOR_MATERIALS||!isfinite(d->scale)||d->scale<=0||d->scale>100000)return 0;
 if(d->model[0]&&!relativeValid(d->model))return 0;
 for(i=0;i<d->pointCount;i++){const ShipAuthorPoint*p=&d->points[i];
  if(!token(p->name,64)||!token(p->parent,64)||p->kind<0||p->kind>SAP_SUBSYSTEM||(p->enabled!=0&&p->enabled!=1)||p->object< -1||p->object>511||p->clone<0||p->clone>1000000||!isfinite(p->size)||p->size<0||p->size>100000)return 0;
  if(p->subsystem[0]) {if(p->kind!=SAP_SUBSYSTEM||!token(p->subsystem,64)||p->object!=-1)return 0;for(k=0;p->subsystem[k];k++)if(!isalnum((unsigned char)p->subsystem[k])&&p->subsystem[k]!='_')return 0;}
  else if(p->kind==SAP_SUBSYSTEM)return 0;
  if(p->subsystemRole<0||p->subsystemRole>3)return 0;
  for(k=0;k<3;k++)if(!isfinite(p->position[k])||fabsf(p->position[k])>1000000||!isfinite(p->rotation[k])||fabsf(p->rotation[k])>360||!isfinite(p->color[k])||p->color[k]<0||p->color[k]>16)return 0;
  for(j=0;j<i;j++)if(!strcmp(p->name,d->points[j].name))return 0;
 }
 /* Imported names refer to real parent joints; user parents must exist in the
    document, and no chain may cycle. A dash denotes ship-space/root. */
 for(i=0;i<d->pointCount;i++){unsigned current=i,steps=0;
  while(strcmp(d->points[current].parent,"-")){
   for(j=0;j<d->pointCount;j++)if(!strcmp(d->points[current].parent,d->points[j].name))break;
   if(j==d->pointCount||j==i||++steps>d->pointCount)return 0;current=j;
  }
 }
 for(i=0;i<d->materialCount;i++){if(!token(d->materials[i].name,64))return 0;for(k=0;k<3;k++)if(d->materials[i].texture[k][0]&&!relativeValid(d->materials[i].texture[k]))return 0;}
 return 1;
}
int shipAuthorProjectPath(const char*name,const char*relative,char*out,size_t bytes,int create){
 char*base;char directory[1024];int n;if(!shipAuthorNameValid(name)||!relativeValid(relative)||!out)return 0;
 base=SDL_GetBasePath();if(!base)return 0;
 if(create){snprintf(directory,sizeof(directory),"%sModding",base);if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){SDL_free(base);return 0;}
  snprintf(directory,sizeof(directory),"%sModding/Ships",base);if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){SDL_free(base);return 0;}
  snprintf(directory,sizeof(directory),"%sModding/Ships/%s",base,name);if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){SDL_free(base);return 0;}
  snprintf(directory,sizeof(directory),"%sModding/Ships/%s/assets",base,name);if(!CreateDirectoryA(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){SDL_free(base);return 0;}}
 n=snprintf(out,bytes,"%sModding/Ships/%s/%s",base,name,relative);SDL_free(base);return n>0&&(size_t)n<bytes;
}
int shipAuthorImportFile(const char*name,const char*source,const char*relative){
 char target[1024],temp[1050];LARGE_INTEGER length;HANDLE f;if(!source||!shipAuthorProjectPath(name,relative,target,sizeof(target),1))return 0;
 f=CreateFileA(source,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
 if(!GetFileSizeEx(f,&length)||length.QuadPart<=0||length.QuadPart>256*1024*1024){CloseHandle(f);return 0;}CloseHandle(f);
 if(!_stricmp(source,target))return 1;snprintf(temp,sizeof(temp),"%s.import",target);
 if(!CopyFileA(source,temp,FALSE))return 0;
 return MoveFileExA(temp,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
int shipAuthorCopyAssets(const ShipAuthorDocument*d,const char*oldName){
 unsigned i;int k;char source[1024];if(!strcmp(oldName,d->name))return 1;
 if(d->model[0]&&(!shipAuthorProjectPath(oldName,d->model,source,sizeof(source),0)||!shipAuthorImportFile(d->name,source,d->model)))return 0;
 for(i=0;i<d->materialCount;i++)for(k=0;k<3;k++){const char*r=d->materials[i].texture[k];if(r[0]&&(!shipAuthorProjectPath(oldName,r,source,sizeof(source),0)||!shipAuthorImportFile(d->name,source,r)))return 0;}
 for(k=0;k<2;k++){const char*definition=k?"ship.owshipdef":"gameplay.shp";
 if(shipAuthorProjectPath(oldName,definition,source,sizeof(source),0)&&GetFileAttributesA(source)!=INVALID_FILE_ATTRIBUTES){char target[1024];
  if(!shipAuthorProjectPath(d->name,definition,target,sizeof(target),1))return 0;
  /* Preserve an existing destination project's definition during Save As. */
  if(GetFileAttributesA(target)==INVALID_FILE_ATTRIBUTES&&!shipAuthorImportFile(d->name,source,definition))return 0;
 }}return 1;
}
int shipAuthorSave(const ShipAuthorDocument*d){
 char path[1024],temp[1050],backup[1050];FILE*f;unsigned i;int ok=1;
 if(!shipAuthorDocumentValid(d)||!shipAuthorProjectPath(d->name,"ship.owship",path,sizeof(path),1))return 0;
 snprintf(temp,sizeof(temp),"%s.tmp",path);f=fopen(temp,"wb");if(!f)return 0;
 if(fprintf(f,"OPENHW_SHIP 1\nSOURCE %d %d %.9g %s\n",d->race,d->type,d->scale,d->model[0]?d->model:"-")<0)ok=0;
 for(i=0;i<d->materialCount;i++){const ShipAuthorMaterial*m=&d->materials[i];if(fprintf(f,"MATERIAL %s %s %s %s\n",m->name,m->texture[0][0]?m->texture[0]:"-",m->texture[1][0]?m->texture[1]:"-",m->texture[2][0]?m->texture[2]:"-")<0)ok=0;}
 for(i=0;i<d->pointCount;i++){const ShipAuthorPoint*p=&d->points[i];if(fprintf(f,"POINT %d %d %d %d %s %s %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",p->kind,p->enabled,p->object,p->clone,p->name,p->parent,p->position[0],p->position[1],p->position[2],p->rotation[0],p->rotation[1],p->rotation[2],p->color[0],p->color[1],p->color[2],p->size)<0)ok=0;}
 for(i=0;i<d->pointCount;i++)if(d->points[i].subsystem[0])if(fprintf(f,"SUBSYSTEM %s %s\n",d->points[i].name,d->points[i].subsystem)<0)ok=0;
 for(i=0;i<d->pointCount;i++)if(d->points[i].subsystem[0])if(fprintf(f,"SUBSYSTEM_ROLE %s %d\n",d->points[i].name,d->points[i].subsystemRole)<0)ok=0;
 if(fclose(f)!=0)ok=0;if(!ok){remove(temp);return 0;}
 if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES){snprintf(backup,sizeof(backup),"%s.bak-%llu",path,(unsigned long long)SDL_GetPerformanceCounter());if(!CopyFileA(path,backup,TRUE)){remove(temp);return 0;}}
 return MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
int shipAuthorLoad(const char*name,ShipAuthorDocument*out){
 ShipAuthorDocument*d;char path[1024],line[1024],tag[32],tail;FILE*f;int source=0,ok=1,version;
 if(!out||!shipAuthorProjectPath(name,"ship.owship",path,sizeof(path),0))return 0;f=fopen(path,"rb");if(!f)return 0;
 d=(ShipAuthorDocument*)calloc(1,sizeof(*d));if(!d){fclose(f);return 0;}snprintf(d->name,sizeof(d->name),"%s",name);
 if(!fgets(line,sizeof(line),f)||sscanf(line,"OPENHW_SHIP %d %c",&version,&tail)!=1||version!=1)ok=0;
 while(ok&&fgets(line,sizeof(line),f)){
  if(!strchr(line,'\n')&&!feof(f)){ok=0;break;}if(sscanf(line,"%31s",tag)!=1||tag[0]=='#')continue;
  if(!strcmp(tag,"SOURCE")){if(source++||sscanf(line,"SOURCE %d %d %f %127s %c",&d->race,&d->type,&d->scale,d->model,&tail)!=4)ok=0;if(!strcmp(d->model,"-"))d->model[0]=0;}
  else if(!strcmp(tag,"MATERIAL")){ShipAuthorMaterial*m;if(d->materialCount>=SHIP_AUTHOR_MATERIALS){ok=0;break;}m=&d->materials[d->materialCount++];if(sscanf(line,"MATERIAL %63s %127s %127s %127s %c",m->name,m->texture[0],m->texture[1],m->texture[2],&tail)!=4)ok=0;{int k;for(k=0;k<3;k++)if(!strcmp(m->texture[k],"-"))m->texture[k][0]=0;}}
  else if(!strcmp(tag,"POINT")){ShipAuthorPoint*p;if(d->pointCount>=SHIP_AUTHOR_POINTS){ok=0;break;}p=&d->points[d->pointCount++];if(sscanf(line,"POINT %d %d %d %d %63s %63s %f %f %f %f %f %f %f %f %f %f %c",&p->kind,&p->enabled,&p->object,&p->clone,p->name,p->parent,&p->position[0],&p->position[1],&p->position[2],&p->rotation[0],&p->rotation[1],&p->rotation[2],&p->color[0],&p->color[1],&p->color[2],&p->size,&tail)!=16)ok=0;}
  else if(!strcmp(tag,"SUBSYSTEM")){char slot[64],type[64];unsigned i;if(sscanf(line,"SUBSYSTEM %63s %63s %c",slot,type,&tail)!=2){ok=0;break;}for(i=0;i<d->pointCount;i++)if(!strcmp(slot,d->points[i].name))break;if(i==d->pointCount||d->points[i].subsystem[0])ok=0;else snprintf(d->points[i].subsystem,64,"%s",type);}
  else if(!strcmp(tag,"SUBSYSTEM_ROLE")){char slot[64];unsigned i;int role;if(sscanf(line,"SUBSYSTEM_ROLE %63s %d %c",slot,&role,&tail)!=2||role<0||role>3){ok=0;break;}for(i=0;i<d->pointCount;i++)if(!strcmp(slot,d->points[i].name))break;if(i==d->pointCount||!d->points[i].subsystem[0])ok=0;else d->points[i].subsystemRole=role;}
  else ok=0;
 }
 if(ferror(f)||source!=1)ok=0;fclose(f);if(ok&&shipAuthorDocumentValid(d))*out=*d;else ok=0;free(d);return ok;
}
int shipAuthorCopySelection(const ShipAuthorDocument*d,unsigned selected,ShipAuthorDocument*clip){
 unsigned i,j,current,steps;if(!clip||!shipAuthorDocumentValid(d)||selected>=d->pointCount)return 0;
 *clip=*d;clip->pointCount=1;clip->points[0]=d->points[selected];
 for(i=0;i<d->pointCount;i++)if(i!=selected){current=i;steps=0;
  while(strcmp(d->points[current].parent,"-")&&steps++<d->pointCount){
   for(j=0;j<d->pointCount;j++)if(!strcmp(d->points[current].parent,d->points[j].name))break;
   if(j==d->pointCount)break;if(j==selected){clip->points[clip->pointCount++]=d->points[i];break;}current=j;
  }
 }return 1;
}
int shipAuthorPaste(ShipAuthorDocument*d,const ShipAuthorDocument*clip,float offset,int*selected){
 ShipAuthorDocument*candidate;unsigned i,j,k,group=1,start;char names[SHIP_AUTHOR_POINTS][64];int ok=1;
 if(!d||!clip||!selected||!shipAuthorDocumentValid(d)||!clip->pointCount||clip->pointCount>SHIP_AUTHOR_POINTS||d->pointCount+clip->pointCount>SHIP_AUTHOR_POINTS||!isfinite(offset))return 0;
 for(i=0;i<clip->pointCount;i++)if(clip->points[i].object>=0&&(d->race!=clip->race||d->type!=clip->type||strcmp(d->model,clip->model)))return 0;
 for(i=0;i<d->pointCount;i++)if((unsigned)d->points[i].clone>=group)group=d->points[i].clone+1;
 /* Names are unique even after prior copies were deleted or renamed. */
 for(;;){int collision=0;if(group>1000000)return 0;
  for(i=0;i<clip->pointCount;i++){if(clip->points[0].subsystem[0]){if(!i)snprintf(names[i],64,"%.40s_Copy%u",clip->points[0].name,group);else snprintf(names[i],64,"%.32s_Copy%u_Joint%u",clip->points[0].name,group,i);}else snprintf(names[i],64,"copy_%u_%u",group,i);for(j=0;j<d->pointCount;j++)if(!strcmp(names[i],d->points[j].name))collision=1;}
  if(!collision)break;group++;
 }
 candidate=(ShipAuthorDocument*)malloc(sizeof(*candidate));if(!candidate)return 0;*candidate=*d;start=candidate->pointCount;
 for(i=0;i<clip->pointCount;i++){ShipAuthorPoint*p=&candidate->points[candidate->pointCount++];*p=clip->points[i];p->kind=shipAuthorCategory(p);p->clone=group;snprintf(p->name,64,"%s",names[i]);
  for(k=0;k<clip->pointCount;k++)if(!strcmp(clip->points[i].parent,clip->points[k].name))break;
  if(k<clip->pointCount)snprintf(p->parent,64,"%s",names[k]);
  else {for(j=0;j<d->pointCount;j++)if(!strcmp(p->parent,d->points[j].name))break;if(j==d->pointCount)strcpy(p->parent,"-");}
 }
 candidate->points[start].position[0]+=offset;if(!shipAuthorDocumentValid(candidate))ok=0;
 if(ok){*d=*candidate;*selected=start;}free(candidate);return ok;
}
