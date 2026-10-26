"""Exercise production parade persistence/math; inspect the UI/runtime contracts."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MilitaryParadeEditorTests(unittest.TestCase):
    def test_load_overrides_old_parade_without_cancelling_orders(self):
        source = (ROOT / 'src/Game/Formation.c').read_text()
        production = 'static bool32 paradeCampaignPending=FALSE;' + source.split(
            'static bool32 paradeCampaignPending=FALSE;', 1)[1].split('void scriptSetSlotInfoCB(', 1)[0]
        harness = r'''
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef int bool32,sdword;
#define TRUE 1
#define FALSE 0
#define Mothership 1
#define R2 1
#define NO_HYPERSPACE 0
#define COMMAND_NULL 0
#define SOF_Dead 1
#define SOF_NISShip 2
#define SOF_Hyperspace 4
#define SOF_Disabled 8
#define SOF_Clamped 16
#define SOF_Slaveable 32
#define SPECIAL_Launching 64
#define SF_SLAVE 1
typedef struct vector {float x,y,z;} vector;
typedef struct matrix {vector up,right,heading;} matrix;
typedef struct SlaveInfo {int flags;} SlaveInfo;
typedef struct Ship Ship;
typedef Ship SpaceObjRot;
typedef struct Player {Ship *PlayerMothership;} Player;
typedef struct CommandToDo {struct {int order,attributes;} ordertype;void *militaryParade;} CommandToDo;
struct Ship {
    int shiptype,shiprace,flags,specialFlags,shipidle;
    Player *playerowner;SlaveInfo *slaveinfo;CommandToDo *command;
    struct {vector position,velocity;} posinfo;
    struct {matrix coordsys;vector rotspeed;} rotinfo;
};
typedef struct Node {struct Node *next;Ship *ship;} Node;
struct {Player *curPlayerPtr;struct {Node *head;} ShipList;int mainCommandLayer;} universe;
struct {int hyperspaceState;} singlePlayerGameInfo;
typedef struct ParadeShipLayout {int enabled;} ParadeShipLayout;
int singlePlayerGame=1,multiPlayerGame=0,nisIsRunning=0,reloaded=0,joined=0;
CommandToDo parade={{2,0},0},busy={{5,0},0};
#define listGetStructOfNode(n) ((n)->ship)
#define vecSet(v,a,b,c) ((v)=(vector){a,b,c})
void vecCrossProduct(vector r,vector h,vector u){}
#undef vecCrossProduct
#define vecCrossProduct(r,h,u) ((r)=(vector){1,0,0})
void matCreateMatFromVecs(matrix *m,vector *u,vector *r,vector *h){m->up=*u;m->right=*r;m->heading=*h;}
int paradeLayoutReload(int host){reloaded++;return 1;}
int paradeLayoutRuntime(int host,int race,int type,int ordinal,ParadeShipLayout *e,float *local){return type==2;}
CommandToDo *getShipAndItsCommand(int *layer,Ship *s){return s->command;}
void GroupShipIntoMilitaryParade(int *layer,Ship *s,Ship *leader){s->command=&parade;joined++;}
CommandToDo *GetMilitaryGroupAroundShip(int *layer,Ship *leader){return &parade;}
int paradeCustomPose(void *p,Ship *s,vector *position,vector *heading,vector *up){
    if(s->shiptype!=2)return 0;
    *position=(vector){1234,567,890};*heading=(vector){0,1,0};*up=(vector){0,0,1};return 1;
}
void univUpdateObjRotInfo(SpaceObjRot *s){}
'''
        cases = r'''
int main(void){
    Ship mother={0},ships[5]={0};Player owner={&mother},enemy={0};Node nodes[5];int i;
    mother.shiptype=Mothership;mother.playerowner=&owner;universe.curPlayerPtr=&owner;
    for(i=0;i<5;i++){ships[i].shiptype=2;ships[i].playerowner=&owner;nodes[i].ship=&ships[i];nodes[i].next=i<4?&nodes[i+1]:0;}
    ships[0].command=&parade;ships[1].command=0;ships[2].command=&busy;
    ships[3].playerowner=&enemy;ships[4].shiptype=3;universe.ShipList.head=nodes;
    paradeCampaignLoaded();assert(reloaded==1);
    nisIsRunning=1;paradeCampaignUpdate();assert(joined==0 && ships[0].posinfo.position.x==0);
    nisIsRunning=0;singlePlayerGameInfo.hyperspaceState=2;paradeCampaignUpdate();assert(joined==0);
    singlePlayerGameInfo.hyperspaceState=0;paradeCampaignUpdate();assert(joined==1);
    assert(ships[0].posinfo.position.x==1234 && ships[1].posinfo.position.z==890);
    assert(ships[2].command==&busy && ships[2].posinfo.position.x==0);
    assert(ships[3].command==0 && ships[4].command==0);
    paradeCampaignUpdate();assert(joined==1);
    multiPlayerGame=1;paradeCampaignLoaded();assert(reloaded==1);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-parade-save-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + production + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)
        save = (ROOT / 'src/Game/SaveGame.c').read_text().split('void LoadGame(char *filename)', 1)[1]
        self.assertLess(save.index('FixAllSpaceObjs();'),save.index('paradeCampaignLoaded();'))
        sp = (ROOT / 'src/Game/SinglePlayer.c').read_text()
        self.assertIn('paradeCampaignLoaded();', sp.split('void singlePlayerLevelLoaded(void)', 1)[1])
        self.assertIn('paradeCampaignUpdate();', sp.split('void singlePlayerGameUpdate(void)', 1)[1])

    def test_shared_game_camera_controls(self):
        camera = (ROOT / 'src/Game/Camera.c').read_text()
        functions = camera.split('real32 cameraWheelZoomFactor(bool32 zoomIn)', 1)[1].split('sdword cameraControl(', 1)[0]
        functions = 'real32 cameraWheelZoomFactor(bool32 zoomIn)' + functions
        harness = r'''
#include <assert.h>
#include <math.h>
typedef float real32;
typedef int bool32,sdword;
typedef struct Camera { float angle,declination,distance; } Camera;
float CAMERA_MOUSE_SENS=1, CAMERA_WHEEL_ZOOM_IN=.925f, CAMERA_WHEEL_ZOOM_OUT=1.081081f;
float CAMERA_ZOOM_OUT=1.005f,CAMERA_ZOOM_IN=.995f;
float CAMERA_DECLINATION_ROT=.001745329252f,CAMERA_ANGLE_ROT=.001745329252f;
int useSlowWheelZoomIn=0;
void cameraZoom(Camera *c,float factor,int enforce){c->distance*=factor;}
void cameraRotDeclination(Camera *c,float d){c->declination+=d;}
void cameraRotAngle(Camera *c,float d){c->angle+=d;}
'''
        cases = r'''
int main(void){
    Camera c={0,0,100};
    cameraMouseDrag(&c,-100,-50,0,0);
    assert(fabsf(c.angle+.1745329252f)<.00001f);
    assert(fabsf(c.declination+.0872664626f)<.00001f && c.distance==100);
    cameraMouseDrag(&c,200,-20,1,0);
    assert(fabsf(c.distance-110)<.001f && fabsf(c.angle+.1745329252f)<.00001f);
    cameraMouseDrag(&c,200,20,1,0);assert(fabsf(c.distance-99)<.001f);
    assert(fabsf(cameraWheelZoomFactor(1)-.925f)<.00001f);
    assert(fabsf(cameraWheelZoomFactor(0)-1.081081f)<.00001f);
    CAMERA_MOUSE_SENS=2; c.angle=c.declination=0;
    cameraMouseDrag(&c,-100,-50,0,0);assert(fabsf(c.angle+.3490658504f)<.00001f);
    assert(fabsf(cameraWheelZoomFactor(1)-.85f)<.00001f);
    useSlowWheelZoomIn=1;assert(fabsf(cameraWheelZoomFactor(1)-.92f)<.00001f);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-parade-camera-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + functions + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)
        ui = (ROOT / 'src/Game/MilitaryParadeEditor.inc').read_text()
        self.assertIn('cameraMouseDrag(&camera,cx-x,cy-y,mouseLeftButton(),FALSE)', ui)
        self.assertIn('cameraWheelZoomFactor(d<0)', ui)
        self.assertIn('CAMERA_WHEEL_ZOOM_RESPONSE', ui)
        self.assertIn('mousePositionSet(cx,cy)', ui)
        self.assertIn('mouseCursorShow()', ui)
        self.assertIn('mouseClipToRect(NULL)', ui)
        self.assertIn('glRotatef(mpe.pitch,1,0,0);glRotatef(mpe.yaw,0,0,1)', ui)
        live = camera.split('sdword cameraControl(', 1)[1]
        self.assertIn('cameraMouseDrag(camera,camMouseX,camMouseY,zoom,EnforceShipDistances)', live)

    def test_production_persistence_rotation_and_isolated_mp(self):
        source = (ROOT / 'src/Game/MilitaryParadeLayout.c').read_text()
        production = 'static ParadeLayoutDocument layouts[2];' + source.split(
            'static ParadeLayoutDocument layouts[2];', 1)[1]
        header = (ROOT / 'src/Game/MilitaryParadeLayout.h').read_text()
        api = 'typedef struct ParadeShipLayout' + header.split(
            'typedef struct ParadeShipLayout', 1)[1].rsplit('#endif', 1)[0]
        harness = r'''
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
typedef int bool32,sdword;
typedef float real32;
typedef unsigned int ShipRace,ShipType;
#define TRUE 1
#define FALSE 0
#define R1 0
#define R2 1
#define NUM_RACES 6
#define TOTAL_NUM_SHIPS 8
#define RaceToRaceBits(r) (1<<(r))
#define SDL_free free
ShipType FirstShipTypeOfRace[NUM_RACES]={0,0,0,0,0,0};
ShipType LastShipTypeOfRace[NUM_RACES]={7,7,7,7,7,7};
unsigned int RacesAllowedForGivenShip[TOTAL_NUM_SHIPS]={63,63,63,63,63,63,63,63};
char *SDL_GetBasePath(void){char *p=malloc(3);strcpy(p,"./");return p;}
unsigned long long SDL_GetPerformanceCounter(void){static unsigned long long n=100;return ++n;}
char *ShipRaceToStr(ShipRace r){static char *names[]={"R1","R2","P1","P2","P3","Traders"};return names[r];}
char *ShipTypeToStr(ShipType t){static char *names[]={"A","B","C","D","E","F","G","H"};return names[t];}
ShipRace StrToShipRace(char *s){unsigned int i;for(i=0;i<6;i++)if(!strcmp(s,ShipRaceToStr(i)))return i;return -1;}
ShipType StrToShipType(char *s){unsigned int i;for(i=0;i<8;i++)if(!strcmp(s,ShipTypeToStr(i)))return i;return -1;}
'''
        cases = r'''
int main(void){
    ParadeLayoutDocument d={0},got={0},mp={0};ParadeShipLayout e;float p[3],r[3]={0,0,90},v[3]={0,1,0},out[3];FILE *fp;
    d.ships[0][1].enabled=1;d.ships[0][1].position[0]=123;d.ships[0][1].growth[0]=400;
    d.ships[1][1].enabled=1;d.ships[1][1].position[0]=-123;
    assert(paradeLayoutSave(R1,&d));paradeLayoutGet(R1,&got);assert(got.ships[0][1].position[0]==123);
    assert(paradeLayoutRuntime(R1,0,1,3,&e,p) && p[0]==1323);
    assert(paradeLayoutRuntime(R1,1,1,0,&e,p) && p[0]==-123);
    assert(!paradeLayoutRuntime(R1,0,2,0,&e,p));
    mp=d;mp.ships[0][1].position[0]=999;
    assert(paradeLayoutSaveMultiplayer(R1,"test-preset",&mp));
    paradeLayoutGet(R1,&got);assert(got.ships[0][1].position[0]==123);
    assert(paradeLayoutLoadMultiplayer(R1,"test-preset",&got) && got.ships[0][1].position[0]==999);
    assert(!paradeLayoutSaveMultiplayer(R1,"../escape",&mp));
    assert(paradeLayoutReload(R1));paradeLayoutGet(R1,&got);assert(got.ships[0][1].position[0]==123);
    assert(paradeLayoutSave(R2,&mp));assert(paradeLayoutSave(R1,&d));
    assert(GetFileAttributesA("Parade-Kushan.cfg.bak-101")!=INVALID_FILE_ATTRIBUTES);
    fp=fopen("Parade-Kushan.cfg","wb");fputs("OPENHW_PARADE 1\nR1 B nan 0 0 0 0 0 0 0 0\n",fp);fclose(fp);
    assert(!paradeLayoutReload(R1));paradeLayoutGet(R1,&got);assert(got.ships[0][1].position[0]==123);
    fp=fopen("Parade-Kushan.cfg","wb");fputs("OPENHW_PARADE 1\nR1 B 0 0 0 0 0 0 0 0 0\nR1 B 1 0 0 0 0 0 0 0 0\n",fp);fclose(fp);
    assert(!paradeLayoutReload(R1));
    paradeLayoutRotate(r,v,out);assert(fabsf(out[0]+1)<.00001f && fabsf(out[1])<.00001f);
    r[0]=30;r[1]=60;r[2]=-20;paradeLayoutRotate(r,v,out);
    assert(fabsf(out[0]*out[0]+out[1]*out[1]+out[2]*out[2]-1)<.00001f);
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-parade-editor-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness + api + production + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)

    def test_real_models_unique_types_and_no_preview_simulation(self):
        ui = (ROOT / 'src/Game/MilitaryParadeEditor.inc').read_text()
        main = (ROOT / 'src/Game/ModernUI.c').read_text()
        self.assertIn('"MODDING", "EXIT"', main)
        self.assertIn('meshLoad(path)', ui)
        self.assertIn('meshRender(mesh,0)', ui)
        self.assertIn('LOD0', ui)
        self.assertIn('meshFree(mpe.meshes[r][t])', ui)
        self.assertNotIn('univAddShip', ui)
        self.assertNotIn('InitStatShipInfo', ui)
        self.assertIn('if(mpe.document.ships[id->race][id->type].enabled)continue;', ui)
        for group in ('POSITION','ROTATION','GROWTH'):
            for axis in 'XYZ':self.assertIn(f'"{group} {axis}"', ui)
        self.assertIn('mpeRingPoint', ui)
        self.assertIn('mpe.drag==4', ui)
        self.assertIn('mpe.drag==5', ui)
        self.assertIn('RPE_HoldCentre', main)

    def test_runtime_separates_identities_and_honors_hyperspace(self):
        formation = (ROOT / 'src/Game/Formation.c').read_text()
        self.assertIn('if(multiPlayerGame || leader->shiptype!=Mothership', formation)
        self.assertIn('other->shiprace==ship->shiprace && other->shiptype==ship->shiptype', formation)
        self.assertIn('aitrackHeadingAndUp(ship,&faceheading,&customUp', formation)
        self.assertIn('positionat=legacyPosition;faceheading=legacyHeading;', formation)
        live = formation.split('void processMilitaryParadeToDo(', 1)[1].split('void setMilitaryParade(', 1)[0]
        self.assertLess(live.index('singlePlayerHyperspaceOwnsSteering(ship)'),live.index('paradeCustomPose(militaryParade,ship'))
        instant = formation.split('void setMilitaryParade(', 1)[1].split('void FreeMilitaryParadeContents', 1)[0]
        self.assertIn('paradeCustomPose(militaryParade,ship', instant)
        self.assertIn('matCreateMatFromVecs(&ship->rotinfo.coordsys,&customUp,&customRight,&customHeading)', instant)


if __name__ == '__main__':
    unittest.main()
