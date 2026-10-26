#include "../Modern/OpenHWMods.h"
static struct {OpenHWModEntry rows[128];int count,scroll,drag,mode;char name[65],notice[256];} modUI;
/* mode 0 manager, 1 new mod name, 2 append picker. */
static void spePublishMod(const char*name,int append);
static void modUIOpen(int mode){char*root=SDL_GetBasePath();memset(&modUI,0,sizeof(modUI));modUI.drag=-1;modUI.mode=mode;
 if(root){modUI.count=openhwModsRead(root,modUI.rows,128,modUI.notice,sizeof(modUI.notice));SDL_free(root);}if(modUI.count<0)modUI.count=0;
 mpeReleaseCamera();mpe.stage=5;
}
static int modUIVisible(void){return (MAIN_WindowHeight-uiScaleSize(230))/uiScaleSize(42);}
static void modUIStore(void){char*root=SDL_GetBasePath();if(!root)return;if(openhwModsSave(root,modUI.rows,modUI.count,modUI.notice,sizeof(modUI.notice)))snprintf(modUI.notice,sizeof(modUI.notice),"Saved. Restart OpenHW to apply changes.");SDL_free(root);}
static void modUIDraw(void){rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight);int i;muiFill(&whole,colRGB(5,12,18));
 muiText(uiScaleSize(40),uiScaleSize(35),muiSignal,modUI.mode==1?"SAVE MOD / NAME":modUI.mode==2?"APPEND TO EXISTING MOD":"MOD MANAGER",muiHeadingFont);
 muiText(uiScaleSize(40),uiScaleSize(85),muiPaper,modUI.mode==1?"Letters, digits, underscore or hyphen. Enter to save; Esc to cancel.":modUI.mode==2?"Select a mod to append this ship. Existing conflicts require confirmation.":"Checkbox: enable / disable. Drag rows to reorder. Lower mods override higher mods.",muiBodyFont);
 if(modUI.mode==1){muiText(uiScaleSize(50),uiScaleSize(145),muiPaper,modUI.name,muiHeadingFont);}else for(i=modUI.scroll;i<modUI.count&&i<modUI.scroll+modUIVisible();i++){
  int y=uiScaleSize(130+(i-modUI.scroll)*42);rectangle row=muiRect(uiScaleSize(40),y,MAIN_WindowWidth-uiScaleSize(40),y+uiScaleSize(38));char label[100];
  snprintf(label,sizeof(label),"%s  %s",modUI.mode==2?">":modUI.rows[i].enabled?"[X]":"[ ]",modUI.rows[i].name);muiDrawButtonRect(&row,label,i==modUI.drag,muiPointIn(&row,mouseCursorX(),mouseCursorY()));
 }
 muiText(uiScaleSize(40),MAIN_WindowHeight-uiScaleSize(85),muiMuted,modUI.notice,muiBodyFont);
 muiText(uiScaleSize(40),MAIN_WindowHeight-uiScaleSize(45),muiPaper,"ESC / BACK",muiBodyFont);
}
static udword modUIProcess(smemsize ID,udword event){int x=mouseCursorX(),y=mouseCursorY(),index=modUI.scroll+(y-uiScaleSize(130))/uiScaleSize(42);
 if(event==RPE_KeyDown){if(ID==ESCKEY){mpe.stage=modUI.mode?4:1;return RPR_Redraw;}if(modUI.mode==1){size_t n=strlen(modUI.name);SDL_Keycode c=SDL_GetKeyFromScancode((SDL_Scancode)ID);
  if(ID==RETURNKEY){spePublishMod(modUI.name,0);return RPR_Redraw;}if(ID==BACKSPACEKEY){if(n)modUI.name[n-1]=0;}else if(n<64&&((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')){modUI.name[n]=(char)(c=='-'&&keyIsHit(SHIFTKEY)?'_':c);modUI.name[n+1]=0;}}
  return RPR_Redraw;}
 if(event==RPE_WheelUp||event==RPE_WheelDown){modUI.scroll=muiClamp(modUI.scroll+(event==RPE_WheelUp?-1:1),0,modUI.count>modUIVisible()?modUI.count-modUIVisible():0);return RPR_Redraw;}
 if(event==RPE_ReleaseLeft){if(modUI.drag>=0){modUI.drag=-1;modUIStore();}return RPR_Redraw;}
 if(modUI.mode==1||y<uiScaleSize(130)||y>=MAIN_WindowHeight-uiScaleSize(100)||x<uiScaleSize(40)||x>=MAIN_WindowWidth-uiScaleSize(40)||index<0||index>=modUI.count)return RPR_Redraw;
 if(event==RPE_PressLeft){if(modUI.mode==2){spePublishMod(modUI.rows[index].name,1);return RPR_Redraw;}if(x<uiScaleSize(95)){modUI.rows[index].enabled=!modUI.rows[index].enabled;modUIStore();}else modUI.drag=index;}
 if(event==RPE_HoldLeft&&modUI.drag>=0&&index!=modUI.drag){OpenHWModEntry moving=modUI.rows[modUI.drag];if(index>modUI.drag)memmove(&modUI.rows[modUI.drag],&modUI.rows[modUI.drag+1],(index-modUI.drag)*sizeof(moving));else memmove(&modUI.rows[index+1],&modUI.rows[index],(modUI.drag-index)*sizeof(moving));modUI.rows[index]=moving;modUI.drag=index;}
 return RPR_Redraw;
}
