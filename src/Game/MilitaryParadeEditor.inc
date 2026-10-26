/* Included by ModernUI.c: shares native UI widgets and the existing modal menu. */
#include "MilitaryParadeLayout.h"
#include "Mesh.h"
#include "Camera.h"
#include "glinc.h"
static void speBegin(void);
static void speClose(void);
static void speDraw(void);
static udword speProcess(smemsize ID,udword event);
typedef struct MpeIdentity { ShipRace race;ShipType type; } MpeIdentity;
#define MPE_MAX_IDENTITIES (NUM_RACES*TOTAL_NUM_SHIPS)
#define MPE_UNDO 32
static real32 muiClampF(real32 v,real32 low,real32 high);
static struct {
    int stage,host,selected,count,listScroll,dropScroll,dropdown,mode,dirty;
    int drag,axis,lastX,lastY,edit,historyCount,historyAt,rotateMode,discardArmed;
    float yaw,pitch,zoom,panX,panY;
    float zoomTarget;
    Uint32 cameraTick;
    int cameraCaptured,cursorX,cursorY;
    char input[48],preset[33],status[150];
    ParadeLayoutDocument document,history[MPE_UNDO];
    MpeIdentity identities[MPE_MAX_IDENTITIES];
    meshdata *meshes[NUM_RACES][TOTAL_NUM_SHIPS];
    bool32 attempted[NUM_RACES][TOTAL_NUM_SHIPS];
} mpe;
static rectangle mpeView,mpeList,mpeInspector,mpeDrop,mpeFields[9],mpeButtons[9],mpePreset;
static const char *mpeFieldNames[9]={"POSITION X","POSITION Y","POSITION Z","ROTATION X","ROTATION Y","ROTATION Z","GROWTH X","GROWTH Y","GROWTH Z"};
static color mpeAxisColor[3];
static void mpeStatus(const char *s){snprintf(mpe.status,sizeof(mpe.status),"%s",s);}
static ParadeShipLayout *mpeSelected(void)
{
    MpeIdentity *id;
    if(mpe.selected<0 || mpe.selected>=mpe.count)return NULL;
    id=&mpe.identities[mpe.selected];return &mpe.document.ships[id->race][id->type];
}
static float *mpeValue(ParadeShipLayout *e,int field)
{return field<3?&e->position[field]:field<6?&e->rotation[field-3]:&e->growth[field-6];}
static void mpeCheckpoint(void)
{
    if(mpe.historyAt+1<MPE_UNDO)mpe.historyAt++;
    else {memmove(mpe.history,mpe.history+1,(MPE_UNDO-1)*sizeof(mpe.history[0]));}
    mpe.historyCount=mpe.historyAt+1;mpe.history[mpe.historyAt]=mpe.document;mpe.dirty=TRUE;mpe.discardArmed=FALSE;
}
static void mpeUndo(int direction)
{
    int next=mpe.historyAt+direction;
    if(next>=0 && next<mpe.historyCount){mpe.historyAt=next;mpe.document=mpe.history[next];mpe.dirty=TRUE;}
}
static void mpeFreeMeshes(void)
{
    int r,t;
    for(r=0;r<NUM_RACES;r++)for(t=0;t<TOTAL_NUM_SHIPS;t++) {
        if(mpe.meshes[r][t])meshFree(mpe.meshes[r][t]);mpe.meshes[r][t]=NULL;mpe.attempted[r][t]=FALSE;
    }
}
static void mpeReleaseCamera(void)
{
    if(mpe.cameraCaptured){mouseClipToRect(NULL);mousePositionSet(mpe.cursorX,mpe.cursorY);mouseCursorShow();mpe.cameraCaptured=FALSE;}
    if(mpe.drag==3)mpe.drag=0;
}
static void mpeClose(void)
{speClose();mpeReleaseCamera();mpeFreeMeshes();mpe.stage=0;mpe.drag=0;mpe.edit=-1;}
static void mpeBegin(int host)
{
    int r,t;
    mpeReleaseCamera();mpeFreeMeshes();memset(&mpe,0,sizeof(mpe));mpe.stage=3;mpe.host=host;mpe.selected=-1;mpe.edit=-1;
    mpe.zoom=.055f;mpe.pitch=-50;snprintf(mpe.preset,sizeof(mpe.preset),"default");
    mpe.zoomTarget=1/mpe.zoom;mpe.cameraTick=SDL_GetTicks();
    paradeSetTweakables();paradeLayoutGet(host,&mpe.document);
    for(r=0;r<NUM_RACES;r++)for(t=FirstShipTypeOfRace[r];t<=LastShipTypeOfRace[r];t++)
        if(paradeLayoutIdentityValid(r,t) && !(r==host && t==Mothership)) {
            mpe.identities[mpe.count].race=r;mpe.identities[mpe.count++].type=t;
        }
    mpe.history[0]=mpe.document;mpe.historyCount=1;
    mpeStatus("PREVIEW ONLY / SAVE CAMPAIGN TO USE IN MISSIONS / ESC BACK");
}
static void mpeLayout(void)
{
    int margin=uiScaleSize(16),left=MAIN_WindowWidth*22/100,right=MAIN_WindowWidth*23/100;
    int top=uiScaleSize(108),bottom=MAIN_WindowHeight-uiScaleSize(55),i,bw=(MAIN_WindowWidth-2*margin)/9;
    mpeList=muiRect(margin,top,left-margin,bottom-uiScaleSize(50));
    mpeView=muiRect(left,top,MAIN_WindowWidth-right,bottom);
    mpeInspector=muiRect(mpeView.x1+margin,top,MAIN_WindowWidth-margin,bottom);
    mpeDrop=muiRect(mpeList.x0,mpeList.y1+uiScaleSize(8),mpeList.x1,bottom);
    for(i=0;i<9;i++)mpeFields[i]=muiRect(mpeInspector.x0+uiScaleSize(8),top+uiScaleSize(46+i*35),mpeInspector.x1-uiScaleSize(8),top+uiScaleSize(76+i*35));
    for(i=0;i<9;i++)mpeButtons[i]=muiRect(margin+i*bw,uiScaleSize(57),margin+(i+1)*bw-uiScaleSize(5),uiScaleSize(94));
    mpePreset=muiRect(mpeInspector.x0+uiScaleSize(8),mpeInspector.y1-uiScaleSize(44),mpeInspector.x1-uiScaleSize(8),mpeInspector.y1-uiScaleSize(10));
    mpeAxisColor[0]=colRGB(240,100,85);mpeAxisColor[1]=colRGB(95,210,135);mpeAxisColor[2]=colRGB(100,165,255);
}
static void mpeViewRotate(const float p[3],float v[3])
{
    float azimuth[3]={0,0,mpe.yaw},elevation[3]={mpe.pitch,0,0},horizontal[3];
    /* Orbit around world up, not screen up (which would roll the preview). */
    paradeLayoutRotate(azimuth,p,horizontal);paradeLayoutRotate(elevation,horizontal,v);
}
static void mpeProject(const float p[3],float *x,float *y)
{
    float v[3];mpeViewRotate(p,v);
    *x=(mpeView.x0+mpeView.x1)*.5f+mpe.panX+v[0]*mpe.zoom;
    *y=(mpeView.y0+mpeView.y1)*.5f+mpe.panY-v[1]*mpe.zoom;
}
static meshdata *mpeMesh(ShipRace race,ShipType type)
{
    char path[256],manifest[256];meshdata *mesh;
    if(mpe.attempted[race][type])return mpe.meshes[race][type];
    mpe.attempted[race][type]=TRUE;
    snprintf(path,sizeof(path),"%s\\%s\\Rl0\\LOD0\\%s.geo",ShipRaceToStr(race),ShipTypeToStr(type),ShipTypeToStr(type));
    snprintf(manifest,sizeof(manifest),"%s\\%s\\RMNative\\visual.rmnative",ShipRaceToStr(race),ShipTypeToStr(type));
    if(!fileExists(path,0) && !fileExists(manifest,FF_IgnoreBIG))return NULL;
    mesh=meshLoad(path);mpe.meshes[race][type]=mesh;return mesh;
}
static void mpeDrawMesh(meshdata *mesh,const ParadeShipLayout *e)
{
    /* Engine model axes are up/right/forward; editor axes are right/forward/up. */
    static const GLfloat basis[16]={0,0,1,0, 1,0,0,0, 0,1,0,0, 0,0,0,1};
    if(!mesh)return;
    glPushMatrix();glTranslatef(e->position[0],e->position[1],e->position[2]);
    glRotatef(e->rotation[2],0,0,1);glRotatef(e->rotation[1],0,1,0);glRotatef(e->rotation[0],1,0,0);
    glMultMatrixf(basis);meshRender(mesh,0);glPopMatrix();
}
static void mpeRenderModels(void)
{
    GLint viewport[4],mode;float fx,fy;int i;ParadeShipLayout origin={0};
    hmatrix savedCamera=rndCameraMatrix,savedProjection=rndProjectionMatrix;
    static const GLfloat light[4]={-1,-2,3,0},diffuse[4]={.9f,.9f,.9f,1},ambient[4]={.25f,.25f,.25f,1};
    glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_MATRIX_MODE,&mode);
    fx=(float)viewport[2]/MAIN_WindowWidth;fy=(float)viewport[3]/MAIN_WindowHeight;
    primModeClear2();glPushAttrib(GL_ALL_ATTRIB_BITS);
    glViewport((int)(mpeView.x0*fx),(int)(viewport[3]-mpeView.y1*fy),(int)((mpeView.x1-mpeView.x0)*fx),(int)((mpeView.y1-mpeView.y0)*fy));
    glEnable(GL_SCISSOR_TEST);glScissor((int)(mpeView.x0*fx),(int)(viewport[3]-mpeView.y1*fy),(int)((mpeView.x1-mpeView.x0)*fx),(int)((mpeView.y1-mpeView.y0)*fy));
    glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_FOG);glDisable(GL_CLIP_PLANE0);glDisable(GL_BLEND);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(-(mpeView.x1-mpeView.x0)*.5f/mpe.zoom,(mpeView.x1-mpeView.x0)*.5f/mpe.zoom,
        -(mpeView.y1-mpeView.y0)*.5f/mpe.zoom,(mpeView.y1-mpeView.y0)*.5f/mpe.zoom,-200000,200000);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glTranslatef(mpe.panX/mpe.zoom,-mpe.panY/mpe.zoom,0);
    glLightfv(GL_LIGHT0,GL_POSITION,light);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0,GL_AMBIENT,ambient);
    glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);rndLightingEnable(TRUE);rndTextureEnable(TRUE);
    glRotatef(mpe.pitch,1,0,0);glRotatef(mpe.yaw,0,0,1);
    glGetFloatv(GL_MODELVIEW_MATRIX,(GLfloat *)&rndCameraMatrix);
    glGetFloatv(GL_PROJECTION_MATRIX,(GLfloat *)&rndProjectionMatrix);
    mpeDrawMesh(mpeMesh(mpe.host,Mothership),&origin);
    for(i=0;i<mpe.count;i++) {
        MpeIdentity *id=&mpe.identities[i];ParadeShipLayout *e=&mpe.document.ships[id->race][id->type];
        if(e->enabled)mpeDrawMesh(mpeMesh(id->race,id->type),e);
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);
    glPopAttrib();glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    rndCameraMatrix=savedCamera;rndProjectionMatrix=savedProjection;
    rndLightingEnable(FALSE);rndTextureEnable(FALSE);primModeSet2();
}
static void mpeRingPoint(ParadeShipLayout *e,int axis,float angle,float *x,float *y)
{
    float p[3];int a=(axis+1)%3,b=(axis+2)%3;memcpy(p,e->position,sizeof(p));
    p[a]+=cosf(angle)*uiScaleSize(78)/mpe.zoom;p[b]+=sinf(angle)*uiScaleSize(78)/mpe.zoom;mpeProject(p,x,y);
}
static void mpeDrawGizmo(void)
{
    ParadeShipLayout *e=mpeSelected();int axis,k;float x,y,tx,ty,lastX,lastY;
    if(!e || !e->enabled)return;mpeProject(e->position,&x,&y);
    if(!muiPointIn(&mpeView,(int)x,(int)y))return;
    for(axis=0;axis<3;axis++) {
        float p[3];char label[8];memcpy(p,e->position,sizeof(p));p[axis]+=uiScaleSize(95)/mpe.zoom;
        mpeProject(p,&tx,&ty);snprintf(label,sizeof(label),"%c",'X'+axis);
        if(!mpe.rotateMode) {
            primLineThick2((int)x,(int)y,(int)tx,(int)ty,uiScaleSize(2),mpeAxisColor[axis]);
            primCircleSolid2((int)tx,(int)ty,uiScaleSize(5),12,mpeAxisColor[axis]);muiText((int)tx+6,(int)ty,mpeAxisColor[axis],label,muiBodyFont);
        } else {
            mpeRingPoint(e,axis,0,&lastX,&lastY);
            for(k=1;k<=64;k++){mpeRingPoint(e,axis,k*.09817477f,&tx,&ty);primLine2((int)lastX,(int)lastY,(int)tx,(int)ty,mpeAxisColor[axis]);lastX=tx;lastY=ty;}
        }
    }
}
#include "ModManager.inc"
static void mpeDrawMenu(void)
{
    rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight),button;int i;
    muiDrawBackdrop(MUI_ART_GALAXY);muiFill(&whole,colRGBA(5,12,18,240));
    muiText(uiScaleSize(40),uiScaleSize(35),muiSignal,"MODDING",muiHeadingFont);
    if(mpe.stage==1) {
        button=muiRect(uiScaleSize(40),uiScaleSize(120),MAIN_WindowWidth-uiScaleSize(40),uiScaleSize(180));
        muiDrawButtonRect(&button,"MILITARY PARADE FORMATION EDITOR",TRUE,muiPointIn(&button,mouseCursorX(),mouseCursorY()));
        button=muiRect(uiScaleSize(40),uiScaleSize(198),MAIN_WindowWidth-uiScaleSize(40),uiScaleSize(258));
        muiDrawButtonRect(&button,"SHIP VIEWER / EDITOR / NEW SHIP PROJECT",TRUE,muiPointIn(&button,mouseCursorX(),mouseCursorY()));
        button=muiRect(uiScaleSize(40),uiScaleSize(276),MAIN_WindowWidth-uiScaleSize(40),uiScaleSize(336));
        muiDrawButtonRect(&button,"MOD MANAGER",TRUE,muiPointIn(&button,mouseCursorX(),mouseCursorY()));
    } else {
        muiText(uiScaleSize(40),uiScaleSize(85),muiPaper,"MILITARY PARADE FORMATION EDITOR / HOST",muiBodyFont);
        for(i=0;i<2;i++){button=muiRect(uiScaleSize(40),uiScaleSize(130+i*70),MAIN_WindowWidth-uiScaleSize(40),uiScaleSize(188+i*70));muiDrawButtonRect(&button,i==0?"KUSHAN":"TAIIDAN",TRUE,muiPointIn(&button,mouseCursorX(),mouseCursorY()));}
    }
    muiText(uiScaleSize(40),MAIN_WindowHeight-uiScaleSize(50),muiMuted,"ESC / BACK",muiBodyFont);
}
static void mpeDraw(void)
{
    static const char *buttons[9]={"BACK","SAVE","LOAD","UNDO","REDO","MOVE / ROTATE","FRAME ALL","REMOVE TYPE","CAMPAIGN / MP"};
    rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight),row;int i,n=0,visible;char text[150];ParadeShipLayout *e;
    if(mpe.stage==4){speDraw();return;}
    if(mpe.stage==5){modUIDraw();return;}
    if(mpe.stage!=3){mpeDrawMenu();return;}
    {
        Uint32 now=SDL_GetTicks();float dt=(now-mpe.cameraTick)*.001f,distance=1/mpe.zoom;
        mpe.cameraTick=now;if(dt>.25f)dt=.25f;
        distance+=(mpe.zoomTarget-distance)*(1-expf(-fmaxf(1,CAMERA_WHEEL_ZOOM_RESPONSE)*dt));
        mpe.zoom=1/muiClampF(distance,.25f,1000);
        if(mpe.cameraCaptured && !mouseRightButton())mpeReleaseCamera();
    }
    mpeLayout();muiFill(&whole,colRGB(8,14,20));
    snprintf(text,sizeof(text),"MILITARY PARADE / %s / %s%s",mpe.host==0?"KUSHAN":"TAIIDAN",mpe.mode?"MULTIPLAYER PRESET":"CAMPAIGN",mpe.dirty?" / UNSAVED":"");
    muiText(uiScaleSize(20),uiScaleSize(22),muiSignal,text,muiHeadingFont);
    for(i=0;i<9;i++)muiDrawButtonRect(&mpeButtons[i],buttons[i],i==1,muiPointIn(&mpeButtons[i],mouseCursorX(),mouseCursorY()));
    muiFill(&mpeList,muiSurface);muiFill(&mpeInspector,muiSurface);muiFill(&mpeView,colRGB(7,15,23));
    mpeRenderModels();
    muiText(mpeList.x0+8,mpeList.y0+8,muiSignal,"PLACED SHIP TYPES",muiBodyFont);
    visible=(mpeList.y1-mpeList.y0-uiScaleSize(38))/uiScaleSize(31);
    for(i=0;i<mpe.count;i++) {
        MpeIdentity *id=&mpe.identities[i];if(!mpe.document.ships[id->race][id->type].enabled)continue;
        if(n++<mpe.listScroll)continue;if(n-mpe.listScroll>visible)break;
        row=muiRect(mpeList.x0+3,mpeList.y0+uiScaleSize(30+(n-mpe.listScroll-1)*31),mpeList.x1-3,mpeList.y0+uiScaleSize(59+(n-mpe.listScroll-1)*31));
        snprintf(text,sizeof(text),"%s / %s",ShipRaceToStr(id->race),ShipTypeToNiceStr(id->type));
        muiDrawButtonRect(&row,text,i==mpe.selected,muiPointIn(&row,mouseCursorX(),mouseCursorY()));
    }
    muiDrawButtonRect(&mpeDrop,"ADD SHIP TYPE  v",TRUE,muiPointIn(&mpeDrop,mouseCursorX(),mouseCursorY()));
    e=mpeSelected();muiText(mpeInspector.x0+8,mpeInspector.y0+8,muiSignal,"TRANSFORM / LOCAL AXES",muiBodyFont);
    if(e && e->enabled) for(i=0;i<9;i++) {
        muiFill(&mpeFields[i],i==mpe.edit?muiSignalSoft:muiSurfaceRaised);
        snprintf(text,sizeof(text),"%s  %s",mpeFieldNames[i],i==mpe.edit?mpe.input:"");
        if(i!=mpe.edit)snprintf(text,sizeof(text),"%s  %.2f",mpeFieldNames[i],*mpeValue(e,i));
        muiText(mpeFields[i].x0+5,mpeFields[i].y0+uiScaleSize(7),muiPaper,text,muiBodyFont);
    }
    muiText(mpeInspector.x0+8,mpeFields[8].y1+uiScaleSize(12),muiMuted,"X RIGHT / Y FORWARD / Z UP",muiBodyFont);
    muiText(mpeInspector.x0+8,mpeFields[8].y1+uiScaleSize(36),muiMuted,"GROWTH = OFFSET PER COPY",muiBodyFont);
    if(mpe.mode) {
        snprintf(text,sizeof(text),"PRESET: %s",mpe.edit==9?mpe.input:mpe.preset);
        muiDrawButtonRect(&mpePreset,text,mpe.edit==9,muiPointIn(&mpePreset,mouseCursorX(),mouseCursorY()));
    }
    for(i=0;i<mpe.count;i++) {
        MpeIdentity *id=&mpe.identities[i];float x,y;if(!mpe.document.ships[id->race][id->type].enabled)continue;
        mpeProject(mpe.document.ships[id->race][id->type].position,&x,&y);
        if(muiPointIn(&mpeView,(int)x,(int)y))muiText((int)x+8,(int)y+8,i==mpe.selected?muiSignal:muiMuted,ShipTypeToNiceStr(id->type),muiBodyFont);
    }
    mpeDrawGizmo();
    muiText(mpeView.x0+8,mpeView.y0+8,muiMuted,"RIGHT ORBIT / LEFT+RIGHT ZOOM / WHEEL ZOOM / MIDDLE PAN",muiBodyFont);
    muiText(uiScaleSize(18),MAIN_WindowHeight-uiScaleSize(34),muiPaper,mpe.status,muiBodyFont);
    if(mpe.dropdown) {
        int count=0;int height=uiScaleSize(29),top=mpeDrop.y0-uiScaleSize(12*29);
        for(i=0;i<mpe.count;i++) {
            MpeIdentity *id=&mpe.identities[i];if(mpe.document.ships[id->race][id->type].enabled)continue;
            if(count++<mpe.dropScroll)continue;if(count-mpe.dropScroll>12)break;
            row=muiRect(mpeDrop.x0,top+(count-mpe.dropScroll-1)*height,mpeInspector.x1,top+(count-mpe.dropScroll)*height);
            snprintf(text,sizeof(text),"%s / %s",ShipRaceToStr(id->race),ShipTypeToNiceStr(id->type));
            muiDrawButtonRect(&row,text,FALSE,muiPointIn(&row,mouseCursorX(),mouseCursorY()));
        }
    }
}
static void mpeCommitField(void)
{
    ParadeShipLayout *e=mpeSelected();char *end;float value;
    if(mpe.edit==9) {if(mpe.input[0])snprintf(mpe.preset,sizeof(mpe.preset),"%.32s",mpe.input);}
    else if(mpe.edit>=0 && e){ParadeShipLayout candidate=*e;value=strtof(mpe.input,&end);
        if(end!=mpe.input && !*end){*mpeValue(&candidate,mpe.edit)=value;if(paradeLayoutValid(&candidate)){*e=candidate;mpeCheckpoint();}else mpeStatus("INVALID VALUE / POSITION-GROWTH +/-100000 / ROTATION +/-360");}}
    mpe.edit=-1;
}
static void mpeFrame(void)
{
    float extent=2500;int i,k;float x,y;
    for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];ParadeShipLayout *e=&mpe.document.ships[id->race][id->type];if(!e->enabled)continue;
        for(k=0;k<3;k++)if(fabsf(e->position[k])+1500>extent)extent=fabsf(e->position[k])+1500;}
    x=(mpeView.x1-mpeView.x0)*.42f/extent;y=(mpeView.y1-mpeView.y0)*.42f/extent;mpe.zoom=muiClampF(x<y?x:y,.001f,4);mpe.zoomTarget=1/mpe.zoom;mpe.panX=mpe.panY=0;
}
static void mpeAction(int index)
{
    ParadeShipLayout *e;int success;
    mpeCommitField();
    switch(index) {
        case 0:if(mpe.dirty){mpeStatus("UNSAVED: SAVE FIRST, OR PRESS ESC TWICE TO DISCARD");return;}mpeReleaseCamera();mpeFreeMeshes();mpe.stage=2;break;
        case 1:success=mpe.mode?paradeLayoutSaveMultiplayer(mpe.host,mpe.preset,&mpe.document):paradeLayoutSave(mpe.host,&mpe.document);
            if(success)mpe.dirty=FALSE;mpeStatus(success?(mpe.mode?"MULTIPLAYER PRESET SAVED / NOT ACTIVE IN NETWORK GAMES":"CAMPAIGN SAVED / USED ON LAUNCH AND PARADE / BACKUP KEPT"):"SAVE FAILED / PRESET NAME: LETTERS, NUMBERS, - OR _");break;
        case 2:success=mpe.mode?paradeLayoutLoadMultiplayer(mpe.host,mpe.preset,&mpe.document):paradeLayoutReload(mpe.host);
            if(success){if(!mpe.mode)paradeLayoutGet(mpe.host,&mpe.document);mpe.historyAt=0;mpe.historyCount=1;mpe.history[0]=mpe.document;mpe.dirty=FALSE;}mpeStatus(success?"LAYOUT LOADED":"LOAD FAILED / NO CHANGES MADE");break;
        case 3:mpeUndo(-1);break;case 4:mpeUndo(1);break;
        case 5:mpe.rotateMode=!mpe.rotateMode;break;
        case 6:mpeFrame();break;
        case 7:e=mpeSelected();if(e){memset(e,0,sizeof(*e));mpeCheckpoint();mpe.selected=-1;}break;
        case 8:mpe.mode=!mpe.mode;mpeStatus(mpe.mode?"MP PRESET FILES ONLY / STORED SEPARATELY FOR FUTURE USE":"CAMPAIGN AUTHORING / SAVE APPLIES TO THIS HOST RACE");break;
    }
}
static udword mpeProcess(smemsize ID,udword event)
{
    int x=mouseCursorX(),y=mouseCursorY(),i,n=0,dx=x-mpe.lastX,dy=y-mpe.lastY;ParadeShipLayout *e=mpeSelected();
    if(mpe.stage==4)return speProcess(ID,event);
    if(mpe.stage==5)return modUIProcess(ID,event);
    mpeLayout();
    if(event==RPE_KeyDown) {
        if(mpe.edit>=0) {
            size_t len=strlen(mpe.input);char ch=0;
            if(ID==RETURNKEY){mpeCommitField();return RPR_Redraw;}
            if(ID==ESCKEY){mpe.edit=-1;return RPR_Redraw;}
            if(ID==BACKSPACEKEY){if(len)mpe.input[len-1]=0;return RPR_Redraw;}
            if(ID>=SDL_SCANCODE_1 && ID<=SDL_SCANCODE_9)ch='1'+(char)(ID-SDL_SCANCODE_1);
            if(ID==ZEROKEY)ch='0';if(ID==MINUSKEY)ch='-';if(ID==GREATERTHAN && mpe.edit!=9)ch='.';
            if(mpe.edit==9 && ID>=AKEY && ID<=ZKEY)ch='a'+(char)(ID-AKEY);
            if(ch && len<(mpe.edit==9?32:45)){mpe.input[len]=ch;mpe.input[len+1]=0;}return RPR_Redraw;
        }
        if(ID==ESCKEY){mpeReleaseCamera();if(mpe.stage==3 && mpe.dirty && !mpe.discardArmed){mpe.discardArmed=TRUE;mpeStatus("PRESS ESC AGAIN TO DISCARD UNSAVED EDITS / OR SAVE");return RPR_Redraw;}if(mpe.stage>1){mpeFreeMeshes();mpe.stage--;}else mpeClose();return RPR_Redraw;}
        if(mpe.stage==1 && ID==RETURNKEY){mpe.stage=2;return RPR_Redraw;}
        if(mpe.stage==2){if(ID==ARRUP || ID==ARRDOWN)mpe.host=!mpe.host;if(ID==RETURNKEY)mpeBegin(mpe.host);return RPR_Redraw;}
        if(mpe.stage==3){if(keyIsHit(CONTROLKEY)){if(ID==SKEY)mpeAction(1);if(ID==ZKEY)mpeUndo(-1);if(ID==YKEY)mpeUndo(1);}
            else if(ID==GKEY)mpe.rotateMode=0;else if(ID==TKEY)mpe.rotateMode=1;else if(ID==FKEY)mpeFrame();}
        return RPR_Redraw;
    }
    if(mpe.stage<3) {
        if(event==RPE_ReleaseLeft&&mpe.stage==1&&x>=uiScaleSize(40)&&x<MAIN_WindowWidth-uiScaleSize(40)&&y>=uiScaleSize(276)&&y<uiScaleSize(336)){modUIOpen(0);return RPR_Redraw;}
        if(event==RPE_ReleaseLeft){if(mpe.stage==1 && y>=uiScaleSize(198)&&y<uiScaleSize(258)){speBegin();return RPR_Redraw;}
            if(mpe.stage==1 && y>=uiScaleSize(120)&&y<uiScaleSize(180))mpe.stage=2;
            else if(mpe.stage==2)for(i=0;i<2;i++)if(y>=uiScaleSize(130+i*70)&&y<uiScaleSize(188+i*70))mpeBegin(i);}
        return RPR_Redraw;
    }
    if(event==RPE_WheelUp || event==RPE_WheelDown) {
        int d=event==RPE_WheelUp?-1:1;
        if(mpe.dropdown){int available=0;for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];if(!mpe.document.ships[id->race][id->type].enabled)available++;}mpe.dropScroll=muiClamp(mpe.dropScroll+d,0,available>12?available-12:0);}
        else if(muiPointIn(&mpeList,x,y)){int placed=0;for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];if(mpe.document.ships[id->race][id->type].enabled)placed++;}mpe.listScroll=muiClamp(mpe.listScroll+d,0,placed>1?placed-1:0);}
        else if(mpe.cameraCaptured || muiPointIn(&mpeView,x,y))mpe.zoomTarget=muiClampF(mpe.zoomTarget*cameraWheelZoomFactor(d<0),.25f,1000);
        return RPR_Redraw;
    }
    if(event==RPE_PressRight || event==RPE_PressCentre){if(muiPointIn(&mpeView,x,y)){
        mpeCommitField();if(mpe.drag==4 || mpe.drag==5)mpeCheckpoint();
        if(event==RPE_PressRight){mpe.drag=3;mpe.cameraCaptured=TRUE;mpe.cursorX=x;mpe.cursorY=y;
            mouseCursorHide();mouseClipToRect(&mpeView);x=(mpeView.x0+mpeView.x1)/2;y=(mpeView.y0+mpeView.y1)/2;mousePositionSet(x,y);
        }else if(!mpe.cameraCaptured)mpe.drag=2;
        mpe.lastX=x;mpe.lastY=y;return RPR_Redraw;
    }}
    /* Left+right is camera zoom, never a transform/selection click. */
    if(mpe.cameraCaptured && (event==RPE_PressLeft || event==RPE_ReleaseLeft))return RPR_Redraw;
    if(event==RPE_PressLeft) {
        if(mpe.dropdown) {
            int top=mpeDrop.y0-uiScaleSize(12*29);
            if(x>=mpeDrop.x0 && x<mpeInspector.x1 && y>=top && y<mpeDrop.y0){int wanted=(y-top)/uiScaleSize(29)+mpe.dropScroll;
                for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];if(mpe.document.ships[id->race][id->type].enabled)continue;
                    if(n++==wanted){ParadeShipLayout entry;paradeLayoutDefault(mpe.host,id->race,id->type,&entry);entry.position[0]=entry.position[1]=entry.position[2]=0;
                        mpe.document.ships[id->race][id->type]=entry;mpe.selected=i;mpeCheckpoint();mpeStatus("SHIP TYPE ADDED AT ORIGIN / MOVE IT USING AXIS HANDLES");break;}}
                mpe.dropdown=0;return RPR_Redraw;}
            mpe.dropdown=0;
        }
        mpeCommitField();
        for(i=0;i<9;i++)if(muiPointIn(&mpeButtons[i],x,y)){mpeAction(i);return RPR_Redraw;}
        if(muiPointIn(&mpeDrop,x,y)){mpe.dropdown=!mpe.dropdown;mpe.dropScroll=0;return RPR_Redraw;}
        if(mpe.mode && muiPointIn(&mpePreset,x,y)){mpe.edit=9;snprintf(mpe.input,sizeof(mpe.input),"%s",mpe.preset);return RPR_Redraw;}
        if(e && e->enabled)for(i=0;i<9;i++)if(muiPointIn(&mpeFields[i],x,y)){mpe.edit=i;snprintf(mpe.input,sizeof(mpe.input),"%.2f",*mpeValue(e,i));return RPR_Redraw;}
        if(muiPointIn(&mpeList,x,y)){int wanted=(y-mpeList.y0-uiScaleSize(30))/uiScaleSize(31)+mpe.listScroll;
            for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];if(mpe.document.ships[id->race][id->type].enabled && n++==wanted){mpe.selected=i;break;}}return RPR_Redraw;}
        if(muiPointIn(&mpeView,x,y)) {
            if(e && e->enabled){float best=uiScaleSize(12)*uiScaleSize(12);int chosen=-1;
                for(i=0;i<3;i++){int k;float px,py;if(!mpe.rotateMode){float p[3];memcpy(p,e->position,sizeof(p));p[i]+=uiScaleSize(95)/mpe.zoom;mpeProject(p,&px,&py);
                    if((px-x)*(px-x)+(py-y)*(py-y)<best){best=(px-x)*(px-x)+(py-y)*(py-y);chosen=i;}}
                    else for(k=0;k<64;k++){mpeRingPoint(e,i,k*.09817477f,&px,&py);if((px-x)*(px-x)+(py-y)*(py-y)<best){best=(px-x)*(px-x)+(py-y)*(py-y);chosen=i;}}}
                if(chosen>=0){mpe.axis=chosen;mpe.drag=mpe.rotateMode?5:4;}}
            if(!mpe.drag){float best=uiScaleSize(50)*uiScaleSize(50);for(i=0;i<mpe.count;i++){MpeIdentity *id=&mpe.identities[i];float px,py;
                if(!mpe.document.ships[id->race][id->type].enabled)continue;mpeProject(mpe.document.ships[id->race][id->type].position,&px,&py);
                if((px-x)*(px-x)+(py-y)*(py-y)<best){best=(px-x)*(px-x)+(py-y)*(py-y);mpe.selected=i;}}}
        }
    }
    if(mpe.drag && (event==RPE_HoldLeft || event==RPE_HoldRight || event==RPE_HoldCentre)) {
        if(mpe.drag==2){mpe.panX+=dx;mpe.panY+=dy;}
        if(mpe.drag==3){Camera camera={0};int cx=(mpeView.x0+mpeView.x1)/2,cy=(mpeView.y0+mpeView.y1)/2;
            /* Convert the editor's view rotation to the game's spherical camera. */
            camera.angle=(-90-mpe.yaw)*.01745329252f;camera.declination=(-90-mpe.pitch)*.01745329252f;
            camera.distance=1/mpe.zoom;camera.closestZoom=.25f;camera.farthestZoom=1000;
            cameraMouseDrag(&camera,cx-x,cy-y,mouseLeftButton(),FALSE);
            mpe.yaw=-90-camera.angle*57.29577951f;mpe.pitch=-90-camera.declination*57.29577951f;
            if(mouseLeftButton()){mpe.zoom=1/camera.distance;mpe.zoomTarget=camera.distance;}
            mousePositionSet(cx,cy);x=cx;y=cy;
        }
        if(e && mpe.drag==4){float axis[3]={0},v[3],denom;axis[mpe.axis]=1;mpeViewRotate(axis,v);
            denom=v[0]*v[0]+v[1]*v[1];if(denom>.0001f)e->position[mpe.axis]=muiClampF(e->position[mpe.axis]+(dx*v[0]-dy*v[1])/(mpe.zoom*denom),-100000,100000);mpe.dirty=TRUE;}
        if(e && mpe.drag==5){e->rotation[mpe.axis]+=dx-dy;while(e->rotation[mpe.axis]>180)e->rotation[mpe.axis]-=360;while(e->rotation[mpe.axis]<-180)e->rotation[mpe.axis]+=360;mpe.dirty=TRUE;}
    }
    if(event==RPE_ReleaseRight && mpe.cameraCaptured)mpeReleaseCamera();
    else if(event==RPE_ReleaseLeft || event==RPE_ReleaseRight || event==RPE_ReleaseCentre){if(mpe.drag==4 || mpe.drag==5)mpeCheckpoint();if(!mpe.cameraCaptured)mpe.drag=0;}
    mpe.lastX=x;mpe.lastY=y;return RPR_Redraw;
}
