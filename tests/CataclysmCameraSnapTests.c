#include "Universe.h"
#include "CataclysmCampaign.h"
#include "SinglePlayer.h"
#include "AIVar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
Universe universe;bool32 singlePlayerGame=TRUE;static MissionEnum mission=MISSION_CATA_2_OUTSKIRTS;
static AIVar vars[8];static int count,checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
AIVar *aivarFind(char *key){int i;for(i=0;i<count;i++)if(!strcmp(vars[i].label,key))return &vars[i];return NULL;}
AIVar *aivarCreate(char *key){AIVar *v=&vars[count++];memset(v,0,sizeof(*v));snprintf(v->label,sizeof(v->label),"%s",key);return v;}
sdword aivarValueGet(AIVar *v){return v->value;}void aivarValueSet(AIVar *v,sdword value){v->value=value;}
static void savedSet(const char *key,sdword value){AIVar *v=aivarFind((char*)key);if(!v)v=aivarCreate((char*)key);aivarValueSet(v,value);}
MissionEnum spGetCurrentMission(void){return mission;}
#include "../src/Game/CataclysmCameraSnap.inc"
int main(void)
{
    CameraCommand *camera=&universe.mainCameraCommand,other={0};CameraStackEntry entry={0};
    camera->currentCameraStack=&entry;camera->zoomInCloseAsPossible=TRUE;
    entry.remembercam.lookatpoint.x=70;entry.remembercam.angle=1.25f;entry.remembercam.declination=.5f;entry.remembercam.distance=100;
    CHECK(!cataCampaignCameraConsumeSnap(camera));cataCampaignCameraRequestSnap(650);
    CHECK(!cataCampaignCameraConsumeSnap(&other));CHECK(cataCampaignCameraConsumeSnap(camera));
    CHECK(camera->actualcamera.lookatpoint.x==70&&camera->actualcamera.angle==1.25f&&camera->actualcamera.declination==.5f);
    CHECK(entry.remembercam.distance==650&&!camera->zoomInCloseAsPossible);
    CHECK(!camera->actualcamera.ignoreZoom);CHECK(!cataCampaignCameraConsumeSnap(camera));
    cataCampaignCameraRequestSnap(0);camera->zoomInCloseAsPossible=TRUE;
    CHECK(cataCampaignCameraConsumeSnap(camera)&&camera->zoomInCloseAsPossible&&entry.remembercam.distance==650);
    cataCampaignCameraRequestSnap(2472);mission=MISSION_1_KHARAK_SYSTEM;
    CHECK(!cataCampaignCameraConsumeSnap(camera));mission=MISSION_CATA_2_OUTSKIRTS;
    singlePlayerGame=FALSE;CHECK(!cataCampaignCameraConsumeSnap(camera));singlePlayerGame=TRUE;
    {AIVar saved[8];memcpy(saved,vars,sizeof(vars));memset(vars,0,sizeof(vars));memcpy(vars,saved,sizeof(vars));}
    CHECK(cataCampaignCameraConsumeSnap(camera)&&entry.remembercam.distance==2472);
    printf("PASS: %d original Cataclysm camera snap and HW1 isolation checks\n",checks);return 0;
}
