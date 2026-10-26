#include "ShipAuthorText.h"
/* Continuous text with native clipboard semantics; no line widgets or numbers. */
static struct {ShipAuthorText text;int open,focus,ready,dirty,scroll,hscroll,drag,lastX,lastY,race,type,linked,linkHistory,matches;const char*linkedText;size_t matchStart[128],matchEnd[128];rectangle box;char project[48],notice[160];} speFile;
static int speBrowse(char*,const char*);
static int speEventView=0;
static void speFileReset(void){shipTextFree(&speFile.text);memset(&speFile,0,sizeof(speFile));}
static int speFilePending(void){return speFile.dirty;}
static rectangle speFileBody(void){return muiRect(speFile.box.x0+10,speFile.box.y0+uiScaleSize(76),speFile.box.x1-10,speFile.box.y1-uiScaleSize(36));}
static rectangle speFileButton(int i){int w=(speFile.box.x1-speFile.box.x0-16)/5;return muiRect(speFile.box.x0+8+i*w,speFile.box.y0+uiScaleSize(35),speFile.box.x0+8+(i+1)*w-3,speFile.box.y0+uiScaleSize(67));}
static int speFileWidth(char c){char text[2]={c,0};fonthandle old=fontMakeCurrent(muiBodyFont);int w=c=='\t'?fontWidth("    "):fontWidth(text);fontMakeCurrent(old);return w>0?w:uiScaleSize(4);}
static int speFileHeight(void){return uiScaleSize(24);}
static int speFileColumn(size_t start,size_t end){int x=0;while(start<end)x+=speFileWidth(speFile.text.text[start++]);return x;}
static size_t speFileLine(int row){size_t p=0;while(row-->0&&speFile.text.text[p]){p=shipTextLineEnd(&speFile.text,p);if(speFile.text.text[p])p++;}return p;}
static size_t speFileHit(int x,int y){rectangle body=speFileBody();size_t p=speFileLine(speFile.scroll+muiClamp((y-body.y0)/speFileHeight(),0,(body.y1-body.y0)/speFileHeight()));int px=body.x0-speFile.hscroll;while(speFile.text.text[p]&&speFile.text.text[p]!='\n'){int w=speFileWidth(speFile.text.text[p]);if(x<px+w/2)break;px+=w;p++;}return p;}
static void speFileReveal(void){rectangle body=speFileBody();size_t i;int row=0,x,rows=(body.y1-body.y0)/speFileHeight();for(i=0;i<speFile.text.cursor;i++)if(speFile.text.text[i]=='\n')row++;if(row<speFile.scroll)speFile.scroll=row;if(row>=speFile.scroll+rows)speFile.scroll=row-rows+1;
 x=speFileColumn(shipTextLineStart(&speFile.text,speFile.text.cursor),speFile.text.cursor);if(x<speFile.hscroll)speFile.hscroll=x;if(x>speFile.hscroll+body.x1-body.x0-12)speFile.hscroll=x-(body.x1-body.x0-12);}
static void speFileInsert(const char*s){if(speEventView)return;if(shipTextInsert(&speFile.text,s,strlen(s))){speFile.dirty=1;speFileReveal();}else snprintf(speFile.notice,160,"Text exceeds 1 MiB or contains unsupported control characters.");}
static int speFileParse(const char*s,size_t n){const char*body;size_t i;if(!shipTextLoad(&speFile.text,s,n))return 0;speFile.dirty=speFile.scroll=speFile.hscroll=0;
 /* Open complete definitions at gameplay settings, not at model plumbing.
    The model declarations remain editable by scrolling upward. */
 if(!strncmp(speFile.text.text,"OPENHW_SHIP_2",13)&&(body=strstr(speFile.text.text,"[Ship]\n"))!=NULL){for(i=0;speFile.text.text+i<body;i++)if(speFile.text.text[i]=='\n')speFile.scroll++;speFile.scroll++;}
 return 1;}
static void speFileOpen(void){char path[1024];void*data=NULL;size_t bytes;speFile.open=speFile.focus=1;mpeReleaseCamera();
 if(speEventView){speEventView=0;speFile.ready=0;}
 if(speFile.ready&&speFile.race==spe.doc.race&&speFile.type==spe.doc.type&&!strcmp(speFile.project,spe.doc.name))return;
 if(speFile.dirty){snprintf(speFile.notice,160,"Save/export or discard the previous ship file first.");return;}
 speFile.ready=1;speFile.race=spe.doc.race;speFile.type=spe.doc.type;snprintf(speFile.project,48,"%s",spe.doc.name);speFile.box=muiRect(MAIN_WindowWidth/5,MAIN_WindowHeight/5,MAIN_WindowWidth*4/5,MAIN_WindowHeight*4/5);speFileParse("",0);
 if(shipAuthorProjectPath(speFile.project,"ship.owshipdef",path,sizeof(path),0)&&speReadFile(path,&data,&bytes)){if(speFileParse(data,bytes))snprintf(speFile.notice,160,"Loaded project runtime ship definition.");free(data);return;}
 snprintf(path,sizeof(path),"%s/%s/RMNative/ship.owshipdef",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 if(fileExists(path,0)&&fileSizeGet(path,0)<=SHIP_TEXT_LIMIT){sdword n=fileLoadAlloc(path,&data,0);if(n>0&&speFileParse(data,n))snprintf(speFile.notice,160,!strncmp(speFile.text.text,"OPENHW_SHIP_2",13)?"Complete ship definition / gameplay and weapons below; model declarations above.":"LEGACY MODEL MANIFEST / complete ship update is not loaded.");if(data)memFree(data);return;}
 if(shipAuthorProjectPath(speFile.project,"gameplay.shp",path,sizeof(path),0)&&speReadFile(path,&data,&bytes)){if(speFileParse(data,bytes))snprintf(speFile.notice,160,"Loaded project definition.");free(data);return;}
 snprintf(path,sizeof(path),"%s\\%s.shp",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 if(fileExists(path,0)&&fileSizeGet(path,0)<=SHIP_TEXT_LIMIT){sdword n=fileLoadAlloc(path,&data,0);if(n>0&&speFileParse(data,n))snprintf(speFile.notice,160,"Loaded source definition; edits save to your project.");if(data)memFree(data);}else snprintf(speFile.notice,160,"Import a ship definition to begin.");
}
static int speFileWrite(const char*path){char temp[1100],backup[1100];const char*p=speFile.text.text;FILE*f;int ok=1;snprintf(temp,sizeof(temp),"%s.tmp",path);f=fopen(temp,"wb");if(!f)return 0;
 while(*p){if(*p=='\n'&&fputc('\r',f)==EOF)ok=0;if(fputc((unsigned char)*p++,f)==EOF)ok=0;}if(fclose(f))ok=0;if(!ok){remove(temp);return 0;}
 if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES){snprintf(backup,sizeof(backup),"%s.bak-%llu",path,(unsigned long long)SDL_GetPerformanceCounter());if(!CopyFileA(path,backup,TRUE)){remove(temp);return 0;}}
 return MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
}
static int speFileValidate(void){char root[256],identity[80],error[256];
 if(strncmp(speFile.text.text,"OPENHW_SHIP_2",13))return 1;
 snprintf(root,sizeof(root),"%s/%s/RMNative/",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 snprintf(identity,sizeof(identity),"%s%s",spe.doc.race==R2?"Taiidan_":"",ShipTypeToStr(spe.doc.type));
 if(!meshValidateOpenHWDefinition(root,identity,speFile.text.text,strlen(speFile.text.text),error,sizeof(error))){snprintf(speFile.notice,sizeof(speFile.notice),"Invalid definition: %.130s",error);return 0;}return 1;
}
static void speFileAction(int action){char path[1024];
 if(speEventView){if(action==4)speFile.open=0;else snprintf(speFile.notice,160,"Connected event data is read-only. Select text and Ctrl+C to copy.");return;}
 if(action==0){if(speFile.dirty){snprintf(speFile.notice,160,"Save/export or discard changes before importing.");return;}if(speBrowse(path,"Ship definitions\0*.owshipdef;*.shp;*.ship;*.txt\0\0")){void*data;size_t n;if(speReadFile(path,&data,&n)){if(speFileParse(data,n))speFile.dirty=1;else snprintf(speFile.notice,160,"Unsupported text or exceeds 1 MiB.");free(data);}}}
 if(action==1||action==2){int chosen=0;const int native=!strncmp(speFile.text.text,"OPENHW_SHIP_",12);if(action==1)chosen=shipAuthorProjectPath(speFile.project,native?"ship.owshipdef":"gameplay.shp",path,sizeof(path),1);else {OPENFILENAMEA d;memset(&d,0,sizeof(d));snprintf(path,sizeof(path),"%s.%s",speFile.project,native?"owshipdef":"shp");d.lStructSize=sizeof(d);d.hwndOwner=GetActiveWindow();d.lpstrFilter="OpenHW definition\0*.owshipdef\0Gameplay definition\0*.shp\0RM ship definition\0*.ship\0All files\0*.*\0\0";d.lpstrFile=path;d.nMaxFile=sizeof(path);d.lpstrDefExt=native?"owshipdef":"shp";d.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;chosen=GetSaveFileNameA(&d)!=0;}
  if(chosen){if(!speFileValidate())return;if(speFileWrite(path)){speFile.dirty=0;snprintf(speFile.notice,160,"Saved / external definition, not applied to live gameplay.");}else snprintf(speFile.notice,160,"Save failed; existing file preserved.");}}
 if(action==3){speFile.dirty=speFile.ready=0;speFileOpen();}if(action==4)speFile.open=0;
}
static int speFileNameChar(char c){return isalnum((unsigned char)c)||c=='_'||c=='.'||c=='-';}
static void speFileLink(void){unsigned i;int selected=spe.selected;speFile.matches=0;
 if(selected<0||selected>=(int)spe.doc.pointCount)return;
 for(i=0;i<spe.doc.pointCount&&speFile.matches<128;i++){int p=i;unsigned steps=0;const char*names[2];int name;
  while(p>=0&&p!=selected&&steps++<spe.doc.pointCount)p=shipAuthorParent(&spe.doc,p);if(p!=selected)continue;
  names[0]=spe.doc.points[i].name;names[1]=NULL;if(spe.mesh&&spe.doc.points[i].object>=0&&spe.doc.points[i].object<spe.mesh->nPolygonObjects)names[1]=spe.mesh->object[spe.doc.points[i].object].pName;
  for(name=0;name<2;name++){size_t n,at,len;if(!names[name]||!*(names[name])||(name&& !strcmp(names[0],names[1])))continue;n=strlen(names[name]);len=strlen(speFile.text.text);
   for(at=0;at+n<=len&&speFile.matches<128;at++)if((!at||!speFileNameChar(speFile.text.text[at-1]))&&!speFileNameChar(speFile.text.text[at+n])&&!_strnicmp(speFile.text.text+at,names[name],n)){speFile.matchStart[speFile.matches]=at;speFile.matchEnd[speFile.matches++]=at+n;}
  }
 }
 if(speFile.matches){size_t at,first=speFile.matchStart[0];int row=0;for(i=1;i<(unsigned)speFile.matches;i++)if(speFile.matchStart[i]<first)first=speFile.matchStart[i];for(at=0;at<first;at++)if(speFile.text.text[at]=='\n')row++;speFile.scroll=row>2?row-2:0;speFile.hscroll=0;snprintf(speFile.notice,160,"Highlighted joint references for %.63s and its children.",spe.doc.points[selected].name);}
 else snprintf(speFile.notice,160,"No direct reference for %.55s in this definition.",spe.doc.points[selected].name);
}
static void speFileDraw(void){static const char*labels[]={"IMPORT","SAVE","EXPORT","DISCARD","CLOSE"};rectangle body,title;int row,rows,i;size_t p,a,b;char titleText[128];if(!speFile.open)return;
 if(speEventView){speFile.matches=0;speFile.linked=spe.selected;speFile.linkHistory=spe.historyAt;speFile.linkedText=speFile.text.text;}
 if(speFile.linked!=spe.selected||speFile.linkHistory!=spe.historyAt||speFile.linkedText!=speFile.text.text){int selectionChanged=speFile.linked!=spe.selected;int oldScroll=speFile.scroll,oldH=speFile.hscroll;speFileLink();if(!selectionChanged&&speFile.focus){speFile.scroll=oldScroll;speFile.hscroll=oldH;}speFile.linked=spe.selected;speFile.linkHistory=spe.historyAt;speFile.linkedText=speFile.text.text;}
 body=speFileBody();title=muiRect(speFile.box.x0,speFile.box.y0,speFile.box.x1,speFile.box.y0+uiScaleSize(30));muiFill(&speFile.box,colRGB(12,25,35));muiFill(&title,colRGB(28,48,60));snprintf(titleText,128,"%s / %s%s",speEventView?"EVENT FILE (READ ONLY)":"SHIP FILE",speFile.project,speFile.dirty?" *":"");muiText(title.x0+8,title.y0+6,muiPaper,titleText,muiBodyFont);for(i=0;i<5;i++){rectangle r=speFileButton(i);muiDrawButtonRect(&r,labels[i],FALSE,muiPointIn(&r,mouseCursorX(),mouseCursorY()));}
 muiFill(&body,colRGB(6,14,21));rows=(body.y1-body.y0)/speFileHeight();p=speFileLine(speFile.scroll);a=speFile.text.anchor<speFile.text.cursor?speFile.text.anchor:speFile.text.cursor;b=speFile.text.anchor>speFile.text.cursor?speFile.text.anchor:speFile.text.cursor;
 for(row=0;row<rows;row++){int x=body.x0-speFile.hscroll,y=body.y0+row*speFileHeight();for(;;){char c=speFile.text.text[p],glyph[2]={c,0};int w=(c&&c!='\n')?speFileWidth(c):uiScaleSize(7);
   {int match=0;for(i=0;i<speFile.matches;i++)if(p>=speFile.matchStart[i]&&p<speFile.matchEnd[i]){match=1;break;}if((p>=a&&p<b)||match){rectangle r=muiRect(muiClamp(x,body.x0,body.x1),y,muiClamp(x+w,body.x0,body.x1),y+speFileHeight());muiFill(&r,p>=a&&p<b?colRGB(35,80,112):colRGB(100,76,18));}}
   if(speFile.focus&&p==speFile.text.cursor&&x>=body.x0&&x<body.x1&&(SDL_GetTicks()%1000)<600)primLineThick2(x,y+2,x,y+speFileHeight()-2,1,muiPaper);
   if(!c||c=='\n')break;if(c!='\t'&&x>=body.x0&&x+w<=body.x1)muiText(x,y+2,muiPaper,glyph,muiBodyFont);x+=w;p++;
  }if(!speFile.text.text[p])break;p++;
 }
 muiText(speFile.box.x0+8,speFile.box.y1-uiScaleSize(27),muiMuted,speFile.notice,muiBodyFont);muiText(speFile.box.x1-uiScaleSize(25),speFile.box.y1-uiScaleSize(22),muiSignal,"//",muiBodyFont);
}
static void speFileCopy(int cut){size_t a=speFile.text.anchor<speFile.text.cursor?speFile.text.anchor:speFile.text.cursor,b=speFile.text.anchor>speFile.text.cursor?speFile.text.anchor:speFile.text.cursor;char*s;if(a==b)return;s=malloc((b-a)*2+1);if(s){size_t i,j=0;for(i=a;i<b;i++){if(speFile.text.text[i]=='\n')s[j++]='\r';s[j++]=speFile.text.text[i];}s[j]=0;if(SDL_SetClipboardText(s)==0&&cut)speFileInsert("");free(s);}}
static int speFileProcess(smemsize ID,udword event,int x,int y){int inside,i;rectangle body;if(!speFile.open)return 0;inside=muiPointIn(&speFile.box,x,y);body=speFileBody();
 if(event==RPE_KeyDown&&speFile.focus){size_t pos=speFile.text.cursor,len=strlen(speFile.text.text);int ctrl=keyIsHit(CONTROLKEY),shift=keyIsHit(SHIFTKEY),move=0;
  if(ID==ESCKEY){speFile.open=0;return 1;}if(ctrl){if(ID==SKEY)speFileAction(1);else if(ID==AKEY){speFile.text.anchor=0;speFile.text.cursor=len;}else if(ID==CKEY||ID==XKEY)speFileCopy(ID==XKEY);else if(ID==VKEY){char*t=SDL_GetClipboardText();if(t){speFileInsert(t);SDL_free(t);}}else if(ID==ZKEY||ID==YKEY){if(!speEventView&&shipTextUndo(&speFile.text,ID==YKEY||shift)){speFile.dirty=1;speFileReveal();}}else if(ID!=SDL_SCANCODE_HOME&&ID!=SDL_SCANCODE_END)return 1;else move=1;if(!move)return 1;}
  if(ID==SDL_SCANCODE_LEFT){if(!shift&&speFile.text.anchor!=pos)pos=pos<speFile.text.anchor?pos:speFile.text.anchor;else if(pos)pos--;move=1;}
  if(ID==SDL_SCANCODE_RIGHT){if(!shift&&speFile.text.anchor!=pos)pos=pos>speFile.text.anchor?pos:speFile.text.anchor;else if(pos<len)pos++;move=1;}
  if(ID==SDL_SCANCODE_HOME){pos=ctrl?0:shipTextLineStart(&speFile.text,pos);move=1;}if(ID==SDL_SCANCODE_END){pos=ctrl?len:shipTextLineEnd(&speFile.text,pos);move=1;}
  if(ID==SDL_SCANCODE_UP||ID==SDL_SCANCODE_DOWN){size_t start=shipTextLineStart(&speFile.text,pos),column=pos-start,end;if(ID==SDL_SCANCODE_UP)start=start?shipTextLineStart(&speFile.text,start-1):0;else {start=shipTextLineEnd(&speFile.text,start);if(start<len)start++;}end=shipTextLineEnd(&speFile.text,start);pos=start+column<end?start+column:end;move=1;}
  if(move){speFile.text.cursor=pos;if(!shift)speFile.text.anchor=pos;speFileReveal();return 1;}
  if(ID==BACKSPACEKEY||ID==SDL_SCANCODE_DELETE){if(speFile.text.anchor==pos){if(ID==BACKSPACEKEY&&pos)speFile.text.anchor=pos-1;else if(ID==SDL_SCANCODE_DELETE&&pos<len)speFile.text.anchor=pos+1;}if(speFile.text.anchor!=pos)speFileInsert("");return 1;}
  if(ID==RETURNKEY){speFileInsert("\n");return 1;}if(ID==SDL_SCANCODE_TAB){speFileInsert("\t");return 1;}
  {SDL_Keycode key=SDL_GetKeyFromScancode((SDL_Scancode)ID);char s[2]={0};if(key>=32&&key<=126){s[0]=(char)key;if(shift){const char*plain="1234567890-=[]\\;',./`",*upper="!@#$%^&*()_+{}|:\"<>?~",*found=strchr(plain,s[0]);if(found)s[0]=upper[found-plain];else if(s[0]>='a'&&s[0]<='z')s[0]-=32;}speFileInsert(s);}}return 1;
 }
 if(event==RPE_ReleaseLeft&&speFile.drag){speFile.drag=0;return 1;}
 if(speFile.drag&&event==RPE_HoldLeft){int dx=x-speFile.lastX,dy=y-speFile.lastY;if(speFile.drag==3){if(y<body.y0&&speFile.scroll)speFile.scroll--;if(y>body.y1&&speFile.text.text[speFileLine(speFile.scroll+1)])speFile.scroll++;speFile.text.cursor=speFileHit(x,y);}else if(speFile.drag==1){int w=speFile.box.x1-speFile.box.x0,h=speFile.box.y1-speFile.box.y0;speFile.box.x0=muiClamp(speFile.box.x0+dx,0,MAIN_WindowWidth-w);speFile.box.y0=muiClamp(speFile.box.y0+dy,0,MAIN_WindowHeight-h);speFile.box.x1=speFile.box.x0+w;speFile.box.y1=speFile.box.y0+h;}else {speFile.box.x1=muiClamp(speFile.box.x1+dx,speFile.box.x0+uiScaleSize(400),MAIN_WindowWidth);speFile.box.y1=muiClamp(speFile.box.y1+dy,speFile.box.y0+uiScaleSize(220),MAIN_WindowHeight);}speFile.lastX=x;speFile.lastY=y;return 1;}
 if(event==RPE_PressLeft||event==RPE_PressRight||event==RPE_PressCentre)speFile.focus=inside;if(!inside)return 0;
 if(event==RPE_WheelUp||event==RPE_WheelDown){if(event==RPE_WheelUp)speFile.scroll=muiClamp(speFile.scroll-3,0,SHIP_TEXT_LIMIT);else if(speFile.text.text[speFileLine(speFile.scroll+1)])speFile.scroll+=3;return 1;}
 if(event==RPE_PressLeft){speFile.lastX=x;speFile.lastY=y;if(x>speFile.box.x1-uiScaleSize(24)&&y>speFile.box.y1-uiScaleSize(24))speFile.drag=2;else if(y<speFile.box.y0+uiScaleSize(30))speFile.drag=1;else {for(i=0;i<5;i++){rectangle r=speFileButton(i);if(muiPointIn(&r,x,y)){speFileAction(i);return 1;}}if(muiPointIn(&body,x,y)){speFile.text.cursor=speFileHit(x,y);if(!keyIsHit(SHIFTKEY))speFile.text.anchor=speFile.text.cursor;speFile.drag=3;}}}return 1;
}
