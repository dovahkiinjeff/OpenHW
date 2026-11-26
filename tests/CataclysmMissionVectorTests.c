#include "Universe.h"
#include "AIUtilities.h"
#include "Sensors.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
real32 smUniverseSizeX=100,smUniverseSizeY=200,smUniverseSizeZ=300;
static int checks,draws;static vector sampled;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void cataScriptFault(const char *message){fprintf(stderr,"%s\n",message);exit(1);}
vector aiuGenerateRandomStandoffPoint(vector center,real32 radius,vector origin,udword flags)
{CHECK(flags==RSP_NORMAL&&origin.x==0&&origin.y==0&&origin.z==0);CHECK(radius==50);++draws;sampled=center;center.x+=radius;return center;}
#include "../src/Game/CataclysmMissionVectors.inc"
int main(void)
{
    hvector a={-100,-200,-300,1},b={100,200,300,1},out={0,0,0,42};
    cataScriptVectorBlend(&out,&a,&b,0,10);CHECK(out.x==-100&&out.y==-200&&out.z==-300&&out.w==42);
    cataScriptVectorBlend(&out,&a,&b,5,10);CHECK(out.x==0&&out.y==0&&out.z==0);
    cataScriptVectorBlend(&out,&a,&b,10,10);CHECK(out.x==100&&out.y==200&&out.z==300);
    cataScriptVectorBlend(&out,&a,&b,20,10);CHECK(out.x==100&&out.y==200&&out.z==300);
    cataScriptVectorBlend(&a,&a,&b,5,10);CHECK(a.x==0&&a.y==0&&a.z==0);
    a.x=80;a.y=250;a.z=-400;cataScriptVectorRandom(&out,&a,50,1);CHECK(out.x==100&&out.y==200&&out.z==-300&&out.w==1);
    cataScriptVectorRandom(&a,&a,50,.5f);CHECK(sampled.x==80&&a.x==50&&a.y==100&&a.z==-150);
    CHECK(draws==2);printf("PASS: %d original mission vector checks\n",checks);return 0;
}
