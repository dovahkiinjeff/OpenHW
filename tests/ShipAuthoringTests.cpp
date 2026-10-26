#include "ShipAuthoringObj.h"
extern "C" {
#include "ShipAuthoring.h"
#include "ShipAuthorText.h"
}
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>
static void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
int main()try {
 const char *obj="v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nusemtl hull\nf -4/1 -3/2 -2/3 -1/4\n";
 HwAuthoringObj mesh={};char error[256];require(hwAuthoringObjRead(obj,strlen(obj),&mesh,error,sizeof(error)),error);
 require(mesh.vertexCount==6&&mesh.materialCount==2,"quad triangulation/material grouping");require(std::fabs(mesh.vertices[0].normal[2]-1)<1e-5,"generated normals");hwAuthoringObjFree(&mesh);
 for(auto bad:{"v 0 0 0\nf 1 2 3\n","v nan 0 0\n","v 0 0 0\nf 1 1 1\n","v 0 0 0\nf 0 1 1\n"})require(!hwAuthoringObjRead(bad,strlen(bad),&mesh,error,sizeof(error))&&!mesh.vertices,"invalid OBJ rejected without partial output");
 static ShipAuthorDocument doc={};snprintf(doc.name,48,"AuthoringTest-%lu",GetTickCount());doc.scale=1;doc.race=0;doc.type=0;doc.pointCount=2;
 auto&p=doc.points[0];p.kind=SAP_NAV;p.enabled=1;p.object=-1;p.size=10;p.color[0]=1;strcpy(p.name,"nav_0");strcpy(p.parent,"-");
 doc.points[1]=p;strcpy(doc.points[1].name,"weapon_0");strcpy(doc.points[1].parent,"nav_0");doc.points[1].kind=SAP_WEAPON;doc.points[1].position[0]=15;
 require(shipAuthorDocumentValid(&doc),"valid authoring document");require(shipAuthorSave(&doc),"atomic project save");
 static ShipAuthorDocument loaded={};require(shipAuthorLoad(doc.name,&loaded),"project reload");require(loaded.pointCount==2&&loaded.points[1].position[0]==15&&loaded.points[0].object==-1,"project round trip");
 static ShipAuthorDocument clipboard={};int pasted=-1;
 static ShipAuthorDocument subsystemDoc;subsystemDoc=doc;subsystemDoc.points[1].kind=SAP_SUBSYSTEM;subsystemDoc.points[1].subsystemRole=2;strcpy(subsystemDoc.points[1].subsystem,"kus_assaultgun");
 require(shipAuthorDocumentValid(&subsystemDoc)&&shipAuthorSave(&subsystemDoc),"save native subsystem placement");
 static ShipAuthorDocument subsystemReload={};require(shipAuthorLoad(subsystemDoc.name,&subsystemReload),"reload native subsystem placement");
 require(subsystemReload.points[1].subsystemRole==2,"subsystem role retained on reload");
 require(subsystemReload.points[1].kind==SAP_SUBSYSTEM&&!strcmp(subsystemReload.points[1].subsystem,"kus_assaultgun"),"subsystem type belongs to the ship document");
 require(shipAuthorCopySelection(&subsystemReload,1,&clipboard)&&shipAuthorPaste(&subsystemReload,&clipboard,20,&pasted),"copy and paste subsystem placement");
 require(!strcmp(subsystemReload.points[pasted].subsystem,"kus_assaultgun"),"copied subsystem retains shared asset identity");
 strcpy(subsystemReload.points[1].subsystem,"../unsafe");require(!shipAuthorDocumentValid(&subsystemReload),"reject subsystem traversal");
 require(shipAuthorCopySelection(&loaded,0,&clipboard)&&clipboard.pointCount==2,"copy selected subtree");
 require(shipAuthorPaste(&loaded,&clipboard,20,&pasted),"paste subtree");
 require(pasted==2&&loaded.pointCount==4&&loaded.points[2].position[0]==20,"separate pasted transform");
 require(!strcmp(loaded.points[3].parent,loaded.points[2].name)&&loaded.points[2].clone>0,"copied child links remapped");
 require(loaded.points[0].position[0]==0&&!strcmp(loaded.points[1].parent,"nav_0"),"original unchanged");
 require(shipAuthorPaste(&loaded,&clipboard,20,&pasted)&&loaded.pointCount==6,"repeated paste has unique names");
 require(shipAuthorSave(&loaded),"overwrite with backup");
 static ShipAuthorDocument rig={};strcpy(rig.name,"RigTest");rig.scale=1;rig.pointCount=4;
 for(int i=0;i<4;i++){auto&q=rig.points[i];q.kind=SAP_JOINT;q.enabled=1;q.object=i;q.size=1;snprintf(q.name,64,"joint_%d",i);if(i)snprintf(q.parent,64,"joint_%d",i-1);else strcpy(q.parent,"-");}
 rig.points[0].rotation[2]=90;rig.points[1].rotation[0]=90;rig.points[2].position[1]=10;rig.points[3].position[0]=5;
 float before[16],after[16];shipAuthorWorld(&rig,3,before);
 require(std::fabs(before[12])<1e-4&&std::fabs(before[13]-5)<1e-4&&std::fabs(before[14]-10)<1e-4,"nested world transform includes all ancestor rotations");
 require(shipAuthorReparent(&rig,2,0),"reparent assembly to hull");shipAuthorWorld(&rig,3,after);
 for(int i=0;i<16;i++)require(std::fabs(before[i]-after[i])<1e-4,"reparent preserves descendant world pose");
 require(!shipAuthorReparent(&rig,0,3),"reject reparent into descendant");
 require(shipAuthorReparent(&rig,2,-1),"detach assembly to world");shipAuthorWorld(&rig,3,after);
 for(int i=0;i<16;i++)require(std::fabs(before[i]-after[i])<1e-4,"detached assembly retains pose");
 strcpy(rig.points[2].name,"Turret_Base");strcpy(rig.points[3].parent,"Turret_Base");
 require(shipAuthorCategory(&rig.points[2])==SAP_TURRET,"automatic turret classification");
 require(shipAuthorCopySelection(&rig,2,&clipboard)&&clipboard.pointCount==2,"copy turret includes child geometry");
 require(shipAuthorPaste(&rig,&clipboard,20,&pasted)&&rig.points[pasted].kind==SAP_TURRET,"copy retains system category after renaming");
 require(rig.points[pasted+1].object==3&&!strcmp(rig.points[pasted+1].parent,rig.points[pasted].name),"copied geometry retains new parent chain");
 static ShipAuthorDocument pivot={};strcpy(pivot.name,"PivotTest");pivot.scale=1;pivot.pointCount=3;
 for(int i=0;i<3;i++){pivot.points[i].object=i;pivot.points[i].enabled=1;snprintf(pivot.points[i].name,64,"p%d",i);if(i)snprintf(pivot.points[i].parent,64,"p%d",i-1);else strcpy(pivot.points[i].parent,"-");}
 pivot.points[0].position[0]=100;pivot.points[1].position[0]=10;pivot.points[2].position[0]=5;
 require(shipAuthorRotateLocal(&pivot,0,2,90),"rotate gun base");shipAuthorWorld(&pivot,2,after);
 require(std::fabs(after[12]-100)<1e-4&&std::fabs(after[13]-15)<1e-4,"all descendants rotate around the base pivot");
 require(pivot.points[1].position[0]==10&&pivot.points[2].position[0]==5,"local child offsets unchanged by base rotation");
 pivot.points[0].position[2]=20;shipAuthorWorld(&pivot,2,after);require(std::fabs(after[14]-20)<1e-4,"base translation carries all descendants");
 ShipAuthorText text={};require(shipTextLoad(&text,"first\r\nsecond\r\nthird",20),"load multiline text");
 require(!strcmp(text.text,"first\nsecond\nthird"),"normalize CRLF");text.anchor=2;text.cursor=14;
 require(shipTextInsert(&text,"A\r\nB",4),"replace selection spanning lines");require(!strcmp(text.text,"fiA\nBhird"),"multiline replacement retains unselected suffix");
 require(shipTextUndo(&text,0)&&!strcmp(text.text,"first\nsecond\nthird"),"undo multiline replacement");
 require(shipTextUndo(&text,1)&&!strcmp(text.text,"fiA\nBhird"),"redo multiline replacement");
 text.anchor=0;text.cursor=strlen(text.text);require(shipTextInsert(&text,"",0)&&!*text.text,"delete full selection");shipTextFree(&text);
 strcpy(doc.points[0].parent,"weapon_0");require(!shipAuthorDocumentValid(&doc),"parent cycle rejected");strcpy(doc.points[0].parent,"-");
 strcpy(doc.model,"../private.obj");require(!shipAuthorDocumentValid(&doc),"traversal rejected");doc.model[0]=0;
 doc.points[0].size=std::nanf("");require(!shipAuthorDocumentValid(&doc),"nonfinite transform rejected");
 require(!shipAuthorNameValid("../outside")&&!shipAuthorNameValid("CON")&&!shipAuthorNameValid("COM1"),"unsafe project names rejected");
 std::cout<<"PASS: OBJ, persistence, nested transforms, world-preserving reparenting, cycle rejection and assembly copying\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
