#include "FrontendPreviewPose.h"
/* One interceptor, outside the universe. No fleet/static-ship initialization,
   AI, game-clock changes or global team-colour edits are needed by the menu. */
static struct {
    meshdata *mesh;
    Ship ship;
    ShipStaticInfo info;
    lodinfo lod;
    trailstatic *trailInfo;
    shiptrail *trail;
    ShipRace race;
    bool32 attempted, thrust, dragging;
    real32 radius, center[3], yaw, pitch, zoom, accumulator, clock, travel;
    color base, stripe;
    Uint32 tick, paintTick;
    sdword mouseX,mouseY;
} muiShipPreview;

static void muiShipPreviewClose(void)
{
    if(muiShipPreview.trail)trailDelete(muiShipPreview.trail);
    if(muiShipPreview.trailInfo)trailStaticDelete(muiShipPreview.trailInfo);
    if(muiShipPreview.mesh)meshFreeMenuPreview(muiShipPreview.mesh);
    memset(&muiShipPreview,0,sizeof(muiShipPreview));
}

static void muiPreviewObjectMatrix(polygonobject *object,float out[16],sdword depth)
{
    float parent[16];const float *local=(const float *)&object->localMatrix;
    sdword a,b,k;
    if(!object->pMother || depth>=256){memcpy(out,local,16*sizeof(float));return;}
    muiPreviewObjectMatrix(object->pMother,parent,depth+1);
    for(a=0;a<4;++a)for(b=0;b<4;++b) {
        out[a*4+b]=0;for(k=0;k<4;++k)out[a*4+b]+=parent[k*4+b]*local[a*4+k];
    }
}

static void muiShipPreviewLoad(ShipRace race,color base,color stripe)
{
    char path[256],manifest[256],definition[128];
    float low[3]={1e20f,1e20f,1e20f},high[3]={-1e20f,-1e20f,-1e20f};
    sdword i,j,k;sdword length;
    const char *raceName=ShipRaceToStr(race);
    muiShipPreviewClose();muiShipPreview.attempted=TRUE;muiShipPreview.race=race;
    muiShipPreview.yaw=42;muiShipPreview.pitch=-18;muiShipPreview.zoom=1;
    muiShipPreview.base=base;muiShipPreview.stripe=stripe;muiShipPreview.tick=SDL_GetTicks();
    snprintf(path,sizeof(path),"%s\\LightInterceptor\\Rl0\\LOD0\\LightInterceptor.geo",raceName);
    snprintf(manifest,sizeof(manifest),"%s\\LightInterceptor\\RMNative\\visual.rmnative",raceName);
    if(!fileExists(path,0) && !fileExists(manifest,0))return;
    muiShipPreview.mesh=meshLoadMenuPreview(path,base,stripe);
    if(!muiShipPreview.mesh)return;
    for(i=0;i<muiShipPreview.mesh->nPolygonObjects;++i) {
        polygonobject *o=&muiShipPreview.mesh->object[i];float m[16];
        muiPreviewObjectMatrix(o,m,0);
        for(j=0;j<o->nVertices;++j) {
            const vertexentry *v=&o->pVertexList[j];float p[3];
            for(k=0;k<3;++k){p[k]=m[k]*v->x+m[4+k]*v->y+m[8+k]*v->z+m[12+k];low[k]=MIN2(low[k],p[k]);high[k]=MAX2(high[k],p[k]);}
        }
    }
    muiShipPreview.radius=1;
    for(i=0;i<3;++i){muiShipPreview.center[i]=(low[i]+high[i])*.5f;muiShipPreview.radius=MAX2(muiShipPreview.radius,(high[i]-low[i])*.65f);}
    muiShipPreview.info.shiprace=race;muiShipPreview.info.shiptype=LightInterceptor;
    muiShipPreview.info.shipclass=CLASS_Fighter;
    muiShipPreview.info.staticheader.maxvelocity=1000;
    muiShipPreview.info.staticheader.staticCollInfo.collspheresize=muiShipPreview.radius;
    muiShipPreview.lod.nLevels=1;muiShipPreview.lod.level[0].flags=LT_Mesh;
    muiShipPreview.lod.level[0].pData=muiShipPreview.mesh;
    muiShipPreview.info.staticheader.LOD=&muiShipPreview.lod;
    muiShipPreview.info.trailStyle[0]=1;
    muiShipPreview.info.trailWidth[0]=race==R1?17:8.8f;
    muiShipPreview.info.trailHeight[0]=race==R1?3.25f:6.5f;
    muiShipPreview.info.trailAngle[0]=-1;
    muiShipPreview.info.trailRibbonAdjust[0]=race==R1?-1:.5f;
    muiShipPreview.info.trailSpriteRadius[0]=race==R1?-1:20;
    muiShipPreview.info.trailSpriteOffset[0]=race==R1?0:-1.5f;
    /* Read just this interceptor's definition, including local loose overrides. */
    snprintf(definition,sizeof(definition),"%s\\LightInterceptor.shp",raceName);
    length=fileExists(definition,0)?fileSizeGet(definition,0):0;
    if(length>0 && length<1024*1024) {
        char *text,*line;
        text=(char *)memAlloc(length+1,"Preview ship definition",NonVolatile);
        fileLoad(definition,text,0);text[length]=0;
        for(line=strtok(text,"\r\n");line;line=strtok(NULL,"\r\n")) {
            char key[64];float value;
            if(sscanf(line,"%63s %f",key,&value)!=2 || !isfinite(value))continue;
            if(!SDL_strcasecmp(key,"maxvelocity") && value>0)muiShipPreview.info.staticheader.maxvelocity=value;
            else if(!SDL_strcasecmp(key,"trailWidth"))muiShipPreview.info.trailWidth[0]=value;
            else if(!SDL_strcasecmp(key,"trailHeight"))muiShipPreview.info.trailHeight[0]=value;
            else if(!SDL_strcasecmp(key,"trailAngle"))muiShipPreview.info.trailAngle[0]=value;
            else if(!SDL_strcasecmp(key,"trailRibbonAdjust"))muiShipPreview.info.trailRibbonAdjust[0]=value;
            else if(!SDL_strcasecmp(key,"trailSpriteRadius"))muiShipPreview.info.trailSpriteRadius[0]=value;
            else if(!SDL_strcasecmp(key,"trailSpriteOffset"))muiShipPreview.info.trailSpriteOffset[0]=value;
        }
        memFree(text);
        muiShipPreview.trailInfo=trailStaticInfoParse("",definition);
    }
    muiShipPreview.ship.objtype=OBJ_ShipType;
    muiShipPreview.ship.flags=SOF_NISShip;
    muiShipPreview.ship.staticinfo=&muiShipPreview.info;
    muiShipPreview.ship.shiptype=LightInterceptor;muiShipPreview.ship.shiprace=race;
    muiShipPreview.ship.magnitudeSquared=1;
    memset(&muiShipPreview.ship.rotinfo.coordsys,0,sizeof(matrix));
    muiShipPreview.ship.rotinfo.coordsys.m11=muiShipPreview.ship.rotinfo.coordsys.m22=muiShipPreview.ship.rotinfo.coordsys.m33=1;
    snprintf(path,sizeof(path),"%s\\LightInterceptor\\Rl0\\LOD0\\LightInterceptor.mex",raceName);
    length=fileExists(path,0)?fileSizeGet(path,0):0;
    if(length>=(sdword)sizeof(MEXFileHeader) && length<1024*1024) {
        ubyte *data=NULL,*at,*end;sdword n;
        fileLoadAlloc(path,(void **)&data,0);
        if(data) {
        at=data+sizeof(MEXFileHeader);end=data+length;
        for(n=0;n<((MEXFileHeader *)data)->nChunks && at+sizeof(MEXChunk)<=end;++n) {
            MEXChunk *chunk=(MEXChunk *)at;
            size_t size=sizeof(MEXChunk)+(size_t)chunk->chunkSize;
            if(size>(size_t)(end-at))break;
            if(!memcmp(chunk->type,"Col",3) && size>=sizeof(MEXCollisionSphereChunk)) {
                float r=((MEXCollisionSphereChunk *)chunk)->r;
                if(isfinite(r) && r>0)muiShipPreview.info.staticheader.staticCollInfo.collspheresize=r;
            }
            if(!memcmp(chunk->type,"Eng",3) && !memcmp(chunk->name,"Eng0",4) && size>=sizeof(MEXEngineChunk))
                muiShipPreview.info.engineNozzleOffset[0]=((MEXEngineChunk *)chunk)->position;
            at+=size;
        }
        memFree(data);
        }
    }
    /* The current native engine joint is shared with the gameplay trail path. */
    if(!meshRemasteredMarkerPosition(&muiShipPreview.ship,"engine_0",&muiShipPreview.info.engineNozzleOffset[0]) &&
        vecMagnitudeSquared(muiShipPreview.info.engineNozzleOffset[0])==0)
        muiShipPreview.info.engineNozzleOffset[0].z=low[2];
    if(muiShipPreview.trailInfo && muiShipPreview.trailInfo->nSegments>=4)
        muiShipPreview.trail=trailNew(muiShipPreview.trailInfo,&muiShipPreview.ship,FALSE,0);
}

static void muiShipPreviewToggle(void)
{
    muiShipPreview.thrust=!muiShipPreview.thrust;
    muiShipPreview.accumulator=0;muiShipPreview.tick=SDL_GetTicks();
    if(muiShipPreview.trail)trailZeroLength(muiShipPreview.trail);
    muiShipPreview.ship.posinfo.position.z=0;
    muiShipPreview.travel=0;
}

static udword muiShipPreviewInput(const rectangle *view,udword event)
{
    int x=mouseCursorX(),y=mouseCursorY();
    if(event==RPE_PressRight && muiPointIn(view,x,y)) {
        muiShipPreview.dragging=TRUE;muiShipPreview.mouseX=x;muiShipPreview.mouseY=y;return RPR_Redraw;
    }
    if(event==RPE_HoldRight && muiShipPreview.dragging) {
        muiShipPreview.yaw+=(x-muiShipPreview.mouseX)*.45f;
        muiShipPreview.pitch=muiClampF(muiShipPreview.pitch+(y-muiShipPreview.mouseY)*.35f,-85,85);
        muiShipPreview.mouseX=x;muiShipPreview.mouseY=y;return RPR_Redraw;
    }
    if(event==RPE_ReleaseRight)muiShipPreview.dragging=FALSE;
    if((event==RPE_WheelUp || event==RPE_WheelDown) && muiPointIn(view,x,y)) {
        muiShipPreview.zoom=muiClampF(muiShipPreview.zoom*(event==RPE_WheelUp?1.12f:1/1.12f),.35f,5);
        return RPR_Redraw;
    }
    return 0;
}

static void muiShipPreviewDraw(const rectangle *rect,ShipRace race,color base,color stripe,color engine)
{
    GLint viewport[4],mode;float fx,fy,span,aspect,dt;
    hmatrix savedCamera=rndCameraMatrix,savedProjection=rndProjectionMatrix;
    Uint32 now=SDL_GetTicks();sdword i;
    static const GLfloat light[4]={-1,2,3,0},diffuse[4]={.9f,.92f,1,1},ambient[4]={.32f,.34f,.38f,1};
    if(!muiShipPreview.attempted || muiShipPreview.race!=race)muiShipPreviewLoad(race,base,stripe);
    if(!muiShipPreview.mesh) {
        muiText(rect->x0+uiScaleSize(16),rect->y0+uiScaleSize(80),muiMuted,"INTERCEPTOR MODEL UNAVAILABLE",muiBodyFont);return;
    }
    dt=MIN2((now-muiShipPreview.tick)*.001f,.1f);muiShipPreview.tick=now;muiShipPreview.clock+=dt;
    /* Small isolated texture pages update while dragging; never trigger the
       mission-wide registry refresh used for fleet recolouring. */
    if((base!=muiShipPreview.base || stripe!=muiShipPreview.stripe) && now-muiShipPreview.paintTick>=66) {
        meshMenuPreviewColors(muiShipPreview.mesh,base,stripe);
        muiShipPreview.base=base;muiShipPreview.stripe=stripe;muiShipPreview.paintTick=now;
    }
    muiShipPreview.ship.posinfo.velocity.z=muiShipPreview.thrust?muiShipPreview.info.staticheader.maxvelocity:0;
    muiShipPreview.ship.posinfo.isMoving=muiShipPreview.thrust?MOVING_LINEARLY_BIT:0;
    if(muiShipPreview.thrust && muiShipPreview.trail) {
        const real32 step=1.0f/UNIVERSE_UPDATE_RATE_ORIG;
        const real32 speed=muiShipPreview.info.staticheader.maxvelocity;
        muiShipPreview.travel+=speed*dt;
        muiShipPreview.accumulator+=dt;
        while(muiShipPreview.accumulator>=step) {
            muiShipPreview.accumulator-=step;
            muiShipPreview.ship.posinfo.position.z=muiShipPreview.travel-speed*muiShipPreview.accumulator;
            trailUpdateMenuPreview(muiShipPreview.trail,&muiShipPreview.ship.posinfo.position,muiShipPreview.info.staticheader.maxvelocity);
        }
        muiShipPreview.ship.posinfo.position.z=muiShipPreview.travel;
        /* Floating origin for a preview that can remain open indefinitely. */
        if(muiShipPreview.ship.posinfo.position.z>muiShipPreview.info.staticheader.maxvelocity*30) {
            vector delta={0,0,-muiShipPreview.ship.posinfo.position.z};
            trailMove(muiShipPreview.trail,&delta);muiShipPreview.ship.posinfo.position.z=0;muiShipPreview.travel=0;
        }
        for(i=0;i<muiShipPreview.trailInfo->nSegments;++i) {
            float t=(float)i/(muiShipPreview.trailInfo->nSegments-1),fade=i==0?0:powf(1-t,1.35f);
            muiShipPreview.trailInfo->segmentColor[0][i]=colRGB(colRed(engine)*fade,colGreen(engine)*fade,colBlue(engine)*fade);
        }
    }
    glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_MATRIX_MODE,&mode);
    fx=(float)viewport[2]/MAIN_WindowWidth;fy=(float)viewport[3]/MAIN_WindowHeight;
    primModeClear2();glPushAttrib(GL_ALL_ATTRIB_BITS);
    glViewport(viewport[0]+rect->x0*fx,viewport[1]+viewport[3]-rect->y1*fy,(rect->x1-rect->x0)*fx,(rect->y1-rect->y0)*fy);
    glEnable(GL_SCISSOR_TEST);glScissor(viewport[0]+rect->x0*fx,viewport[1]+viewport[3]-rect->y1*fy,(rect->x1-rect->x0)*fx,(rect->y1-rect->y0)*fy);
    glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_FOG);glDisable(GL_CLIP_PLANE0);glDisable(GL_BLEND);
    span=muiShipPreview.radius*1.5f/muiShipPreview.zoom;aspect=(float)(rect->x1-rect->x0)/(rect->y1-rect->y0);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(-span*aspect,span*aspect,-span,span,-1000000,1000000);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glLightfv(GL_LIGHT0,GL_POSITION,light);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0,GL_AMBIENT,ambient);
    glEnable(GL_LIGHT0);glDisable(GL_LIGHT1);glEnable(GL_NORMALIZE);rndLightingEnable(TRUE);
    rndTextureEnvironment(RTE_Modulate);glColor4f(1,1,1,1);
    glRotatef(muiShipPreview.pitch,1,0,0);glRotatef(muiShipPreview.yaw,0,1,0);
    glMultMatrixf(hwFrontendPreviewBasis);glTranslatef(-muiShipPreview.center[0],-muiShipPreview.center[1],-muiShipPreview.center[2]);
    glGetFloatv(GL_MODELVIEW_MATRIX,(float *)&rndCameraMatrix);glGetFloatv(GL_PROJECTION_MATRIX,(float *)&rndProjectionMatrix);
    rndTextureEnable(TRUE);meshRenderMenuPreview(muiShipPreview.mesh,muiShipPreview.thrust?1:0);
    if(muiShipPreview.thrust && muiShipPreview.trail) {
        vector eye;const float *m;
        glTranslatef(0,0,-muiShipPreview.ship.posinfo.position.z);
        glGetFloatv(GL_MODELVIEW_MATRIX,(float *)&rndCameraMatrix);
        m=(const float *)&rndCameraMatrix;
        eye.x=-m[0]*m[12]-m[1]*m[13]+m[2]*(100000-m[14]);
        eye.y=-m[4]*m[12]-m[5]*m[13]+m[6]*(100000-m[14]);
        eye.z=-m[8]*m[12]-m[9]*m[13]+m[10]*(100000-m[14]);
        trailDrawMenuPreview(&muiShipPreview.ship.posinfo.position,muiShipPreview.trail,muiShipPreview.info.staticheader.maxvelocity,&eye);
    }
    hwglSurfaceMaterial(NULL);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);glPopAttrib();
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);rndCameraMatrix=savedCamera;rndProjectionMatrix=savedProjection;
    rndLightingEnable(FALSE);rndTextureEnable(FALSE);primModeSet2();
    /* PopAttrib restored a different binding from the last model/trail page.
       The UI must rebind its next texture rather than trust that old handle. */
    trClearCurrent();
}
