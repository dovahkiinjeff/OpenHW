/* Dedicated lobby uses the modern menu palette, independent of old WON/LAN UI. */
static regionhandle muiOnlineRegion;
static char muiOnlineCode[8];
static sdword muiOnlinePage;
static sdword muiOnlineColorSlot;
static const uint32_t muiOnlinePalette[]={colRGB(48,48,48),colRGB(255,32,32),colRGB(64,192,255),colRGB(255,192,64),colRGB(96,221,96),colRGB(176,96,238),colWhite,colRGB(32,48,160)};
static rectangle muiOnlineBox(real32 x,real32 y,real32 w,real32 h)
{return muiRect((sdword)(MAIN_WindowWidth*x),(sdword)(MAIN_WindowHeight*y),
    (sdword)(MAIN_WindowWidth*(x+w)),(sdword)(MAIN_WindowHeight*(y+h)));}
static void muiOnlineButton(rectangle rect,const char *text,bool32 primary)
{muiDrawButtonRect(&rect,text,primary,muiPointIn(&rect,mouseCursorX(),mouseCursorY()));}
static rectangle muiOnlineSlot(sdword index,sdword column)
{
    static const real32 edges[]={.04f,.24f,.335f,.435f,.51f,.56f};
    return muiOnlineBox(edges[column],.28f+index*.058f,edges[column+1]-edges[column]-.004f,.049f);
}
static rectangle muiOnlineRule(sdword index)
{return muiOnlineBox(.59f,.28f+index*.042f,.37f,.037f);}
static const char *muiOnlineRuleNames[13]={
    "UNLIMITED FUEL","ORDERS WHILE PAUSED","WEAPON RECOIL","RESOURCE MULTIPLIER",
    "RANK SYSTEM","STARTING FLEET LAUNCH","RESOURCE POPULATION","UNLIMITED RUs",
    "FOG OF WAR","LARGER RESOURCE FLEET","SUPER SALVAGERS","CAPTURE / BUILD ALL","DISABLE UNIT CAPS"};
static const uint16_t muiOnlineRuleMin[13]={0,0,0,1,0,0,0,0,0,0,0,0,0};
static const uint16_t muiOnlineRuleMax[13]={1,1,1,4,1,2,100,1,1,1,1,1,1};
static void muiOnlineCommit(OHLobby *lobby)
{
    struct {uint32_t revision;OHConfig config;} packet;
    packet.revision=lobby->revision;packet.config=lobby->config;
    hwOnlineSend(OH_CONFIGURE,&packet,sizeof(packet));
}
static void muiOnlineDraw(regionhandle region)
{
    OHLobby lobby;char text[256];sdword i;
    rectangle whole=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight),panel=muiOnlineBox(.02f,.025f,.96f,.95f);
    (void)region;
    muiEnsureFonts();muiDrawBackdrop(MUI_ART_MOTHERSHIP);
    muiFill(&whole,colRGBA(3,6,9,150));muiFill(&panel,colRGBA(10,18,24,246));primRectOutline2(&panel,1,muiLine);
    muiText(panel.x0+uiScaleSize(20),panel.y0+uiScaleSize(18),muiSignal,"FLEET NETWORK / DEDICATED SERVER",muiBodyFont);
    muiText(panel.x0+uiScaleSize(20),panel.y0+uiScaleSize(47),muiPaper,"ONLINE MULTIPLAYER",muiHeadingFont);
    hwOnlineLobbyStatus(text,sizeof(text));
    muiText(panel.x0+uiScaleSize(20),panel.y1-uiScaleSize(32),muiCool,text,muiBodyFont);
    if(!hwOnlineLobbySnapshot(&lobby)) {
        muiOnlineButton(muiOnlineBox(.10f,.31f,.30f,.065f),"CREATE MATCH",TRUE);
        snprintf(text,sizeof(text),"ROOM CODE: %s_",muiOnlineCode);
        muiOnlineButton(muiOnlineBox(.10f,.40f,.30f,.065f),text,FALSE);
        muiOnlineButton(muiOnlineBox(.10f,.49f,.30f,.065f),"JOIN MATCH",FALSE);
        muiOnlineButton(muiOnlineBox(.10f,.58f,.30f,.065f),"RECONNECT",FALSE);
        muiText((sdword)(MAIN_WindowWidth*.47f),(sdword)(MAIN_WindowHeight*.34f),muiPaper,"THE SERVER RUNS THE BATTLE",muiHeadingFont);
        muiText((sdword)(MAIN_WindowWidth*.47f),(sdword)(MAIN_WindowHeight*.40f),muiMuted,"No direct connections or port forwarding.",muiBodyFont);
        muiText((sdword)(MAIN_WindowWidth*.47f),(sdword)(MAIN_WindowHeight*.45f),muiMuted,"Invite friends with your six-character room code.",muiBodyFont);
        muiText((sdword)(MAIN_WindowWidth*.47f),(sdword)(MAIN_WindowHeight*.50f),muiMuted,"Hosts can add AI, assign teams and set match rules.",muiBodyFont);
    } else {
        snprintf(text,sizeof(text),"ROOM %s / %s",lobby.code,lobby.host?"HOST":"PLAYER");
        muiText((sdword)(MAIN_WindowWidth*.04f),(sdword)(MAIN_WindowHeight*.17f),muiSignal,text,muiHeadingFont);
        muiText((sdword)(MAIN_WindowWidth*.59f),(sdword)(MAIN_WindowHeight*.18f),muiPaper,
            lobby.host?"HOST MATCH SETTINGS":"SERVER MATCH SETTINGS",muiHeadingFont);
        muiOnlineButton(muiOnlineBox(.59f,.22f,.12f,.044f),"GAMEPLAY",muiOnlinePage==0);
        muiOnlineButton(muiOnlineBox(.715f,.22f,.12f,.044f),"MAP / MATCH",muiOnlinePage==1);
        muiOnlineButton(muiOnlineBox(.84f,.22f,.12f,.044f),"ADVANCED",muiOnlinePage==2);
        muiText((sdword)(MAIN_WindowWidth*.04f),(sdword)(MAIN_WindowHeight*.245f),muiMuted,
            "PLAYER                         RACE         AI LEVEL       TEAM       REMOVE",muiBodyFont);
        for(i=0;i<8;i++) {
            OHSlot *slot=&lobby.config.slots[i];char value[96];
            if(!slot->kind){snprintf(value,sizeof(value),"%02d / OPEN SLOT",i+1);muiOnlineButton(muiOnlineSlot(i,0),value,FALSE);continue;}
            snprintf(value,sizeof(value),"%s%s",slot->name,slot->ready?" / READY":"");
            muiOnlineButton(muiOnlineSlot(i,0),value,i==lobby.ownSlot);
            muiOnlineButton(muiOnlineSlot(i,1),slot->race?"TAIIDAN":"KUSHAN",FALSE);
            muiOnlineButton(muiOnlineSlot(i,2),slot->kind==2?(slot->difficulty==0?"EASY":slot->difficulty==1?"NORMAL":"HARD"):"HUMAN",FALSE);
            if(slot->team)snprintf(value,sizeof(value),"%u",slot->team);else strcpy(value,"FFA");
            muiOnlineButton(muiOnlineSlot(i,3),value,FALSE);
            if(slot->kind==2 && lobby.host)muiOnlineButton(muiOnlineSlot(i,4),"X",FALSE);
        }
        if(muiOnlinePage==0) {
            for(i=0;i<13;i++) {
                unsigned value=lobby.config.rules[i];char number[32];rectangle row=muiOnlineRule(i);
                muiFill(&row,muiSurface);primRectOutline2(&row,1,muiLineSoft);
                muiText(row.x0+uiScaleSize(10),row.y0+uiScaleSize(5),muiMuted,muiOnlineRuleNames[i],muiBodyFont);
                if(i==3)snprintf(number,sizeof(number),"%ux",value);
                else if(i==6)snprintf(number,sizeof(number),"%u%%",value);
                else if(i==5)snprintf(number,sizeof(number),"%s",value==0?"LAUNCHED":value==1?"HOLD COMBAT":"HOLD ALL");
                else snprintf(number,sizeof(number),"%s",value?"ON":"OFF");
                muiTextRight(row.x1-uiScaleSize(10),row.y0+uiScaleSize(5),muiPaper,number,muiBodyFont);
            }
        } else if(muiOnlinePage==1) {
            static const char *labels[]={"MAP","STARTING FLEET","STARTING RESOURCES","BOUNTY","HARVESTING","RESEARCH","UNIT CAPS","FUEL BURN","ALLIED VICTORY","HYPERSPACE","RESOURCE INJECTIONS","INJECTION INTERVAL","INJECTION AMOUNT"};
            static const unsigned flags[]={0,0,0,0,32,8,512,1024,2048,16384,64,0,0};
            for(i=0;i<13;i++) {
                rectangle row=muiOnlineRule(i);char value[96];
                if(i==0)snprintf(value,sizeof(value),"%.31s",lobby.config.map);
                else if(i==1)snprintf(value,sizeof(value),"%s",lobby.config.startingFleet==0?"SMALL":lobby.config.startingFleet==1?"MEDIUM":"LARGE");
                else if(i==2)snprintf(value,sizeof(value),"%s",lobby.config.startingResources==0?"LOW":lobby.config.startingResources==1?"MEDIUM":"HIGH");
                else if(i==3)snprintf(value,sizeof(value),"%s",lobby.config.bounty==3?"OFF":lobby.config.bounty==0?"LOW":lobby.config.bounty==1?"MEDIUM":"HIGH");
                else if(i==11)snprintf(value,sizeof(value),"%u SECONDS",lobby.config.injectionInterval);
                else if(i==12)snprintf(value,sizeof(value),"%u RUs",lobby.config.injectionAmount);
                else snprintf(value,sizeof(value),"%s",(lobby.config.gameFlags&flags[i])?"ON":"OFF");
                muiFill(&row,muiSurface);primRectOutline2(&row,1,muiLineSoft);
                muiText(row.x0+uiScaleSize(10),row.y0+uiScaleSize(5),muiMuted,labels[i],muiBodyFont);
                muiTextRight(row.x1-uiScaleSize(10),row.y0+uiScaleSize(5),muiPaper,value,muiBodyFont);
            }
        } else {
            static const char *labels[]={"LAST MOTHERSHIP VICTORY","CAPITAL CAPTURE VICTORY","CARRIER AS MOTHERSHIP","CRATES","RESOURCE LUMP SUM","LUMP SUM TIME","LUMP SUM AMOUNT","COLOR SLOT","HULL BASE","STRIPE COLOR","ENGINE COLOR"};
            static const unsigned flags[]={2,4,8192,4096,128};
            for(i=0;i<11;i++) {
                rectangle row=muiOnlineRule(i);char value[80];OHSlot *slot=&lobby.config.slots[muiOnlineColorSlot];
                if(i<5)snprintf(value,sizeof(value),"%s",(lobby.config.gameFlags&flags[i])?"ON":"OFF");
                else if(i==5)snprintf(value,sizeof(value),"%u SECONDS",lobby.config.lumpTime);
                else if(i==6)snprintf(value,sizeof(value),"%u RUs",lobby.config.lumpAmount);
                else if(i==7)snprintf(value,sizeof(value),"%d / %.17s",muiOnlineColorSlot+1,slot->kind?slot->name:"EMPTY");
                else {color c=i==8?slot->base:i==9?slot->stripe:slot->emission;snprintf(value,sizeof(value),"#%02X%02X%02X",colRed(c),colGreen(c),colBlue(c));}
                muiFill(&row,muiSurface);primRectOutline2(&row,1,muiLineSoft);
                muiText(row.x0+uiScaleSize(10),row.y0+uiScaleSize(5),muiMuted,labels[i],muiBodyFont);
                muiTextRight(row.x1-uiScaleSize(10),row.y0+uiScaleSize(5),muiPaper,value,muiBodyFont);
            }
        }
        muiOnlineButton(muiOnlineBox(.04f,.79f,.20f,.055f),"ADD AI PLAYER",lobby.host);
        muiOnlineButton(muiOnlineBox(.25f,.79f,.20f,.055f),"READY / UNREADY",lobby.config.slots[lobby.ownSlot].ready);
        muiOnlineButton(muiOnlineBox(.75f,.79f,.21f,.055f),lobby.state?"LOADING / PLEASE WAIT":"START MATCH",lobby.host);
        muiText((sdword)(MAIN_WindowWidth*.04f),(sdword)(MAIN_WindowHeight*.86f),muiMuted,
            "Click or scroll values. Team 0 = free-for-all. Changes reset readiness.",muiBodyFont);
    }
    muiOnlineButton(muiOnlineBox(.04f,.91f,.16f,.04f),"BACK / LEAVE",FALSE);
}
void hwOnlineCloseMenu(void)
{if(muiOnlineRegion){regRegionDelete(muiOnlineRegion);muiOnlineRegion=NULL;}}
static udword muiOnlineProcess(regionhandle region,smemsize ID,udword event,udword data)
{
    OHLobby lobby;sdword i,col,x=mouseCursorX(),y=mouseCursorY(),direction=event==RPE_WheelDown?-1:1;
    bool32 inLobby=hwOnlineLobbySnapshot(&lobby);
    rectangle rect;
    (void)region;(void)data;
    if(event==RPE_KeyDown) {
        if(ID==ESCKEY){hwOnlineSend(OH_LEAVE,NULL,0);hwOnlineCloseMenu();return RPR_Redraw;}
        if(!inLobby) {
            size_t count=strlen(muiOnlineCode);
            SDL_Keycode c=SDL_GetKeyFromScancode((SDL_Scancode)ID);
            if(ID==BACKSPACEKEY && count)muiOnlineCode[count-1]=0;
            else if(count<6 && ((c>='a'&&c<='z')||(c>='0'&&c<='9'))) {
                muiOnlineCode[count]=(char)(c>='a'&&c<='z'?c-32:c);muiOnlineCode[count+1]=0;
            }
        }
        return RPR_Redraw;
    }
    if(event!=RPE_ReleaseLeft && event!=RPE_WheelUp && event!=RPE_WheelDown)return 0;
    rect=muiOnlineBox(.04f,.91f,.16f,.04f);
    if(muiPointIn(&rect,x,y)){hwOnlineSend(OH_LEAVE,NULL,0);hwOnlineCloseMenu();return RPR_Redraw;}
    if(!inLobby) {
        rect=muiOnlineBox(.10f,.31f,.30f,.065f);if(muiPointIn(&rect,x,y))hwOnlineSend(OH_CREATE,NULL,0);
        rect=muiOnlineBox(.10f,.49f,.30f,.065f);if(muiPointIn(&rect,x,y)&&strlen(muiOnlineCode)==6)hwOnlineSend(OH_JOIN,muiOnlineCode,8);
        rect=muiOnlineBox(.10f,.58f,.30f,.065f);if(muiPointIn(&rect,x,y))hwOnlineConnect(utyName);
        return RPR_Redraw;
    }
    for(i=0;i<3;i++) {
        rect=muiOnlineBox(.59f+.125f*i,.22f,.12f,.044f);
        if(muiPointIn(&rect,x,y)){muiOnlinePage=i;return RPR_Redraw;}
    }
    rect=muiOnlineBox(.25f,.79f,.20f,.055f);
    if(muiPointIn(&rect,x,y)) {
        unsigned char ready[5];memcpy(ready,&lobby.revision,4);ready[4]=!lobby.config.slots[lobby.ownSlot].ready;
        hwOnlineSend(OH_READY,ready,sizeof(ready));return RPR_Redraw;
    }
    if(!lobby.host || lobby.state)return RPR_Redraw;
    rect=muiOnlineBox(.04f,.79f,.20f,.055f);
    if(muiPointIn(&rect,x,y)){unsigned char ai[]={1,1,0};hwOnlineSend(OH_ADD_AI,ai,sizeof(ai));return RPR_Redraw;}
    rect=muiOnlineBox(.75f,.79f,.21f,.055f);
    if(muiPointIn(&rect,x,y)){hwOnlineSend(OH_START,NULL,0);return RPR_Redraw;}
    for(i=0;i<8;i++)for(col=1;col<5;col++) {
        OHSlot *slot=&lobby.config.slots[i];rect=muiOnlineSlot(i,col);
        if(!slot->kind || !muiPointIn(&rect,x,y))continue;
        if(col==1)slot->race^=1;
        else if(col==2 && slot->kind==2)slot->difficulty=(uint8_t)((slot->difficulty+direction+3)%3);
        else if(col==3)slot->team=(uint8_t)((slot->team+direction+9)%9);
        else if(col==4 && slot->kind==2){unsigned char index=(unsigned char)i;hwOnlineSend(OH_REMOVE_SLOT,&index,1);return RPR_Redraw;}
        else return RPR_Redraw;
        muiOnlineCommit(&lobby);return RPR_Redraw;
    }
    for(i=0;i<13;i++) {
        rect=muiOnlineRule(i);if(!muiPointIn(&rect,x,y))continue;
        if(muiOnlinePage==0) {
            sdword value=lobby.config.rules[i],step=i==6?5:1;
            value+=direction*step;if(value>muiOnlineRuleMax[i])value=muiOnlineRuleMin[i];
            if(value<muiOnlineRuleMin[i])value=muiOnlineRuleMax[i];lobby.config.rules[i]=(uint16_t)value;
        } else if(muiOnlinePage==1) {
            static const unsigned flags[]={0,0,0,0,32,8,512,1024,2048,16384,64,0,0};
            if(i==0) {
                sdword index=0;for(index=0;index<spScenarioListLength;index++)if(!strcmp(spScenarios[index].fileSpec,lobby.config.map))break;
                index=(index+direction+spScenarioListLength)%spScenarioListLength;
                snprintf(lobby.config.map,sizeof(lobby.config.map),"%s",spScenarios[index].fileSpec);
            } else if(i==1)lobby.config.startingFleet=(uint8_t)((lobby.config.startingFleet+direction+3)%3);
            else if(i==2)lobby.config.startingResources=(uint8_t)((lobby.config.startingResources+direction+3)%3);
            else if(i==3)lobby.config.bounty=(uint8_t)((lobby.config.bounty+direction+4)%4);
            else if(i==11)lobby.config.injectionInterval=(uint32_t)muiClamp((sdword)lobby.config.injectionInterval+direction*30,30,3600);
            else if(i==12)lobby.config.injectionAmount=(uint32_t)muiClamp((sdword)lobby.config.injectionAmount+direction*100,100,100000);
            else lobby.config.gameFlags^=flags[i];
        } else {
            static const unsigned flags[]={2,4,8192,4096,128};
            OHSlot *slot=&lobby.config.slots[muiOnlineColorSlot];
            if(i<5)lobby.config.gameFlags^=flags[i];
            else if(i==5)lobby.config.lumpTime=(uint32_t)muiClamp((sdword)lobby.config.lumpTime+direction*30,30,86400);
            else if(i==6)lobby.config.lumpAmount=(uint32_t)muiClamp((sdword)lobby.config.lumpAmount+direction*100,100,1000000);
            else if(i==7){muiOnlineColorSlot=(muiOnlineColorSlot+direction+8)%8;return RPR_Redraw;}
            else if(i<11 && slot->kind) {
                uint32_t *color=i==8?&slot->base:i==9?&slot->stripe:&slot->emission;
                unsigned index;for(index=0;index<8;index++)if(muiOnlinePalette[index]==*color)break;
                *color=muiOnlinePalette[(index+direction+8)%8];
            } else return RPR_Redraw;
        }
        muiOnlineCommit(&lobby);return RPR_Redraw;
    }
    return RPR_Redraw;
}
static void muiOnlineOpen(void)
{
    sdword key;
    if(muiOnlineRegion)return;
    if(!hwOnlineConnected())hwOnlineConnect(utyName);
    muiOnlineRegion=regChildAlloc(ghMainRegion,0,0,0,MAIN_WindowWidth,MAIN_WindowHeight,0,
        RPE_PressLeft|RPE_ReleaseLeft|RPE_WheelUp|RPE_WheelDown|RPE_DrawEveryFrame);
    regDrawFunctionSet(muiOnlineRegion,muiOnlineDraw);regFunctionSet(muiOnlineRegion,muiOnlineProcess);
    for(key=SDL_SCANCODE_A;key<=SDL_SCANCODE_0;key++)
        regKeyChildAlloc(muiOnlineRegion,key,RPE_KeyDown,muiOnlineProcess,1,key);
    regKeyChildAlloc(muiOnlineRegion,ESCKEY,RPE_KeyDown,muiOnlineProcess,1,ESCKEY);
    regKeyChildAlloc(muiOnlineRegion,BACKSPACEKEY,RPE_KeyDown,muiOnlineProcess,1,BACKSPACEKEY);
    regSiblingMoveToFront(muiOnlineRegion);regRecursiveSetDirty(muiOnlineRegion);
}
