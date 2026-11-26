#include "CataclysmNPCTrack.h"
#include "Universe.h"
#include "Physics.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
Universe universe;
real32 ERROR_VELOCITY_SQUARED=.05f,STILL_VELOCITY_LO=-3,STILL_VELOCITY_HI=3,STILL_ROT_LO=-.3f,STILL_ROT_HI=.3f;
real32 STABILIZE_Z_LO=-.1f,STABILIZE_Z_HI=.1f,AITRACKRIGHT_UPACCURACY=.985f;
static int forces;
void dbgFatalf(char *file,sdword line,char *format,...){(void)file;(void)line;fprintf(stderr,"%s\n",format);exit(1);}
void physApplyRotToObj(SpaceObjRot *object,real32 magnitude,uword direction){(void)object;(void)magnitude;(void)direction;++forces;}
void physApplyForceToObj(SpaceObj *object,real32 magnitude,uword direction){(void)object;(void)magnitude;(void)direction;++forces;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;}} while(0)
int main(void)
{
    Ship ship={0};ShipStaticInfo info={0};vector zero={0,0,0},forward={0,0,1};
    ship.objtype=OBJ_ShipType;ship.staticinfo=&info;
    ship.nonstatvars.rotstrength[ROT_PITCHUP]=1;
    ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m22=ship.rotinfo.coordsys.m33=1;
    universe.phystimeelapsed=.1f;
    ship.posinfo.velocity.x=50;ship.rotinfo.rotspeed.y=2;
    CHECK(cataNpc_aitrackHeadingFunc((SpaceObjRotImpTargGuidance*)&ship,&zero,.99f,.99f,0,0));
    CHECK(ship.posinfo.velocity.x==50&&ship.rotinfo.rotspeed.y==2&&forces==0);
    CHECK(cataNpc_aitrackHeadingFunc((SpaceObjRotImpTargGuidance*)&ship,&forward,.99f,.99f,0,AITRACKHEADING_IGNOREUPVEC));
    CHECK(ship.posinfo.velocity.x==50&&ship.rotinfo.rotspeed.y==0);
    ship.flags=SOF_Crazy;ship.rotinfo.rotspeed.y=2;
    cataNpc_aitrackForceSteadyShip(&ship);
    CHECK(ship.posinfo.velocity.x==50&&ship.rotinfo.rotspeed.y==2);
    ship.flags=0;cataNpc_aitrackForceSteadyShip(&ship);
    CHECK(ship.posinfo.velocity.x==0&&ship.rotinfo.rotspeed.y==0);
    CHECK(cataNpc_aitrackSteadyShipAnywhere(&ship));
    ship.posinfo.velocity.x=50;ship.rotinfo.rotspeed.y=2;
    cataNpc_aitrackForceZeroRotation(&ship);
    CHECK(ship.posinfo.velocity.x==50&&ship.rotinfo.rotspeed.y==0);
    ship.rotinfo.rotspeed.y=2;
    cataNpc_aitrackForceGuidanceZeroRotation((SpaceObjRotImpTargGuidance*)&ship);
    CHECK(ship.posinfo.velocity.x==0&&ship.rotinfo.rotspeed.y==2);
    puts("PASS: Cataclysm NPC guidance handles zero headings, preserves velocity while aiming, respects careening and settles independently of gun reset.");return 0;
}
