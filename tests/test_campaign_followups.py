from pathlib import Path
import unittest
from test_campaign_report_fixes import run_c
ROOT=Path(__file__).resolve().parents[1]
def function(path,name):
 s=(ROOT/path).read_text();a=s.index(name+'(');a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;i=b+1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
class Followups(unittest.TestCase):
 def test_movie_tail_only_waits_for_real_animatic_speech(self):
  source=function('src/Game/SpeechEvent.c','speechEventAnimaticPending')
  run_c(r'''
#include <assert.h>
#include <string.h>
typedef int bool32,sdword;
#define TRUE 1
#define FALSE 0
#define SE_NUM_ACTORS 2
#define SE_MAX_QUEUE 3
#define SOUND_PLAYING 2
#define SOUND_OK 0
#define SPEECH_TYPE_ANIMATIC 0x40000000
struct Event {int handle,event;};
struct Queue {int status;struct Event current,queue[SE_MAX_QUEUE];} speechqueue[SE_NUM_ACTORS];
''',source,r'''
int main(void){
 int i,j;for(i=0;i<2;i++)for(j=0;j<3;j++)speechqueue[i].queue[j].handle=-1;
 assert(!speechEventAnimaticPending());speechqueue[1].status=SOUND_PLAYING;
 speechqueue[1].current.event=SPEECH_TYPE_ANIMATIC;assert(speechEventAnimaticPending());
 speechqueue[1].status=0;assert(!speechEventAnimaticPending());
 speechqueue[0].queue[2]=(struct Event){123,SPEECH_TYPE_ANIMATIC};assert(speechEventAnimaticPending());
 speechqueue[0].queue[2].handle=-1;assert(!speechEventAnimaticPending());
 speechqueue[0].queue[0]=(struct Event){123,1};assert(!speechEventAnimaticPending());return 0;
}
''')
 def test_fuelpod_uses_small_bay_but_large_ship_flags_remain(self):
  source=function('src/Game/Dock.c','shipStaticIndicatesBigHangerBayForRace')
  run_c(r'''
#include <assert.h>
#include <stddef.h>
typedef int bool32,sdword;
#define FALSE 0
#define P2FuelPod 15
#define R1 0
#define R2 1
struct Salvage {int needBigR1,needBigR2;};
typedef struct ShipStaticInfo {int shiptype;struct Salvage *salvageStaticInfo;} ShipStaticInfo;
''',source,r'''
int main(void){struct Salvage salvage={1,1};ShipStaticInfo pod={P2FuelPod,&salvage};
 assert(!shipStaticIndicatesBigHangerBayForRace(&pod,R1));assert(!shipStaticIndicatesBigHangerBayForRace(&pod,R2));
 pod.shiptype=99;assert(shipStaticIndicatesBigHangerBayForRace(&pod,R1));assert(shipStaticIndicatesBigHangerBayForRace(&pod,R2));
 assert(!shipStaticIndicatesBigHangerBayForRace(NULL,R1));return 0;}
''')
 def test_rollcall_recovers_idle_captured_fighters_without_overriding_busy_ships(self):
  source=function('src/Game/SinglePlayer.c','spRetryStrandedRollCallShips')
  run_c(r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef float real32;typedef int sdword;
#define COMMAND_NULL 0
#define DOCK_PERMANENTLY 4
typedef struct CommandToDo {struct {int order;} ordertype;} CommandToDo;
typedef struct Ship {CommandToDo *command;} Ship;
typedef struct SelectCommand {int numShips;Ship *ShipPtr[3];} SelectCommand;
struct {real32 totaltimeelapsed;int mainCommandLayer;} universe;
Ship fleet[3];CommandToDo busy={{3}};int docks;
SelectCommand *selectAllCurrentPlayersNonHyperspacingShips(void){SelectCommand *s=malloc(sizeof(*s));s->numShips=3;s->ShipPtr[0]=fleet;s->ShipPtr[1]=fleet+1;s->ShipPtr[2]=fleet+2;return s;}
CommandToDo *getShipAndItsCommand(int *layer,Ship *s){return s->command;}
void clDock(int *layer,SelectCommand *s,int mode,void *host){assert(s->numShips==1&&s->ShipPtr[0]==fleet&&mode==DOCK_PERMANENTLY);docks++;}
void memFree(void *p){free(p);}
''',source,r'''
int main(void){fleet[1].command=fleet[2].command=&busy;universe.totaltimeelapsed=10;
 spRetryStrandedRollCallShips();assert(docks==1);
 universe.totaltimeelapsed=11;spRetryStrandedRollCallShips();assert(docks==1);
 universe.totaltimeelapsed=12;spRetryStrandedRollCallShips();assert(docks==2);
 universe.totaltimeelapsed=0;spRetryStrandedRollCallShips();assert(docks==3);return 0;}
''')
 def test_native_lights_do_not_fall_back_to_absent_classic_lamps(self):
  source=function('src/Game/NavLights.c','navLightPositionForShip')
  run_c(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int bool32;
#define TRUE 1
#define FALSE 0
#define OBJ_ShipType 1
#define R2 2
#define AdvanceSupportFrigate 3
typedef struct vector {float x,y,z;} vector;
typedef struct Ship {int objtype,shiprace,shiptype;} Ship;
typedef struct NAVLightStatic {char name[20];vector position;} NAVLightStatic;
int native;
int meshRemasteredMarkerPosition(Ship *s,const char *name,vector *p){if(!native)return 0;if(!strcmp(name,"nav_antenna1")||!strcmp(name,"nav_Dock1")){*p=(vector){10,20,30};return 1;}return 0;}
''',source,r'''
int main(void){Ship support={OBJ_ShipType,R2,AdvanceSupportFrigate};NAVLightStatic light={"Dock1",{1,2,3}};
 native=1;assert(navLightPositionForShip(&support,&light)&&light.position.x==10);
 strcpy(light.name,"Dock2");assert(!navLightPositionForShip(&support,&light));
 strcpy(light.name,"Dock2a");assert(!navLightPositionForShip(&support,&light));
 strcpy(light.name,"caution2");assert(!navLightPositionForShip(&support,&light));
 native=0;light.position.x=9;assert(navLightPositionForShip(&support,&light)&&light.position.x==9);
 native=1;support.shiprace=1;assert(navLightPositionForShip(&support,&light));return 0;}
''')
 def test_native_khar_selim_approach_does_not_require_super_salvagers(self):
  source=function('src/Ships/SalCapCorvette.c','salCapNativeWreckIntercept')
  run_c(r'''
#include <assert.h>
#include <stddef.h>
typedef int bool32;
#define SalCapCorvette 1
typedef struct Ship {int shiptype;} Ship;
typedef struct SpaceObjRotImpTargGuidanceShipDerelict {int knownNativeWreck;} SpaceObjRotImpTargGuidanceShipDerelict;
int salCapAlignKharSelimPoints(SpaceObjRotImpTargGuidanceShipDerelict *target){return target&&target->knownNativeWreck;}
''',source,r'''
int main(void){Ship salvager={SalCapCorvette},other={2};SpaceObjRotImpTargGuidanceShipDerelict wreck={1},classic={0};
 assert(salCapNativeWreckIntercept(&salvager,&wreck));assert(!salCapNativeWreckIntercept(&other,&wreck));
 assert(!salCapNativeWreckIntercept(&salvager,&classic));assert(!salCapNativeWreckIntercept(NULL,&wreck));
 assert(!salCapNativeWreckIntercept(&salvager,NULL));return 0;}
''')
if __name__=='__main__':unittest.main()
