/* The editor DLL creates a WebView directly inside SDL's existing client area. */
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#include "../Modern/OpenHWShipDocument.h"
extern SDL_Window *sdlwindow;
static char hodNotice[192];
static HMODULE hodModule;
static int hodVisible,hodAddedClip;
static HWND hodParent;
static void (__cdecl *hodCloseView)(void);
typedef int (__cdecl *HodComplete)(const char *,const char *,size_t,char *,size_t,char *,size_t);
static int (__cdecl *hodOpenView)(intptr_t,HodComplete);
static int (__cdecl *hodTickView)(void);

static int __cdecl hodCompleteDefinition(const char *root,const char *bytes,size_t count,
                                        char *out,size_t capacity,char *error,size_t errorSize)
{
    void *complete=NULL;
    size_t length=0;
    if(!meshCompleteOpenHWDefinition(root,bytes,count,&complete,&length,error,errorSize))return 0;
    if(length>=capacity) {
        snprintf(error,errorSize,"Completed ship definition exceeds the editor limit.");
        openhwShipDocumentFree(complete);
        return 0;
    }
    memcpy(out,complete,length);
    out[length]=0;
    openhwShipDocumentFree(complete);
    return 1;
}
static void hodWorkshopClose(void)
{
    if(!hodVisible)return;
    hodCloseView();
    hodVisible=0;
    if(IsWindow(hodParent)){
        if(hodAddedClip)SetWindowLongPtrW(hodParent,GWL_STYLE,GetWindowLongPtrW(hodParent,GWL_STYLE)&~WS_CLIPCHILDREN);
        SetFocus(hodParent);
    }
}
static int hodWorkshopActive(void)
{
    if(hodVisible && !hodTickView())hodWorkshopClose();
    return hodVisible;
}
static void hodWorkshopOpen(void)
{
    SDL_SysWMinfo info;
    wchar_t dll[32768],*slash;
    DWORD n;
    if(hodVisible || !hwLiveMapAuthoringEnabled())return;
    SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(sdlwindow,&info))return;
    if(!hodModule){
        n=GetModuleFileNameW(NULL,dll,_countof(dll));
        if(!n || n>=_countof(dll) || !(slash=wcsrchr(dll,L'\\')))return;
        if(wcscpy_s(slash,_countof(dll)-(slash-dll),L"\\Data\\Workshop\\OpenHWHodEditor.dll"))return;
        hodModule=LoadLibraryExW(dll,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(hodModule){
            hodOpenView=(void*)GetProcAddress(hodModule,"hod_open");
            hodCloseView=(void*)GetProcAddress(hodModule,"hod_close");
            hodTickView=(void*)GetProcAddress(hodModule,"hod_tick");
        }
    }
    if(!hodCloseView || !hodOpenView || !hodTickView || !hodOpenView((intptr_t)info.info.win.window,hodCompleteDefinition)){
        snprintf(hodNotice,sizeof(hodNotice),"Could not open the HOD editor. Check the workshop installation and WebView2 runtime.");return;
    }
    hodParent=info.info.win.window;
    hodAddedClip=!(GetWindowLongPtrW(hodParent,GWL_STYLE)&WS_CLIPCHILDREN);
    if(hodAddedClip)SetWindowLongPtrW(hodParent,GWL_STYLE,GetWindowLongPtrW(hodParent,GWL_STYLE)|WS_CLIPCHILDREN);
    hodVisible=1;hodNotice[0]=0;
}
