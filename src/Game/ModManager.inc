#include "../Modern/OpenHWMods.h"
static struct {OpenHWModEntry rows[128];int count,scroll,drag,creating;char name[65],notice[256];} modUI;
static void modUIOpen(void){char*root=SDL_GetBasePath();memset(&modUI,0,sizeof(modUI));modUI.drag=-1;
 if(root){modUI.count=openhwModsRead(root,modUI.rows,128,modUI.notice,sizeof(modUI.notice));SDL_free(root);}if(modUI.count<0)modUI.count=0;
 mpeReleaseCamera();mpe.stage=5;
}
static rectangle modUICreateButton(void){return muiRect(MAIN_WindowWidth-uiScaleSize(270),uiScaleSize(25),MAIN_WindowWidth-uiScaleSize(40),uiScaleSize(70));}
static void modUICreate(void){
 char *root=SDL_GetBasePath();
 if(!root)return;
 if(openhwModCreate(root,modUI.name,modUI.notice,sizeof(modUI.notice))){
  modUI.count=openhwModsRead(root,modUI.rows,128,modUI.notice,sizeof(modUI.notice));
  modUI.creating=0;
  snprintf(modUI.notice,sizeof(modUI.notice),"Mod created and enabled. Restart to apply.");
 }
 SDL_free(root);
}
static int modUIVisible(void){return (MAIN_WindowHeight-uiScaleSize(230))/uiScaleSize(42);}
static void modUIStore(void){char*root=SDL_GetBasePath();if(!root)return;if(openhwModsSave(root,modUI.rows,modUI.count,modUI.notice,sizeof(modUI.notice)))snprintf(modUI.notice,sizeof(modUI.notice),"Saved. Restart OpenHW to apply changes.");SDL_free(root);}
static void modUIDraw(void){rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight);int i;muiConceptPanel(MUI_ART_TAIIDAN_DARK,&whole,208);
 muiText(uiScaleSize(40),uiScaleSize(35),muiSignal,"MOD MANAGER",muiHeadingFont);
 {rectangle button=modUICreateButton();muiDrawButtonRect(&button,"CREATE MOD",FALSE,muiPointIn(&button,mouseCursorX(),mouseCursorY()));}
 if(modUI.creating){
  char text[180];snprintf(text,sizeof(text),"MOD NAME / %s_   ENTER to create / ESC to cancel",modUI.name);
  muiText(uiScaleSize(40),uiScaleSize(105),muiSignal,text,muiBodyFont);
 }
 muiText(uiScaleSize(40),uiScaleSize(85),muiPaper,"Checkbox: enable / disable. Drag rows to reorder. Lower mods override higher mods.",muiBodyFont);
 for(i=modUI.scroll;i<modUI.count&&i<modUI.scroll+modUIVisible();i++){
  int y=uiScaleSize(130+(i-modUI.scroll)*42);rectangle row=muiRect(uiScaleSize(40),y,MAIN_WindowWidth-uiScaleSize(40),y+uiScaleSize(38));char label[100];
  snprintf(label,sizeof(label),"%s  %s",modUI.rows[i].enabled?"[X]":"[ ]",modUI.rows[i].name);muiDrawButtonRect(&row,label,i==modUI.drag,muiPointIn(&row,mouseCursorX(),mouseCursorY()));
 }
 muiText(uiScaleSize(40),MAIN_WindowHeight-uiScaleSize(85),muiMuted,modUI.notice,muiBodyFont);
 muiText(uiScaleSize(40),MAIN_WindowHeight-uiScaleSize(45),muiPaper,"ESC / BACK",muiBodyFont);
}
static udword modUIProcess(smemsize ID,udword event){int x=mouseCursorX(),y=mouseCursorY(),index=modUI.scroll+(y-uiScaleSize(130))/uiScaleSize(42);
 if(event==RPE_KeyDown){
  if(modUI.creating){
   size_t length=strlen(modUI.name);char ch=0;
   if(ID==ESCKEY){modUI.creating=0;return RPR_Redraw;}
   if(ID==RETURNKEY){modUICreate();return RPR_Redraw;}
   if(ID==BACKSPACEKEY){if(length)modUI.name[length-1]=0;return RPR_Redraw;}
   if(ID>=AKEY&&ID<=ZKEY)ch='a'+(char)(ID-AKEY);
   if(ID>=SDL_SCANCODE_1&&ID<=SDL_SCANCODE_9)ch='1'+(char)(ID-SDL_SCANCODE_1);
   if(ID==ZEROKEY)ch='0';if(ID==MINUSKEY)ch='-';
   if(ch&&length<64){modUI.name[length]=ch;modUI.name[length+1]=0;}
   return RPR_Redraw;
  }
  if(ID==ESCKEY)mpe.stage=1;return RPR_Redraw;
 }
 if(event==RPE_PressLeft){rectangle button=modUICreateButton();if(muiPointIn(&button,x,y)){modUI.creating=1;modUI.name[0]=0;return RPR_Redraw;}}
 if(modUI.creating)return RPR_Redraw;
 if(event==RPE_WheelUp||event==RPE_WheelDown){modUI.scroll=muiClamp(modUI.scroll+(event==RPE_WheelUp?-1:1),0,modUI.count>modUIVisible()?modUI.count-modUIVisible():0);return RPR_Redraw;}
 if(event==RPE_ReleaseLeft){if(modUI.drag>=0){modUI.drag=-1;modUIStore();}return RPR_Redraw;}
 if(y<uiScaleSize(130)||y>=MAIN_WindowHeight-uiScaleSize(100)||x<uiScaleSize(40)||x>=MAIN_WindowWidth-uiScaleSize(40)||index<0||index>=modUI.count)return RPR_Redraw;
 if(event==RPE_PressLeft){if(x<uiScaleSize(95)){modUI.rows[index].enabled=!modUI.rows[index].enabled;modUIStore();}else modUI.drag=index;}
 if(event==RPE_HoldLeft&&modUI.drag>=0&&index!=modUI.drag){OpenHWModEntry moving=modUI.rows[modUI.drag];if(index>modUI.drag)memmove(&modUI.rows[modUI.drag],&modUI.rows[modUI.drag+1],(index-modUI.drag)*sizeof(moving));else memmove(&modUI.rows[index+1],&modUI.rows[index],(modUI.drag-index)*sizeof(moving));modUI.rows[index]=moving;modUI.drag=index;}
 return RPR_Redraw;
}
