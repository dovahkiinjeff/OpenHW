/* Uses the existing UI renderer and fonts; no external art dependency. */
static regionhandle muiPilotRegion;
static void muiPilotKeycap(sdword x,sdword y,sdword width,const char *key,const char *action,bool32 down)
{
    rectangle r=muiRect(x,y,x+width,y+44),line;
    muiFill(&r,down?colRGB(25,132,145):colRGB(12,28,38));
    primRectOutline2(&r,down?2:1,down?colRGB(135,255,239):colRGB(77,109,124));
    line=muiRect(x,y+2,x+width,y+21);
    muiTextCentered(&line,colRGB(245,250,255),key,muiBodyFont);
    line=muiRect(x,y+22,x+width,y+43);
    muiTextCentered(&line,down?colRGB(255,255,255):colRGB(160,188,203),action,muiCompactFont);
}
static void muiPilotKeyboard(void)
{
    sdword x=16,y=max(300,MAIN_WindowHeight-224),k=74;
    char throttle[80];
    real32 power=shipPilotThrottle();
    if(shipPilotCommandMode())
    {
        Ship *target=shipPilotTarget();vector delta;real32 distance=0;
        rectangle r;
        if(target) { vecSub(delta,target->posinfo.position,shipPilotShip()->posinfo.position);distance=(real32)sqrt(vecMagnitudeSquared(delta)); }
        snprintf(throttle,sizeof(throttle),"COMMAND / %s / RANGE %.0f / BANK %d",shipPilotNavigation(),shipPilotCommandRange(),shipPilotWeaponBank()+1);
        muiText(x,y-48,colRGB(235,176,65),throttle,muiBodyFont);
        snprintf(throttle,sizeof(throttle),target?"TARGET LOCKED / DISTANCE %.0f":"NO TARGET / CTRL + CLICK TO LOCK",distance);
        muiText(x,y-24,colRGB(112,239,222),throttle,muiBodyFont);
        muiPilotKeycap(x,y,112,"Q","APPROACH",keyIsHit(QKEY));
        muiPilotKeycap(x+116,y,112,"W","ORBIT",keyIsHit(WKEY));
        muiPilotKeycap(x+232,y,112,"E","KEEP RANGE",keyIsHit(EKEY));
        muiPilotKeycap(x+348,y,112,"CTRL SPACE","STOP",keyIsHit(CONTROLKEY)&&keyIsHit(SPACEKEY));
        muiPilotKeycap(x+464,y,112,"CEASE FIRE","ALL OFF",shipPilotAutoFire());
        r=muiRect(x,y+48,x+576,y+92);muiFill(&r,colRGB(12,28,38));primRectOutline2(&r,1,colRGB(77,109,124));
        r.x1=x+(sdword)(576*power);muiFill(&r,colRGB(25,100,115));
        snprintf(throttle,sizeof(throttle),"SPEED LIMIT %d%% / CLICK TO SET / +/-",(int)(power*100));
        muiText(x+12,y+61,colRGB(240,247,250),throttle,muiBodyFont);
        muiPilotKeycap(x,y+96,112,"RANGE -","SHORTER",FALSE);
        muiPilotKeycap(x+116,y+96,112,"RANGE +","FARTHER",FALSE);
        muiPilotKeycap(x+464,y+96,112,"UNLOCK","CLEAR TARGET",FALSE);
        muiText(x,y+148,colRGB(160,188,203),"DOUBLE CLICK: FLY / RMB DRAG: CAMERA / WHEEL: ZOOM",muiCompactFont);
        muiText(x,y+172,colRGB(160,188,203),"F1-F12: GUNS / PGUP/DN: BANK / CTRL WHEEL: RANGE / ESC: EXIT",muiCompactFont);
        return;
    }
    snprintf(throttle,sizeof(throttle),"CRUISE: %s %d%%",power<0?"REVERSE":(power>0?"AHEAD":"STOP"),(int)(fabs(power)*100+0.5f));
    muiText(x,y-24,colRGB(112,239,222),throttle,muiBodyFont);
    muiPilotKeycap(x,y,k-4,"1",shipPilotCockpit()?"--":"ZOOM IN",keyIsHit(ONEKEY));
    muiPilotKeycap(x+k,y,k-4,"2",shipPilotCockpit()?"--":"ZOOM OUT",keyIsHit(TWOKEY));
    if(shipPilotCockpitAvailable()) muiPilotKeycap(x+k*2,y,k-4,"C","COCKPIT",keyIsHit(CKEY));
    muiPilotKeycap(x+k*3,y,k-4,"ESC","EXIT",keyIsHit(ESCKEY));
    y+=48;
    muiPilotKeycap(x,y,k-4,"Q","STRAFE L",keyIsHit(QKEY));
    muiPilotKeycap(x+k,y,k-4,"W",shipPilotCockpit()?"ACCEL":"POWER +",keyIsHit(WKEY));
    muiPilotKeycap(x+k*2,y,k-4,"E","STRAFE R",keyIsHit(EKEY));
    muiPilotKeycap(x+k*3,y,k-4,"R","ASCEND",keyIsHit(RKEY));
    y+=48;x+=12;
    muiPilotKeycap(x,y,k-4,"A",shipPilotCockpit()?"ROLL L":"TURN L",keyIsHit(AKEY));
    muiPilotKeycap(x+k,y,k-4,"S",shipPilotCockpit()?"BRAKE/REV":"POWER -",keyIsHit(SKEY));
    muiPilotKeycap(x+k*2,y,k-4,"D",shipPilotCockpit()?"ROLL R":"TURN R",keyIsHit(DKEY));
    muiPilotKeycap(x+k*3,y,k-4,"F","DESCEND",keyIsHit(FKEY));
    y+=48;
    muiPilotKeycap(x+k/2,y,k*3-4,"SPACE","FIRE",keyIsHit(SPACEKEY));
    x=16+k*4+24;y=max(300,MAIN_WindowHeight-224)+48;
    muiPilotKeycap(x,y,90,"LMB","FIRE",mouseLeftButton());
    muiPilotKeycap(x+94,y,90,"RMB",shipPilotCockpit()?"FREELOOK":"RECENTER",mouseRightButton());
    muiPilotKeycap(x+32,y+48,120,"WHEEL",shipPilotCockpit()?"POWER +/-":"ZOOM",FALSE);
    if(shipPilotCockpit()) muiText(x,y-24,colRGB(112,239,222),"MOUSE: STEER",muiBodyFont);
    {
        rectangle body=muiRect(x+198,y,x+246,y+90),half;
        muiFill(&body,colRGB(12,28,38));primRectOutline2(&body,2,colRGB(112,160,174));
        half=muiRect(x+201,y+3,x+220,y+39);muiFill(&half,mouseLeftButton()?colRGB(25,132,145):colRGB(34,51,58));
        half=muiRect(x+224,y+3,x+243,y+39);muiFill(&half,mouseRightButton()?colRGB(25,132,145):colRGB(34,51,58));
        half=muiRect(x+220,y+11,x+224,y+30);muiFill(&half,colRGB(235,176,65));
    }
}
static void muiPilotBar(rectangle r,const char *label,real32 fraction,color tint)
{
    rectangle fill=r;
    char text[160];
    muiFill(&r,colRGB(9,20,27));
    if(fraction>=0)
    {
        fraction=max(0.0f,min(1.0f,fraction));
        fill.x1=fill.x0+(sdword)((fill.x1-fill.x0)*fraction);
        muiFill(&fill,tint);
        snprintf(text,sizeof(text),"%s  %d%%",label,(int)(fraction*100+0.5f));
    }
    else snprintf(text,sizeof(text),"%s  N/A",label);
    primRectOutline2(&r,1,colRGB(134,164,176));
    muiTextCentered(&r,colRGB(255,255,255),text,muiBodyFont);
}
static void muiPilotDraw(regionhandle region)
{
    Ship *ship;
    rectangle button,r;
    sdword i,count,columns,width,x,y,rows;
    char label[128];
    real32 value;
    const char *state;
    region->rect=muiRect(0,0,0,0);
    if(!gameIsRunning || nisIsRunning || smSensorsActive) return;
    ship=shipPilotShip();
    if(!ship && !shipPilotCanEnter()) return;
    muiEnsureFonts();
    button=muiRect(MAIN_WindowWidth-80,180,MAIN_WindowWidth-16,244);
    region->rect=ship?muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight):button;
    if(!ship && shipPilotCommandAvailable())
    {
        r=muiRect(MAIN_WindowWidth-80,108,MAIN_WindowWidth-16,172);
        region->rect.y0=108;
        muiFill(&r,colRGB(44,39,20));primRectOutline2(&r,2,colRGB(235,176,65));
        muiTextCentered(&r,colRGB(255,220,140),"COMMAND",muiCompactFont);
    }
    muiFill(&button,colRGB(15,36,46));
    primRectOutline2(&button,2,colRGB(77,210,227));
    /* Flight-chevron / exit-door glyph, drawn at actual 64x64 pixels. */
    x=(button.x0+button.x1)/2; y=button.y0+18;
    r=muiRect(x-10,y-8,x+10,y+10);
    primRectOutline2(&r,2,colRGB(77,210,227));
    r=muiRect(x-2,y-13,x+2,y+15); muiFill(&r,colRGB(77,210,227));
    r=muiRect(button.x0,button.y0+36,button.x1,button.y1);
    muiTextCentered(&r,colRGB(240,247,250),ship?"EXIT":(shipPilotSquadCount()>1?"SQUAD":"PILOT"),muiBodyFont);
    if(shipPilotSquadCount()>1)
    {
        r=muiRect(x-24,y-2,x-17,y+5);muiFill(&r,colRGB(235,176,65));
        r=muiRect(x+17,y-2,x+24,y+5);muiFill(&r,colRGB(235,176,65));
        primRectOutline2(&button,2,colRGB(235,176,65));
    }
    if(!ship) return;
    if(shipPilotCockpit())
    {
        /* Sparse canopy rails and instrument sill keep the sightline clear. */
        r=muiRect(0,110,10,MAIN_WindowHeight-260);muiFill(&r,colRGB(34,51,58));
        r=muiRect(MAIN_WindowWidth-10,110,MAIN_WindowWidth,MAIN_WindowHeight-260);muiFill(&r,colRGB(34,51,58));
        r=muiRect(10,MAIN_WindowHeight-258,MAIN_WindowWidth-10,MAIN_WindowHeight-250);muiFill(&r,colRGB(168,120,40));
        r=muiRect(MAIN_WindowWidth/2-155,MAIN_WindowHeight-248,MAIN_WindowWidth/2+155,MAIN_WindowHeight-220);
        muiFill(&r,colRGB(12,28,38));
        muiTextCentered(&r,colRGB(112,239,222),"FLIGHT / COCKPIT",muiBodyFont);
        primLine2(10,110,MAIN_WindowWidth/5,155,colRGB(112,160,174));
        primLine2(MAIN_WindowWidth-10,110,MAIN_WindowWidth*4/5,155,colRGB(112,160,174));
        primLine2(10,MAIN_WindowHeight-400,MAIN_WindowWidth/5,MAIN_WindowHeight-258,colRGB(112,160,174));
        primLine2(MAIN_WindowWidth-10,MAIN_WindowHeight-400,MAIN_WindowWidth*4/5,MAIN_WindowHeight-258,colRGB(112,160,174));
        /* Fixed boresight is distinct from the movable steering/aim reticle. */
        x=MAIN_WindowWidth/2;y=MAIN_WindowHeight/2;
        primLine2(x-20,y,x-6,y,colRGB(235,176,65));
        primLine2(x+6,y,x+20,y,colRGB(235,176,65));
        primLine2(x,y-20,x,y-6,colRGB(235,176,65));
        primLine2(x,y+6,x,y+20,colRGB(235,176,65));
    }
    if(shipPilotCockpitAvailable())
    {
        r=muiRect(MAIN_WindowWidth-156,294,MAIN_WindowWidth-16,328);
        muiFill(&r,shipPilotCockpit()?colRGB(25,114,101):colRGB(25,47,57));
        primRectOutline2(&r,1,colRGB(134,164,176));
        muiTextCentered(&r,colRGB(255,255,255),shipPilotCockpit()?"C: EXTERNAL":"C: COCKPIT",muiBodyFont);
    }
    if(shipPilotSquadCount()>1)
    {
        snprintf(label,sizeof(label),"WINGLEADER / %d SHIPS / LIMIT %.0f m/s",shipPilotSquadCount(),shipPilotMaxSpeed());
        muiText(16,MAIN_WindowHeight-270,colRGB(235,176,65),label,muiBodyFont);
    }
    count=ship->gunInfo?ship->gunInfo->numGuns:0;
    columns=max(1,(MAIN_WindowWidth-24)/180);
    if(count>0) columns=min(columns,count);
    width=(MAIN_WindowWidth-24)/columns;
    rows=(count+columns-1)/columns;
    for(i=0;i<count;i++)
    {
        Gun *gun=&ship->gunInfo->guns[i];
        if(!gun->gunstatic) continue;
        value=shipPilotGunReady(ship,gun,&state);
        if(shipPilotCommandMode() && shipPilotWeaponActive(i))
        {
            Ship *locked=shipPilotTarget();
            if(!locked) state="NO LOCK";
            else if(!shipPilotWeaponCanHit(ship,gun,(SpaceObjRotImpTarg *)locked)) state="RANGE/ARC";
        }
        snprintf(label,sizeof(label),"GUN %d  %s",i+1,state);
        if(shipPilotGunSeconds(gun)>0)
            snprintf(label,sizeof(label),"GUN %d  %.1fs",i+1,shipPilotGunSeconds(gun));
        {
            real32 charge=rmPresentationIonChargeRemaining(ship,(unsigned)i);
            if(charge>0)
            {
                value=1-charge/2.0f;
                snprintf(label,sizeof(label),"GUN %d CHARGE %.1fs",i+1,charge);
            }
        }
        r=muiRect(12+(i%columns)*width,12+(i/columns)*30,8+(i%columns+1)*width,38+(i/columns)*30);
        if(shipPilotCommandMode())
        {
            if(i/12==shipPilotWeaponBank()) snprintf(label,sizeof(label),"F%d %s / %s",i%12+1,shipPilotWeaponActive(i)?"ON":"OFF",state);
            else snprintf(label,sizeof(label),"BANK %d GUN %d %s",i/12+1,i+1,shipPilotWeaponActive(i)?"ON":"OFF");
        }
        muiPilotBar(r,label,value,value>=1?colRGB(25,114,101):colRGB(136,87,27));
    }
    width=(MAIN_WindowWidth-24)/4;
    y=20+rows*30;
    r=muiRect(12,y,8+width,y+28);
    muiPilotBar(r,"HEALTH",ship->health*ship->staticinfo->oneOverMaxHealth,colRGB(39,116,64));
    r=muiRect(12+width,y,8+width*2,y+28);
    snprintf(label,sizeof(label),"SPEED / %s %d%%",shipPilotThrottle()<0?"REV":"AHEAD",(int)(fabs(shipPilotThrottle())*100+0.5f));
    value=shipPilotMaxSpeed()>0?(real32)sqrt(vecMagnitudeSquared(ship->posinfo.velocity))/shipPilotMaxSpeed():0;
    muiPilotBar(r,label,value,colRGB(30,100,156));
    r=muiRect(12+width*2,y,8+width*3,y+28);
    muiPilotBar(r,"SPECIAL",shipPilotSpecialReady(ship),colRGB(117,62,157));
    r=muiRect(12+width*3,y,MAIN_WindowWidth-16,y+28);
    muiPilotBar(r,"FUEL",ship->staticinfo->maxfuel>0?ship->fuel/ship->staticinfo->maxfuel:-1,colRGB(155,106,28));
    r=muiRect(MAIN_WindowWidth-156,252,MAIN_WindowWidth-16,286);
    muiFill(&r,shipPilotFireAll()?colRGB(145,65,30):colRGB(25,47,57));
    primRectOutline2(&r,1,colRGB(134,164,176));
    muiTextCentered(&r,colRGB(255,255,255),shipPilotFireAll()?"FIRE ALL: ON":"FIRE ALL: OFF",muiBodyFont);
    muiPilotKeyboard();
    if(!shipPilotCockpit())
    {
        Ship *target=shipPilotTarget();real32 sx,sy;
        if(!shipPilotCommandMode()) muiText(16,MAIN_WindowHeight-296,colRGB(235,176,65),"TAB: LOCK NEAREST TO CURSOR",muiCompactFont);
        if(target)
        {
            bool32 front=shipPilotTargetScreen(target,&sx,&sy);
            if(!front) { sx=MAIN_WindowWidth/2;sy=MAIN_WindowHeight-300; }
            x=(sdword)max(35,min(MAIN_WindowWidth-35,sx));y=(sdword)max(115,min(MAIN_WindowHeight-300,sy));
            r=muiRect(x-25,y-25,x+25,y+25);primRectOutline2(&r,2,colRGB(255,183,51));
            muiText(x-35,y-46,colRGB(255,183,51),front?"LOCKED":"BEHIND",muiBodyFont);
        }
    }
    shipPilotReticle(&x,&y);
    /* Hollow four-bracket reticle keeps the target visible. */
    r=muiRect(x-15,y-15,x-7,y-13); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x+7,y-15,x+15,y-13); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x-15,y+13,x-7,y+15); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x+7,y+13,x+15,y+15); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x-15,y-15,x-13,y-7); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x+13,y-15,x+15,y-7); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x-15,y+7,x-13,y+15); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x+13,y+7,x+15,y+15); muiFill(&r,colRGB(90,240,228));
    r=muiRect(x-1,y-1,x+2,y+2); muiFill(&r,colRGB(255,255,255));
}
static udword muiPilotProcess(regionhandle region,smemsize id,udword event,udword data)
{
    (void)region; (void)id;
    shipPilotMouse(event,data);
    return RPR_Redraw;
}
