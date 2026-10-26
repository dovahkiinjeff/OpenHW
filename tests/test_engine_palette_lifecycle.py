"""Execute the production engine-material palette registration and cleanup."""
from pathlib import Path
import os,re,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
def function(source,name):
    match=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    start=match.start();a=source.index('{',match.start());depth=1;i=a+1
    while depth:
        depth+=(source[i]=='{')-(source[i]=='}');i+=1
    return source[start:i]+'\n'

class EnginePaletteLifecycle(unittest.TestCase):
    @unittest.skipUnless(os.name=='nt','MSVC required')
    def test_actual_palette_registration_recolor_and_failure_cleanup(self):
        source=(ROOT/'src/Game/Mesh.c').read_text()
        pair=re.search(r'typedef struct meshEngineMaterialPair \{.*?\} meshEngineMaterialPair;',source,re.S).group()
        harness=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int sdword; typedef int bool32; typedef uint32_t trhandle;
#define MAX_MULTIPLAYER_PLAYERS 8
#define TR_Invalid UINT32_MAX
#define TRUE 1
#define NonVolatile 0
#define HW_ENABLE_D3D12_BACKEND 1
#define FL_Path 260
#define DBG_Loc 0
#define dbgAssertOrIgnore assert
typedef struct {uint32_t base,detail;} trcolorinfo;
typedef struct {trcolorinfo textureColor;} ColorScheme;
static ColorScheme teColorSchemes[8];
typedef struct {int teamColor[8];} ShipStaticInfo;
typedef ShipStaticInfo StaticInfo;
static ShipStaticInfo info;
static int utyShipsAlwaysUseOwnerColors,failSurface,allocations,unregisters,registrations;
typedef struct {char *textureNameSave; trhandle *texture; int bTexturesRegistered;} materialentry;
typedef struct {materialentry *localMaterial; int nLocalMaterials,nPublicMaterials;char*fileName;} meshdata;
static trcolorinfo colors[1024];static int usage[1024];
static void*memAlloc(size_t n,const char*name,int flags){void*p=malloc(n);assert(p);++allocations;return p;}
static void memFree(void*p){if(p){--allocations;free(p);}}
static StaticInfo*meshNameToStaticInfo(char*n){return &info;}
static int meshStaticColorSchemeEnabled(StaticInfo*i,const char*n,int scheme){return i->teamColor[scheme];}
static int meshModernSurfaceDiagEnabled(void){return 0;}
static int meshNativeVisualParameters(meshdata*m,float*s,unsigned*l){return 1;}
static int fileExists(char*p,int flags){return 1;}
static trhandle trTextureRegister(char*n,trcolorinfo*c,void*m){assert(c);assert(strstr(n,"_engineOn"));++registrations;colors[registrations]=*c;usage[registrations]=1;return registrations;}
static void trTextureUnregister(trhandle h){assert(h<1024&&usage[h]==1);usage[h]=0;++unregisters;}
static int meshRegisterModernSurfaceTexture(materialentry*m,char*n){return !failSurface;}
static void hwModernGraphicsUnregisterSurfaceTexture(void*m){}
static void meshTextureNameToPath(char*out,char*mesh,char*tex){snprintf(out,260,"R2\\HeavyCruiser\\RMNative\\%s",tex);}
'''
        methods=(pair+'\nstatic meshEngineMaterialPair *meshEngineMaterials[256];\n'+
            function(source,'meshEngineMaterialBucket')+function(source,'meshTextureRegisterAllPlayers')+
            function(source,'meshRegisterEngineMaterials')+function(source,'meshFreeEngineMaterials'))
        cases=r'''
static int verify(meshdata*mesh){int n=0;unsigned b;for(b=0;b<256;++b){meshEngineMaterialPair*p=meshEngineMaterials[b];for(;p;p=p->next){int s;assert(p->active.texture==p->handles&&p->active.bTexturesRegistered);for(s=0;s<8;++s){trhandle h=p->handles[s];if(info.teamColor[s]){assert(h!=TR_Invalid&&usage[h]);assert(colors[h].base==teColorSchemes[s].textureColor.base);assert(colors[h].detail==teColorSchemes[s].textureColor.detail);}else assert(h==TR_Invalid);}++n;}}return n;}
int main(void){
 materialentry materials[2]={{"c0_m0",0,0},{"c0_m1",0,0}};
 meshdata mesh={materials,0,2,"R2\\HeavyCruiser\\RMNative\\HeavyCruiser.geo"};int i;
 for(i=0;i<8;++i){teColorSchemes[i].textureColor.base=0xff007000u+i;teColorSchemes[i].textureColor.detail=0xff000040u+i;}
 teColorSchemes[1].textureColor.base=0xff00d2ffu;teColorSchemes[1].textureColor.detail=0xff0000ffu;
 info.teamColor[1]=info.teamColor[2]=info.teamColor[4]=1;
 meshRegisterEngineMaterials(&mesh,mesh.fileName);assert(verify(&mesh)==2&&registrations==6);meshFreeEngineMaterials(&mesh);assert(allocations==0&&unregisters==6);
 // New mission custom palette and a captured ship's original scheme coexist.
 teColorSchemes[2].textureColor.base=0xffab5500u;info.teamColor[5]=1;
 meshRegisterEngineMaterials(&mesh,mesh.fileName);assert(verify(&mesh)==2&&registrations==14);meshFreeEngineMaterials(&mesh);assert(allocations==0&&unregisters==14);
 // Every per-scheme registration is balanced even when DXR binding fails.
 failSurface=1;meshRegisterEngineMaterials(&mesh,mesh.fileName);assert(verify(&mesh)==0&&allocations==0&&unregisters==registrations);
 failSurface=0;memset(&info,0,sizeof(info));meshRegisterEngineMaterials(&mesh,mesh.fileName);assert(verify(&mesh)==0&&allocations==0);
 for(i=1;i<=registrations;++i)assert(!usage[i]);
 puts("PASS: engine-active hulls preserve exact default/mission/captured palettes; no neutral substitution; balanced failure, recolor and destruction ownership");return 0;
}
'''
        # Run the real recolor tail's rebuild sequence after palette updates.
        recolor=function(source,'meshRecolorize')
        self.assertIn('meshFreeEngineMaterials(mesh);\n    meshRegisterEngineMaterials(mesh,mesh->fileName);',recolor)
        cache=(ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup=Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)',cache)[1])/'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-engine-palettes-') as tmp:
            folder=Path(tmp);(folder/'test.c').write_text(harness+methods+cases)
            result=subprocess.run(f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /std:c11 /W4 /O2 test.c /Fe:test.exe"',cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            subprocess.run([str(folder/'test.exe')],cwd=folder,check=True)
if __name__=='__main__':unittest.main()
