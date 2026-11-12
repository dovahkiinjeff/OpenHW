"""Exercise the actual window-mode routine without changing any display mode."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from tests.test_shared_dds_cache import build_and_run

ROOT=Path(__file__).resolve().parents[1]


class RefreshRatePersistenceTests(unittest.TestCase):
    def test_requested_refresh_survives_desktop_and_fallback_modes(self):
        source=(ROOT/'src/SDL/render.c').read_text(encoding='utf-8')
        function='static bool32 rndApplyWindowMode(void)'+source.split(
            'static bool32 rndApplyWindowMode(void)',1)[1].split(
            '\n/* Should remove this stuff',1)[0]
        stubs=r'''
typedef int bool32;
#define TRUE 1
#define FALSE 0
#define SDL_WINDOW_FULLSCREEN 1
#define SDL_WINDOW_FULLSCREEN_DESKTOP 4097
#define HW_DIAG_FPRINTF(...) ((void)0)
struct SDL_DisplayMode { unsigned format; int w,h,refresh_rate; void* driverdata; };
int fullScreen=1,mainExclusiveFullscreen=0,mainRefreshRate=360,mainActiveRefreshRate=0;
int MAIN_WindowWidth=1920,MAIN_WindowHeight=1080;
int display=2,desktopHz=60,closestHz=360,unsupported=0,lastFlags=0,lastRequested=0,lastDisplay=-1,failSwitch=0;
void* sdlwindow=nullptr;
int SDL_GetWindowDisplayIndex(void*){return display;}
int SDL_GetCurrentDisplayMode(int i,SDL_DisplayMode* m){lastDisplay=i;m->refresh_rate=desktopHz;return 0;}
int SDL_GetDesktopDisplayMode(int i,SDL_DisplayMode* m){lastDisplay=i;*m={0,1920,1080,desktopHz,nullptr};return 0;}
SDL_DisplayMode* SDL_GetClosestDisplayMode(int i,const SDL_DisplayMode* wanted,SDL_DisplayMode* result){lastDisplay=i;lastRequested=wanted->refresh_rate;if(unsupported)return nullptr;*result=*wanted;result->refresh_rate=closestHz;return result;}
int SDL_SetWindowDisplayMode(void*,const SDL_DisplayMode*){return failSwitch?-1:0;}
int SDL_SetWindowFullscreen(void*,unsigned flags){lastFlags=flags;return failSwitch?-1:0;}
const char* SDL_GetError(){return "fixture";}
'''
        cases=r'''
int main(){
 assert(rndApplyWindowMode()&&mainRefreshRate==360&&mainActiveRefreshRate==60&&lastFlags==SDL_WINDOW_FULLSCREEN_DESKTOP&&lastDisplay==2);
 assert(rndApplyWindowMode()&&mainRefreshRate==360); // A later launch must retain the request.
 fullScreen=0;lastFlags=0;assert(rndApplyWindowMode()&&mainRefreshRate==360&&mainActiveRefreshRate==60&&lastFlags==0);
 fullScreen=mainExclusiveFullscreen=1;assert(rndApplyWindowMode()&&mainRefreshRate==360&&mainActiveRefreshRate==360&&lastRequested==360&&lastFlags==SDL_WINDOW_FULLSCREEN);
 unsupported=1;assert(rndApplyWindowMode()&&mainRefreshRate==360&&mainActiveRefreshRate==60);
 unsupported=0;closestHz=240;assert(rndApplyWindowMode()&&mainRefreshRate==360&&mainActiveRefreshRate==240);
 mainRefreshRate=0;assert(rndApplyWindowMode()&&mainRefreshRate==0&&mainActiveRefreshRate==240);
 mainRefreshRate=360;failSwitch=1;assert(!rndApplyWindowMode()&&mainRefreshRate==360);
 failSwitch=0;fullScreen=0;display=-1;assert(rndApplyWindowMode()&&mainRefreshRate==360&&lastDisplay==0);
 std::puts("PASS: borderless/windowed request retention, exact exclusive refresh, fallback, Auto, failure and display selection");
}
'''
        with tempfile.TemporaryDirectory(prefix='openhw-refresh-') as tmp:
            result=build_and_run(stubs+function,cases,Path(tmp))
            self.assertIn('PASS:',result.stdout)
            print(result.stdout.strip())


if __name__=='__main__':unittest.main()
