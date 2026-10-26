"""CPU-only checks of the production FPS formatter, draw batches and settings."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class FPSOverlay(unittest.TestCase):
    def test_persistence_and_presenter_clock_contract(self):
        main = (ROOT / 'src/SDL/main.c').read_text()
        utility = (ROOT / 'src/SDL/utility.c').read_text()
        options = (ROOT / 'src/Game/Options.c').read_text()
        ui = (ROOT / 'src/Game/ModernUI.c').read_text()
        self.assertIn('bool32 mainShowFPS = FALSE;', main)
        self.assertRegex(utility, r'\{"showFPS",\s*scriptSetUdwordCB,\s*&mainShowFPS\}')
        self.assertIn('mainShowFPS = opSaveShowFPS;',
                      function(options, 'void opRestoreSavedSettings(void)\n{'))
        self.assertIn('opSaveShowFPS = mainShowFPS;',
                      function(options, 'void opOptionsSaveSettings(void)\n{'))
        self.assertIn('muiGameplay.savedShowFPS = mainShowFPS;', ui)
        draw = function(ui, 'void modernUIDrawFPS(void)')
        self.assertIn('hwModernGraphicsGetFramesPerSecond()', draw)
        self.assertNotIn('SDL_GetTicks', draw)
        self.assertNotIn('SDL_GetPerformanceCounter', draw)

    def test_actual_format_drawing_toggle_cancel_and_layout(self):
        source = (ROOT / 'src/Game/ModernUI.c').read_text()
        gameplay = source[source.index('typedef struct ModernUIGameplayState'):
                          source.index('typedef struct ModernUIAudioState')]
        layout = source[source.index('typedef struct ModernUILayout'):
                        source.index('typedef enum ModernUIScreenKind')]
        harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef int bool32,sdword;
typedef unsigned int udword;
typedef unsigned char ubyte;
typedef float real32;
#define TRUE 1
#define FALSE 0
#define GL_QUADS 7
#define MUI_GAMEPLAY_ROW_COUNT ROW_COUNT_FROM_SOURCE
#define MUI_COLOR_TAB_COUNT 6
#define MUI_ROW_COUNT 5
typedef struct {int x0,y0,x1,y1;} rectangle;
'''
        harness = harness.replace("ROW_COUNT_FROM_SOURCE", re.search(r"#define MUI_GAMEPLAY_ROW_COUNT (\d+)", source)[1])
        harness += gameplay + layout
        harness += r'''
static ModernUIGameplayState muiGameplay;
static sdword opMouseSens,mouseCursorScalePercent;
static udword opUnlimitedFuel,opPauseOrders,opShipRecoil,opSuperSalvagers;
static udword opCaptureBuildAll,opDisableUnitCaps,opRankSystem;
static udword opMissionStartLaunch,opResourceMultiplier,opDisableSPResourceScaling,opUnlimitedRUs;
static int mainShowFPS,MAIN_WindowWidth=1280,MAIN_WindowHeight=720;
static int primModeEnabled,ghMainRegion,closed,saves,savedShowFPS;
static int begins,ends,vertices,colors,reads,badColor;
static float sourceFPS=239.6f,scale=1,minX,minY,maxX,maxY;
static sdword muiClamp(sdword v,sdword a,sdword b){return v<a?a:(v>b?b:v);}
static int uiScaleSize(int v){return (int)(scale*v+0.5f);}
static float hwModernGraphicsGetFramesPerSecond(void){++reads;return sourceFPS;}
#define primScreenToGLX(x) ((float)(x))
#define primScreenToGLY(y) ((float)(y))
#define primModeSet2() do {primModeEnabled=1;} while(0)
#define primModeClear2() do {primModeEnabled=0;} while(0)
static void rndTextureEnable(int enabled){(void)enabled;}
static void glBegin(int mode){if(mode!=GL_QUADS)abort();++begins;}
static void glEnd(void){++ends;}
static void glColor4ub(int r,int g,int b,int a){
    if(r!=(colors%2?255:0)||g!=r||b!=0||a!=255)badColor=1;++colors;
}
static void glVertex2f(float x,float y){
    if(!vertices){minX=maxX=x;minY=maxY=y;}
    if(x<minX)minX=x;if(x>maxX)maxX=x;
    if(y<minY)minY=y;if(y>maxY)maxY=y;++vertices;
}
static void muiGameplayApplyLive(void){}
static void opOptionsSaveSettings(void){++saves;savedShowFPS=mainShowFPS;}
static void feScreenDisappear(void*a,void*b){(void)a;(void)b;++closed;}
static void feScreenStart(int a,const char*b){(void)a;(void)b;}
'''
        for signature in ('static rectangle muiRect(',
                          'static void muiFPSFormat(',
                          'void modernUIDrawFPS(void)',
                          'static const char *muiGameplayValue(',
                          'static void muiAdjustGameplay(',
                          'static void muiExitGameplay(',
                          'static void muiBuildLayout(',
                          'static void muiGameplayFitLayout(',
                          'static rectangle muiGameplaySpecialRect('):
            harness += '\n' + function(source, signature) + '\n'
        harness += r'''
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    char text[5],value[64];int index,before;
    const float inputs[]={0,-1,NAN,0.49f,0.5f,9.49f,9.5f,239.6f,9999,INFINITY};
    const char *expected[]={"0","0","0","0","1","9","10","240","9999","9999"};
    for(index=0;index<10;++index){muiFPSFormat(inputs[index],text);CHECK(!strcmp(text,expected[index]));}
    modernUIDrawFPS();CHECK(!reads&&!begins&&!vertices);
    muiAdjustGameplay(8,1);CHECK(mainShowFPS);
    CHECK(!strcmp(muiGameplayValue(8,value,sizeof(value)),"ON"));
    for(index=1;index<=3;++index){
        ModernUILayout l;rectangle special;int row;
        scale=(float)index;MAIN_WindowWidth=1280*index;MAIN_WindowHeight=720*index;
        begins=ends=vertices=colors=badColor=0;primModeEnabled=index%2;
        before=primModeEnabled;modernUIDrawFPS();
        CHECK(begins==2&&ends==2&&colors==2&&vertices>0&&!badColor);
        CHECK(primModeEnabled==before);
        CHECK(maxX==MAIN_WindowWidth-uiScaleSize(8)+uiScaleSize(1));
        CHECK(minX>=MAIN_WindowWidth-uiScaleSize(50));
        CHECK(minY==uiScaleSize(8)-uiScaleSize(1));
        CHECK(maxY==uiScaleSize(8)+7*uiScaleSize(2)+uiScaleSize(1));
        muiBuildLayout(&l);muiGameplayFitLayout(&l);
        for(row=0;row<MUI_GAMEPLAY_ROW_COUNT;++row){CHECK(l.rows[row].y0<l.rows[row].y1);
            if(row)CHECK(l.rows[row].y0>l.rows[row-1].y1);}
        special=muiGameplaySpecialRect(&l,2);
        CHECK(special.y1<=l.apply.y0-uiScaleSize(8));
    }
    muiGameplay.savedShowFPS=0;muiGameplay.savedDisableSPResourceScaling=0;muiGameplay.savedUnlimitedRUs=0;
    muiAdjustGameplay(10,1);CHECK(opUnlimitedRUs);
    CHECK(!strcmp(muiGameplayValue(10,value,sizeof(value)),"ON"));
    muiAdjustGameplay(9,1);CHECK(opDisableSPResourceScaling);
    CHECK(!strcmp(muiGameplayValue(9,value,sizeof(value)),"ON"));
    muiExitGameplay(FALSE);CHECK(!opDisableSPResourceScaling && !opUnlimitedRUs);
    CHECK(!mainShowFPS&&!saves&&closed==1);
    CHECK(!strcmp(muiGameplayValue(8,value,sizeof(value)),"OFF"));
    muiAdjustGameplay(8,-1);muiAdjustGameplay(9,-1);muiAdjustGameplay(10,-1);muiExitGameplay(TRUE);
    CHECK(opDisableSPResourceScaling && opUnlimitedRUs);
    CHECK(mainShowFPS&&saves==1&&savedShowFPS&&closed==2);
    puts("PASS: production FPS formatting, two-color batches, resolution scaling, gameplay row fit and resource scaling toggle, toggle/apply/cancel");
    return 0;
}
'''
        compilers = list(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC required for CPU FPS overlay test')
        compiler = sorted(compilers)[-1]
        vcvars = compiler.parents[7] / 'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-fps-overlay-') as directory:
            directory = Path(directory)
            (directory / 'fps.c').write_text(harness)
            command = (f'call "{vcvars}" >nul && "{compiler}" /nologo /TC '
                       '/D_CRT_SECURE_NO_WARNINGS fps.c /Fe:fps.exe && fps.exe')
            result = subprocess.run(command, shell=True, cwd=directory,
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout)


if __name__ == '__main__':
    unittest.main()
