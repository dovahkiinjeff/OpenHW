"""Exercise production parade placement and bounded research text without a GPU."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def run_c(harness, production, cases):
    cache = (ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
    setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1])/'Common7/Tools/VsDevCmd.bat'
    with tempfile.TemporaryDirectory(prefix='hw-community-campaign-') as tmp:
        folder = Path(tmp)
        (folder/'test.c').write_text(harness+production+cases)
        built = subprocess.run(
            f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe"',
            cwd=folder, capture_output=True, text=True)
        if built.returncode:
            raise AssertionError(built.stdout+built.stderr)
        subprocess.run([str(folder/'test.exe')], cwd=folder, check=True, timeout=10)


class CampaignReportTests(unittest.TestCase):
    def test_audio_frequency_previews_original_range_and_clamps(self):
        source=(ROOT/'src/Game/ModernUI.c').read_text()
        production='static void muiAdjustAudio('+source.split('static void muiAdjustAudio(',1)[1].split('static void muiExitAudio(',1)[0]
        run_c(r'''
#include <assert.h>
typedef int sdword;
enum {SOUND_MODE_NORM,SOUND_MODE_AUTO,SOUND_MODE_LOW};
struct {int tab;} muiAudio;
int opMusicVol,opSpeechVol,opSFXVol,opNumChannels,opAutoChannel,opSoundQuality,opSpeakerSetting;
int opVoiceComm,opVoiceStat,opVoiceChat,opBattleChatter,lastFrequency,applied;
int muiClamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
void soundGetVoiceLimits(int *lo,int *hi){*lo=4;*hi=32;}
void battleChatterFrequencySet(int n){lastFrequency=n;}
void opUpdateAudioSettings(void){applied++;}
''',production,r'''
int main(void){
    muiAudio.tab=1;opBattleChatter=50;muiAdjustAudio(5,1);
    assert(opBattleChatter==51 && lastFrequency==51 && applied==1);
    opBattleChatter=100;muiAdjustAudio(5,1);assert(opBattleChatter==100 && lastFrequency==100);
    opBattleChatter=0;muiAdjustAudio(5,-1);assert(opBattleChatter==0 && lastFrequency==0);
    muiAdjustAudio(4,1);assert(opVoiceChat==1 && opBattleChatter==0);
    return 0;
}
''')

    def test_new_ship_uses_authored_parade_pose_without_moving_existing_ships(self):
        source = (ROOT/'src/Game/Formation.c').read_text()
        production = 'static void setMilitaryParadeShips('+source.split(
            'static void setMilitaryParadeShips(', 1)[1].split('void FreeMilitaryParadeContents(', 1)[0]
        run_c(r'''
#include <assert.h>
#include <string.h>
typedef int bool32,sdword;
#define TRUE 1
#define FALSE 0
#define NUMBER_PARADE_TYPES 1
#define MAX_MILITARY_SLOTS 1
#define dbgAssertOrIgnore(x) assert(x)
typedef struct vector {float x,y,z;} vector;
typedef struct matrix {vector up,right,heading;} matrix;
typedef struct Ship {int custom,updates;struct {vector position;} posinfo;struct {matrix coordsys;} rotinfo;} Ship;
typedef Ship SpaceObjRot;
typedef struct SlotInfo {float upoffset,rightoffset,headingoffset,rightspacing,upspacing,headingspacing;int facedirection;} SlotInfo;
typedef struct MilitarySlot {int highestSlotUsed,numSubslots;struct {Ship *ship;} subslots[3];} MilitarySlot;
typedef struct MilitaryParadeCommand {Ship *aroundShip;int paradeType;MilitarySlot *militarySlots[1];} MilitaryParadeCommand;
typedef struct ParadeTypeInfo {SlotInfo slotinfos[1];} ParadeTypeInfo;
struct CommandToDo {MilitaryParadeCommand *militaryParade;};
ParadeTypeInfo paradeTypeInfos[1];
#define matGetVectFromMatrixCol1(v,m) ((v)=(m).up)
#define matGetVectFromMatrixCol2(v,m) ((v)=(m).right)
#define matGetVectFromMatrixCol3(v,m) ((v)=(m).heading)
#define vecAddToScalarMultiply(v,a,s) do{(v).x+=(a).x*(s);(v).y+=(a).y*(s);(v).z+=(a).z*(s);}while(0)
#define vecScalarMultiply(v,a,s) do{(v).x=(a).x*(s);(v).y=(a).y*(s);(v).z=(a).z*(s);}while(0)
#define vecAddTo(v,a) do{(v).x+=(a).x;(v).y+=(a).y;(v).z+=(a).z;}while(0)
#define vecCopyAndNegate(v,a) vecScalarMultiply(v,a,-1)
#define vecCrossProduct(v,a,b) do{(v).x=(a).y*(b).z-(a).z*(b).y;(v).y=(a).z*(b).x-(a).x*(b).z;(v).z=(a).x*(b).y-(a).y*(b).x;}while(0)
void matCreateMatFromVecs(matrix *m,vector *u,vector *r,vector *h){m->up=*u;m->right=*r;m->heading=*h;}
void univUpdateObjRotInfo(SpaceObjRot *s){s->updates++;}
int paradeCustomPose(MilitaryParadeCommand *p,Ship *s,vector *v,vector *h,vector *u){
    if(!s->custom)return 0;
    *v=(vector){123,456,789};*h=(vector){0,1,0};*u=(vector){0,0,1};return 1;
}
''', production, r'''
int main(void){
    Ship host={0},old={0},fresh={0},next={0};MilitarySlot slot={2,3,{{&old},{&fresh},{&next}}};
    MilitaryParadeCommand parade={&host,0,{&slot}};struct CommandToDo command={&parade};
    host.rotinfo.coordsys=(matrix){{0,0,1},{1,0,0},{0,1,0}};
    old.posinfo.position=(vector){9,8,7};next.posinfo.position=(vector){6,5,4};
    paradeTypeInfos[0].slotinfos[0]=(SlotInfo){10,20,30,50,0,0,0};
    fresh.custom=1;setMilitaryParadeShip(&command,&fresh);
    assert(fresh.posinfo.position.x==123 && fresh.posinfo.position.z==789 && fresh.updates==1);
    assert(old.posinfo.position.x==9 && next.posinfo.position.x==6 && old.updates==0 && next.updates==0);
    fresh.custom=0;setMilitaryParadeShip(&command,&fresh);
    assert(fresh.posinfo.position.x==70 && fresh.posinfo.position.y==30 && fresh.posinfo.position.z==10);
    assert(old.posinfo.position.x==9 && next.posinfo.position.x==6);
    setMilitaryParade(&command);assert(old.posinfo.position.x==20 && next.posinfo.position.x==120);
    return 0;
}
''')
        sp = (ROOT/'src/Game/SinglePlayer.c').read_text()
        load = sp.split('void singlePlayerLevelLoaded(void)', 1)[1].split('void CleanupDamageEffects(', 1)[0]
        self.assertLess(load.index('paradeCampaignLoaded();'), load.index('spCompleteStartingResourceFleetFor('))
        self.assertIn('!singlePlayerHyperspacingInto', load)
        arrival = sp.split('case HYPERSPACE_SHIPSARRIVING:', 1)[1].split('break;', 1)[0]
        self.assertLess(arrival.index('CleanupHyperspacingShips(FALSE,FALSE);'), arrival.index('spCompleteStartingResourceFleetFor('))

    def test_research_description_draw_handles_long_localized_words_and_bounded_panel(self):
        source = (ROOT/'src/Game/ResearchGUI.c').read_text()
        production = 'void rmTechInfoDraw('+source.split('void rmTechInfoDraw(', 1)[1].split('static void rmDockDetailsDraw(', 1)[0]
        run_c(r'''
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef int sdword,fonthandle;
typedef struct region {struct {int x0,y0,x1,y1;} rect;} *regionhandle;
enum {R1,NumTechnologies=32,DDDFDFGFTech=17,CloakDefenseFighter=3};
enum {strR1DDDFTechinfo=100,strR2DFGFTechinfo,strR1CloakFighterinfo,strR2DefenseFighterTechinfo,strTechInfoOffsetR1=200,strTechInfoOffsetR2=300};
struct Player {int race;};struct {struct Player *curPlayerPtr;} universe;
int techinfo=1,rmTechInfoFont=1,rmStandardTextColor=1,prints=0,selected=-1,largest=0;
char description[5000];
#define colRGB(r,g,b) (r)
int uiScaleSize(int x){return x;}
int fontMakeCurrent(int f){return 1;}
int fontHeight(char *s){return 12;}
int fontWidthN(char *s,int n){return n*8;}
int fontPrint(int x,int y,int c,char *s){return 0;}
int fontPrintf(int x,int y,int c,char *fmt,...){return 0;}
int fontPrintN(int x,int y,int c,char *s,int n){assert(n>=0 && n<=511);prints++;if(n>largest)largest=n;return 0;}
char *RaceSpecificTechTypeToNiceString(int t,int r){return "Topic";}
const char *strGetString(int id){selected=id;return description;}
void rmAppendShipsUnlocked(char *dest,size_t n){size_t used=strlen(dest);snprintf(dest+used,n-used,"\nUnlocked ships");}
''', production, r'''
int main(void){
    struct Player player={R1};struct region region={{0,0,60,500}};universe.curPlayerPtr=&player;
    memset(description,'A',sizeof(description)-1);description[sizeof(description)-1]=0;
    rmTechInfoDraw(&region);assert(selected==201 && prints>0 && prints<50 && largest<=4);
    player.race=1;techinfo=DDDFDFGFTech;prints=0;rmTechInfoDraw(&region);assert(selected==strR2DFGFTechinfo && prints>0);
    techinfo=CloakDefenseFighter;rmTechInfoDraw(&region);assert(selected==strR2DefenseFighterTechinfo);
    techinfo=-1;prints=0;rmTechInfoDraw(&region);assert(prints==0);
    region.rect.x1=1;techinfo=0;rmTechInfoDraw(&region);assert(prints==0);
    return 0;
}
''')


if __name__ == '__main__':
    unittest.main()
