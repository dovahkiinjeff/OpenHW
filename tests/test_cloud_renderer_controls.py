"""Exercise the production map editor cloud controls and persisted records."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def block_after(text, marker):
    start = text.index(marker)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class CloudRendererControlsTests(unittest.TestCase):
    def test_production_control_and_map_record_roundtrip(self):
        editor = (ROOT / 'src/Game/ModernMapEditor.c').read_text()
        numeric = block_after(editor.split('static bool32 mmeNumericValue(', 1)[1],
                              'if(mme.tab==MME_TAB_CLOUDS)')
        adjust = block_after(editor.split('static bool32 mmeAdjustSelection(', 1)[1],
                             'if (mme.tab == MME_TAB_CLOUDS)')
        harness = r'''
#include "CloudRenderSettings.h"
#include <assert.h>
#include <stdlib.h>
typedef int bool32,sdword;
typedef float real32;
#define TRUE 1
#define FALSE 0
#define MME_TAB_CLOUDS 6
static struct {int tab,inspectorRow;} mme={MME_TAB_CLOUDS,0};
static HWModernCloudRenderSettings current;
void hwModernGraphicsGetCloudRenderSettings(HWModernCloudRenderSettings *settings){*settings=current;}
void hwModernGraphicsSetCloudRenderSettings(const HWModernCloudRenderSettings *settings){current=hwModernCloudRenderSettingsSanitize(settings);}
void hwModernGraphicsResetCloudRenderSettings(void){current=hwModernCloudRenderSettingsDefaults();}
static void mmeStatus(const char *text){(void)text;}
static int mmeClamp(int v,int low,int high){return v<low?low:v>high?high:v;}
static float mmeClampF(float v,float low,float high){return v<low?low:v>high?high:v;}
static int numeric(double *number,int apply){
    float *target=NULL;double minimum=0,maximum=8;int row=mme.inspectorRow;
NUMERIC_BLOCK
    return FALSE;
}
static int adjust(int direction,float fine,float coarse){
ADJUST_BLOCK
    return FALSE;
}
static void exact(int row,double value,int expected){
    mme.inspectorRow=row;assert(numeric(&value,TRUE)==expected);
}
int main(void){
    char line[256];FILE *file;HWModernCloudRenderSettings loaded,before;
    double value=0;
    hwModernGraphicsResetCloudRenderSettings();
    assert(current.samplingQuality==0 && current.detail==3 && current.motionEnabled==0);
    assert(current.motionSpeed==1 && current.billowStrength==1 && current.fieldCoverage==.8f);
    mme.inspectorRow=3;assert(!numeric(&value,FALSE));
    assert(adjust(1,1,1));assert(current.motionSpeed==1 && current.motionEnabled==0);
    exact(0,-4.123456789,TRUE);assert(fabs(current.samplingQuality+4.123456789)<.000001);
    before=current;exact(0,-5.01,FALSE);assert(!memcmp(&current,&before,sizeof current));
    exact(0,NAN,FALSE);exact(0,INFINITY,FALSE);exact(0,10.01,FALSE);
    exact(1,0,TRUE);assert(current.detail==0);exact(1,2.5,FALSE);exact(1,4,FALSE);
    exact(1,3,TRUE);assert(current.detail==3);
    mme.inspectorRow=2;assert(adjust(1,1,1));assert(current.motionEnabled==1);
    exact(3,.000123456,TRUE);assert(fabs(current.motionSpeed-.000123456)<.000000001);
    exact(3,-.1,FALSE);exact(3,5.1,FALSE);
    mme.inspectorRow=3;assert(adjust(1,.1f,1));assert(fabs(current.motionSpeed-.005123456)<.00000001);
    exact(4,0,TRUE);assert(current.billowStrength==0);exact(4,2,TRUE);exact(4,2.01,FALSE);
    exact(5,.123456789,TRUE);assert(fabs(current.fieldCoverage-.123456789)<.00000001);
    exact(5,-.001,FALSE);exact(5,1.001,FALSE);exact(5,NAN,FALSE);exact(5,INFINITY,FALSE);
    mme.inspectorRow=5;assert(adjust(1,.1f,1));assert(fabs(current.fieldCoverage-.124456789)<.00000001);
    file=tmpfile();assert(file);assert(hwModernCloudRenderSettingsWrite(file,&current));
    rewind(file);assert(fgets(line,sizeof line,file));fclose(file);
    assert(!strncmp(line,"CLOUD_RENDERER 2 ",17));
    loaded=hwModernCloudRenderSettingsDefaults();
    assert(hwModernCloudRenderSettingsRead(line,&loaded));assert(!memcmp(&current,&loaded,sizeof current));
    before=loaded;
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 3 1 1 1 1 1\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 nan 1 1 1 1\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 1 1 1 inf 1\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 1 1 1 1\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 1 1 1 1 1 garbage\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 1 1 1 1 1 .9\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1 nan\n",&loaded));
    assert(!hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1 inf\n",&loaded));
    assert(!memcmp(&loaded,&before,sizeof loaded));
    assert(hwModernCloudRenderSettingsRead("CLOUD_RENDERER 1 -80 90 19 80 -8\r\n",&loaded));
    assert(loaded.samplingQuality==-5 && loaded.detail==3 && loaded.motionEnabled==1);
    assert(loaded.motionSpeed==5 && loaded.billowStrength==0);
    assert(loaded.fieldCoverage==.8f);
    assert(hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1 .987654321\n",&loaded));
    assert(fabs(loaded.fieldCoverage-.987654321)<.00000003);
    file=tmpfile();assert(file);assert(hwModernCloudRenderSettingsWrite(file,&loaded));
    rewind(file);assert(fgets(line,sizeof line,file));fclose(file);
    before=loaded;assert(hwModernCloudRenderSettingsRead(line,&loaded));
    assert(!memcmp(&loaded,&before,sizeof loaded));
    assert(hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1 8\n",&loaded));
    assert(loaded.fieldCoverage==1);
    assert(hwModernCloudRenderSettingsRead("CLOUD_RENDERER 2 1 1 1 1 1 -8\n",&loaded));
    assert(loaded.fieldCoverage==0);
    /* Loading an older next map starts from defaults, not previous map motion. */
    hwModernGraphicsResetCloudRenderSettings();
    assert(!hwModernCloudRenderSettingsRead("DUST_FADE 65000 1\n",&current));
    assert(current.motionEnabled==0 && current.samplingQuality==0 && current.detail==3);
    current.motionEnabled=1;current.billowStrength=.25f;current.fieldCoverage=.25f;mme.inspectorRow=6;
    assert(adjust(1,1,1));assert(current.motionEnabled==0 && current.billowStrength==1 && current.fieldCoverage==.8f);
    loaded=current;loaded.samplingQuality=NAN;loaded.motionSpeed=INFINITY;
    loaded.billowStrength=-INFINITY;loaded.fieldCoverage=NAN;loaded=hwModernCloudRenderSettingsSanitize(&loaded);
    assert(loaded.samplingQuality==0 && loaded.motionSpeed==1 && loaded.billowStrength==1 && loaded.fieldCoverage==.8f);
    return 0;
}
'''.replace('NUMERIC_BLOCK', numeric).replace('ADJUST_BLOCK', adjust)
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        instance = re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]
        setup = Path(instance) / 'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-cloud-controls-') as tmp:
            folder = Path(tmp)
            (folder / 'test.c').write_text(harness)
            (folder / 'header.cpp').write_text(
                '#include "CloudRenderSettings.h"\n'
                'int main(){return hwModernCloudRenderSettingsDefaults().motionEnabled;}\n')
            command = (
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && '
                f'cl /nologo /std:c11 /W3 /O2 /I"{ROOT / "src/Modern"}" '
                'test.c /Fe:test.exe && '
                f'cl /nologo /std:c++17 /W3 /O2 /I"{ROOT / "src/Modern"}" '
                'header.cpp /Fe:header.exe"')
            result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(folder / 'test.exe')], cwd=folder, check=True)
            subprocess.run([str(folder / 'header.exe')], cwd=folder, check=True)

    def test_reset_and_simulation_clock_are_in_gameplay_path(self):
        editor = (ROOT / 'src/Game/ModernMapEditor.c').read_text()
        load = block_after(editor, 'static bool32 mmeLoadMapOverlay(void)')
        self.assertLess(load.index('hwModernGraphicsResetCloudRenderSettings();'),
                        load.index('fp = fopen(path, "rb");'))
        update = block_after(editor, 'void modernMapEditorCameraUpdate(void)')
        self.assertIn('universe.totaltimeelapsed', update)
        self.assertLess(update.index('hwModernGraphicsSetCloudSimulationTime('),
                        update.index('if (gameIsRunning && mmeVolumeMission'))
        self.assertIn('hwModernGraphicsResetCloudRenderSettings();', update)
        self.assertIn('hwModernCloudRenderSettingsWrite(fp, &settings);',
                      block_after(editor, 'static bool32 mmeSaveMap(void)'))


if __name__ == '__main__':
    unittest.main()
