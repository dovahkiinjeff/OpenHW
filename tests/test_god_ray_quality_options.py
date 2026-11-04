"""Exercise the production god-ray quality UI, config writer and cancel paths."""
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


class GodRayQualityOptions(unittest.TestCase):
    def test_production_ui_persistence_and_independence(self):
        main = (ROOT / 'src/SDL/main.c').read_text()
        options = (ROOT / 'src/Game/Options.c').read_text()
        ui = (ROOT / 'src/Game/ModernUI.c').read_text()
        utility = (ROOT / 'src/SDL/utility.c').read_text()
        default = re.search(r'sdword mainGodRayQuality = \d+;', main)[0]
        config_row = re.search(r'\{"godRayQuality",\s*scriptSetUdwordCB,\s*&mainGodRayQuality\}', utility)[0]
        self.assertIn('scriptSetFileSystem(ch_buf, UTY_CONFIG_FILENAME, utyOptionsList)', utility)
        self.assertIn('"GOD RAYS", "GOD RAY QUALITY", "BLOOM", "LENS FLARES"', ui)
        self.assertEqual(ui.count('(muiVideo.tab >= 2 ? MUI_VIDEO_ROW_COUNT : MUI_ROW_COUNT)'), 2)
        clamp = re.search(r'mainGodRayQuality = min\(max\(mainGodRayQuality, 0\), 3\);', options)[0]
        save = re.search(r'opSaveGodRayQuality = mainGodRayQuality;', options)[0]
        restore = re.search(r'mainGodRayQuality = opSaveGodRayQuality;', options)[0]
        self.assertEqual(options.count('hwModernGraphicsSetGodRayQuality(mainGodRayQuality);'), 2)
        self.assertIn('hwModernGraphicsSetGodRayQuality(mainGodRayQuality);', ui)
        writer = utility.split('else if (utyOptionsList[index].setVarCB == scriptSetUdwordCB)', 1)[1]
        writer = writer[writer.index('{') + 1:writer.index('}')]
        harness = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int sdword,bool32;
typedef unsigned int udword;
#define TRUE 1
#define FALSE 0
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int mainChromaticAberrationPercent,mainMotionBlurPercent,mainFilmGrainPercent;
static int mainGodRaysPercent=100,mainBloomPercent=137,mainLensFlares=1;
static int opSaveGodRayQuality,appliedQuality=-1;
static int muiClamp(int n,int a,int b){return n<a?a:n>b?b:n;}
static void hwModernGraphicsSetGodRayQuality(int q){appliedQuality=q;}
static void muiApplyRendererLive(void){hwModernGraphicsSetGodRayQuality(mainGodRayQuality);}
'''
        # Keep the real declaration before the test renderer adapter.
        harness = harness.replace('static void muiApplyRendererLive', default + '\nstatic void muiApplyRendererLive')
        harness += function(main, 'static bool32 ParseDisplayInteger(') + '\n'
        harness += function(main, 'bool32 SetGodRayQuality(') + '\n'
        harness += function(ui, 'static void muiAdjustPost(') + '\n'
        harness += function(ui, 'static const char *muiPostValue(') + '\n'
        harness += 'static void clampQuality(void){' + clamp + '}\n'
        harness += 'static void saveQuality(void){' + save + '}\n'
        harness += 'static void cancelQuality(void){' + restore + 'muiApplyRendererLive();}\n'
        harness += 'enum {scriptSetUdwordCB=1};\nstruct Entry {const char *name;int setVarCB;void *dataPtr;} utyOptionsList[]={' + config_row + '};\n'
        harness += 'static void writeConfig(FILE *f){int index=0;' + writer + '}\n'
        harness += r'''
int main(void) {
    char text[64],name[64];unsigned parsed;FILE *config;
    CHECK(mainGodRayQuality==1 && !strcmp(muiPostValue(4,text,sizeof(text)),"MEDIUM"));
    CHECK(!SetGodRayQuality("-1") && !SetGodRayQuality("4") && !SetGodRayQuality("2junk") && mainGodRayQuality==1);
    CHECK(SetGodRayQuality("3") && mainGodRayQuality==3);
    mainGodRayQuality=-100;clampQuality();CHECK(mainGodRayQuality==0);
    mainGodRayQuality=100;clampQuality();CHECK(mainGodRayQuality==3);
    for(int q=0;q<4;++q) {
        const char *labels[]={"LOW","MEDIUM","HIGH","ULTRA"};
        mainGodRayQuality=q;CHECK(!strcmp(muiPostValue(4,text,sizeof(text)),labels[q]));
        muiAdjustPost(4,1);CHECK(mainGodRayQuality==(q+1)%4 && appliedQuality==mainGodRayQuality);
        muiAdjustPost(4,-1);CHECK(mainGodRayQuality==q);
        CHECK(mainGodRaysPercent==100 && mainBloomPercent==137 && mainLensFlares==1);
        config=tmpfile();CHECK(config);writeConfig(config);rewind(config);
        CHECK(fscanf(config,"%63s %u",name,&parsed)==2 && !strcmp(name,"godRayQuality") && parsed==(unsigned)q);
        fclose(config);mainGodRayQuality=0;*((udword*)utyOptionsList[0].dataPtr)=parsed;clampQuality();
        CHECK(mainGodRayQuality==q);
    }
    mainGodRayQuality=2;saveQuality();muiAdjustPost(4,1);cancelQuality();CHECK(mainGodRayQuality==2 && appliedQuality==2);
    muiAdjustPost(3,1);CHECK(mainGodRaysPercent==0 && mainGodRayQuality==2);
    muiAdjustPost(5,1);CHECK(mainBloomPercent==147 && mainGodRayQuality==2);
    muiAdjustPost(6,1);CHECK(mainLensFlares==0 && mainGodRayQuality==2);
    puts("PASS: quality defaults, clamp, presets, cyclic UI, config roundtrip, cancel and independent toggle/bloom/lens controls");return 0;
}
'''
        compilers = list(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC compiler missing')
        compiler = sorted(compilers)[-1]
        vcvars = compiler.parents[7] / 'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-godray-quality-') as directory:
            directory = Path(directory)
            (directory / 'quality.c').write_text(harness)
            command = (f'call "{vcvars}" >nul && "{compiler}" /nologo /TC '
                       '/D_CRT_SECURE_NO_WARNINGS quality.c /Fe:quality.exe && quality.exe')
            result = subprocess.run(command, shell=True, cwd=directory, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout)


if __name__ == '__main__':
    unittest.main()
