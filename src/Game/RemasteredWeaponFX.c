#include "RemasteredWeaponFX.h"
#include "EffectOwner.h"
#include "WindowsDiagnostics.h"
#include "RemasteredWeaponFXData.h"
#include "Clouds.h"
#include "NativeVisualRegistry.h"
#include "RemasteredPresentation.h"
#include "File.h"
#include "Memory.h"
#include "LOD.h"
#include "Mesh.h"
#include "Ships.h"
#include "ETG.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "SinglePlayer.h"
#include "Vector.h"
#include "glinc.h"
#include "NativeGunPosePolicy.h"
#ifdef HW_ENABLE_D3D12_BACKEND
#include "ModernGraphics.h"
#endif
#include <stdio.h>
#include <time.h>
#include <SDL.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#endif

bool32 rmWeaponFxDiagnosticsEnabled=FALSE;
/* Presentation policy only; no simulation, native joints, or save layouts change. */
static bool32 useClassicShipFx=TRUE;
static RmResourceDustStyle resourceDustStyle={1,1,1,.5f,{1,1,1}};
static int resourceDustStyleLoaded, drawingResourceDust;
static sdword resourceDustMission=-1;
static sdword resourceDustCurrentMission(void)
{
    return singlePlayerGame ? spGetCurrentMission() : 0;
}
static bool32 resourceDustPath(char *path,size_t bytes,bool32 legacy)
{
    char *base=SDL_GetBasePath();
    int length;
    if(!base) return FALSE;
    if(legacy) length=snprintf(path,bytes,"%sRMHarvestableDust.cfg",base);
    else length=snprintf(path,bytes,"%sRMHarvestableDust-Mission%02d.cfg",base,resourceDustCurrentMission());
    SDL_free(base);
    return length>0 && (size_t)length<bytes;
}
void rmFxResourceDustReset(void)
{
    RmResourceDustStyle defaults={1,1,1,.5f,{1,1,1}};
    resourceDustStyle=defaults;
    resourceDustStyleLoaded=1;
    resourceDustMission=resourceDustCurrentMission();
}
void rmFxResourceDustReload(void)
{
    char path[1024]; FILE *fp;
    RmResourceDustStyle loaded;
    rmFxResourceDustReset();
    if(!resourceDustPath(path,sizeof(path),FALSE)) return;
    fp=fopen(path,"rb");
    // Keep existing authored global settings as a fallback for unedited maps.
    // Saving never overwrites that fallback or another mission's configuration.
    if(!fp)
    {
        if(!resourceDustPath(path,sizeof(path),TRUE) || !(fp=fopen(path,"rb"))) return;
    }
    if(fscanf(fp,"RM_DUST_V1 %f %f %f %f %f %f %f",&loaded.opacity,&loaded.brightness,
        &loaded.size,&loaded.phase,&loaded.tint[0],&loaded.tint[1],&loaded.tint[2])==7 &&
        isfinite(loaded.opacity) && loaded.opacity>=0 && loaded.opacity<=8 &&
        isfinite(loaded.brightness) && loaded.brightness>=0 && loaded.brightness<=8 &&
        isfinite(loaded.size) && loaded.size>=.25f && loaded.size<=4 &&
        isfinite(loaded.phase) && loaded.phase>=.05f && loaded.phase<=.95f &&
        isfinite(loaded.tint[0]) && loaded.tint[0]>=0 && loaded.tint[0]<=4 &&
        isfinite(loaded.tint[1]) && loaded.tint[1]>=0 && loaded.tint[1]<=4 &&
        isfinite(loaded.tint[2]) && loaded.tint[2]>=0 && loaded.tint[2]<=4) resourceDustStyle=loaded;
    fclose(fp);
}
RmResourceDustStyle *rmFxResourceDustStyle(void)
{
    if(!resourceDustStyleLoaded || resourceDustMission!=resourceDustCurrentMission()) rmFxResourceDustReload();
    return &resourceDustStyle;
}
bool32 rmFxResourceDustSave(void)
{
    char path[1024]; FILE *fp; int result;
    RmResourceDustStyle *s=rmFxResourceDustStyle();
    if(!resourceDustPath(path,sizeof(path),FALSE) || !(fp=fopen(path,"wb"))) return FALSE;
    result=fprintf(fp,"RM_DUST_V1 %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
        s->opacity,s->brightness,s->size,s->phase,s->tint[0],s->tint[1],s->tint[2]);
    return fclose(fp)==0 && result>0;
}
static FILE *diagnosticFile;
static unsigned int diagnosticRows;
static int diagnosticAttempted;
static float diagnosticLastFlush;
static void *package;
static int diagnosticShip(const Ship *ship)
{ return !hwDiagnosticsDisabled() && rmWeaponFxDiagnosticsEnabled && ship &&
    ((ship->shiprace==R1 && ship->shiptype==HeavyInterceptor) || ship->shiptype==HeavyCorvette ||
     (ship->shiprace==P1 && (ship->shiptype==P1StandardCorvette || ship->shiptype==P1MissileCorvette))); }
static void diagnosticRowDetails(const char *event,const Ship *ship,unsigned int owner,unsigned int gun,int slot,
                          const char *reason,float age,const vector *origin,const vector *direction,
                          Camera *camera,unsigned int quads,unsigned int bright,float energy,
                          float lightRadius,float lightIntensity)
{
#if HW_ENABLE_DIAGNOSTICS

    float speed=0,dot=-2,distance=0,azimuth=0,declination=0; vector view;
    if(hwDiagnosticsDisabled() || !rmWeaponFxDiagnosticsEnabled || diagnosticRows>=100000) return;
    if(!diagnosticAttempted) {
        char path[256]; diagnosticAttempted=1;
#ifdef _WIN32
        const char *development=getenv("HW_DEVELOPMENT_ROOT");
        if(development)snprintf(path,sizeof(path),"%s/Diagnostics/muzzle-fx-%lld-%d.csv",development,(long long)time(NULL),_getpid());
        else { _mkdir("Development");_mkdir("Development/Diagnostics");
            snprintf(path,sizeof(path),"Development/Diagnostics/muzzle-fx-%lld-%d.csv",(long long)time(NULL),_getpid()); }
#else
        snprintf(path,sizeof(path),"muzzle-fx-%lld.csv",(long long)time(NULL));
#endif
        diagnosticFile=fopen(path,"w");
        if(diagnosticFile) {
            setvbuf(diagnosticFile,NULL,_IOFBF,65536);
            fputs("sim_time,event,ship_id,gun_index,event_slot,reason,age,speed,lod,ship_flags,fire_fx_enabled,package_loaded,origin_x,origin_y,origin_z,heading_x,heading_y,heading_z,camera_distance,view_dot,submitted_quads,positive_rgb_layers,max_colour_rgb,light_radius,light_intensity,gun_count,gun_angle,gun_declination,owner_visual_scale,ship_type,ship_race,fuel,mad_gun_status,attack_state,has_attack_target\n",diagnosticFile);
        }
    }
    if(!diagnosticFile) return;
    if(ship && ship->gunInfo && gun<(unsigned)ship->gunInfo->numGuns)
        nativeGunMechanicalAngles(&ship->gunInfo->guns[gun],&azimuth,&declination);
    if(ship) speed=sqrtf(vecMagnitudeSquared(ship->posinfo.velocity));
    if(camera && origin) {
        vecSub(view,*origin,camera->eyeposition); distance=sqrtf(vecMagnitudeSquared(view));
        vecSub(view,camera->lookatpoint,camera->eyeposition);
        if(direction && vecMagnitudeSquared(view)>.00001f) { vecNormalize(&view); dot=vecDotProduct(view,*direction); }
    }
    fprintf(diagnosticFile,"%.6f,%s,%u,%u,%d,%s,%.5f,%.3f,%d,%u,%d,%d,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.4f,%u,%u,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.5f,%d,%d,%.5f,%u,%u,%d\n",
        universe.totaltimeelapsed,event,owner,gun,slot,reason,age,speed,ship?ship->currentLOD:-1,
        ship?(unsigned int)ship->flags:0,etgFireEffectsEnabled,package!=NULL,
        origin?origin->x:0,origin?origin->y:0,origin?origin->z:0,
        direction?direction->x:0,direction?direction->y:0,direction?direction->z:0,
        distance,dot,quads,bright,energy,lightRadius,lightIntensity,
        ship && ship->gunInfo?ship->gunInfo->numGuns:0,
        azimuth,declination,
        ship?ship->magnitudeSquared:0,ship?(int)ship->shiptype:-1,ship?(int)ship->shiprace:-1,
        ship?ship->fuel:0,ship?(unsigned)ship->madGunStatus:0,
        ship?(unsigned)ship->aistateattack:0,ship && ship->attackvars.attacktarget!=NULL);
    ++diagnosticRows;
    if(universe.totaltimeelapsed-diagnosticLastFlush>=1.f) {
        fflush(diagnosticFile); diagnosticLastFlush=universe.totaltimeelapsed;
    }

#else

#endif
}
static void diagnosticRow(const char *event,const Ship *ship,unsigned int owner,unsigned int gun,int slot,
                          const char *reason,float age,const vector *origin,const vector *direction,
                          Camera *camera,unsigned int quads,unsigned int bright,float energy)
{
    diagnosticRowDetails(event,ship,owner,gun,slot,reason,age,origin,direction,camera,quads,bright,energy,0,0);
}
void rmWeaponFxDiagnosticFireCheck(Ship *ship,Gun *gun,const char *reason)
{
#if HW_ENABLE_DIAGNOSTICS

    static struct { unsigned int id,gun,reason; float created,next; int valid; } checks[64];
    unsigned int i,hash=2166136261u,index=gun && gun->gunstatic?gun->gunstatic->gunindex:0;
    const char *p;
    if(!rmWeaponFxDiagnosticsEnabled || !ship || ship->shiptype!=HeavyCorvette) return;
    for(p=reason;*p;p++) hash=(hash^(unsigned char)*p)*16777619u;
    for(i=0;i<64;i++) if(checks[i].valid && checks[i].id==ship->shipID.shipNumber &&
        checks[i].gun==index && checks[i].reason==hash && checks[i].created==ship->timeCreated) break;
    if(i==64) for(i=0;i<64 && checks[i].valid;i++);
    if(i==64) i=(ship->shipID.shipNumber+index+hash)%64;
    if(checks[i].valid && checks[i].id==ship->shipID.shipNumber && checks[i].gun==index &&
       checks[i].reason==hash && checks[i].created==ship->timeCreated &&
       universe.totaltimeelapsed<checks[i].next) return;
    checks[i].valid=1; checks[i].id=ship->shipID.shipNumber; checks[i].gun=index;
    checks[i].reason=hash; checks[i].created=ship->timeCreated; checks[i].next=universe.totaltimeelapsed+1;
    diagnosticRow("fire_check",ship,ship->shipID.shipNumber,index,-1,reason,0,
        &ship->posinfo.position,gun?&gun->gunheading:NULL,NULL,0,0,0);

#else

#endif
}
void rmWeaponFxDiagnosticLight(Ship *ship,Gun *gun,const vector *position,const vector *direction,
                              float radius,float intensity)
{
    if(diagnosticShip(ship) && gun && gun->gunstatic)
        diagnosticRowDetails("muzzle_light",ship,ship->shipID.shipNumber,gun->gunstatic->gunindex,-1,
            "timed_world_position",0,position,direction,NULL,0,0,0,radius,intensity);
}
void rmWeaponFxDiagnosticBinding(const Ship *ship,unsigned int gun,const char *reason)
{
    static struct { unsigned int id,gun; float created,next; int valid; } samples[32];
    unsigned int i; char clean[256];
    if(!diagnosticShip(ship)) return;
    for(i=0;i<32;i++) if(samples[i].valid && samples[i].id==ship->shipID.shipNumber &&
        samples[i].gun==gun && samples[i].created==ship->timeCreated) break;
    if(i==32) for(i=0;i<32 && samples[i].valid;i++);
    if(i==32) i=(ship->shipID.shipNumber+gun)%32;
    if(samples[i].valid && samples[i].id==ship->shipID.shipNumber && samples[i].gun==gun &&
       samples[i].created==ship->timeCreated && universe.totaltimeelapsed<samples[i].next) return;
    samples[i].valid=1; samples[i].id=ship->shipID.shipNumber; samples[i].gun=gun;
    samples[i].created=ship->timeCreated; samples[i].next=universe.totaltimeelapsed+1;
    snprintf(clean,sizeof(clean),"%s",reason);
    for(i=0;clean[i];i++) if(clean[i]==',' || clean[i]=='\n' || clean[i]=='\r') clean[i]=';';
    diagnosticRow("native_binding_failed",ship,ship->shipID.shipNumber,gun,-1,clean,0,
        &ship->posinfo.position,NULL,NULL,0,0,0);
}

#define RMFX_EVENTS 128
#if ETG_DISABLEABLE
#define RMFX_ENABLED etgEffectsEnabled
#else
#define RMFX_ENABLED TRUE
#endif
typedef struct {
    uint32_t effect; float born,scale,timeScale; vector position,velocity,direction;
    int active,attached; ShipID ownerID; float ownerCreated; unsigned int gun;
    vector localPosition; float nextDiagnostic; int diagnostic;
    int framed;vector frameRight,frameUp;
} RmFxEvent;
static int attempted,uploaded;
static RmFxHeader *header;
static RmFxMapping *mappings;
static RmFxEffect *effects;
static RmFxLayer *layers;
static RmFxKey *keys;
static unsigned char *pixels;
static GLuint textures[256];
static uint32_t textureWidth[256],textureHeight[256];
static RmFxEvent events[RMFX_EVENTS];

static int muzzleOwnerMatches(const Ship *ship,const RmFxEvent *event)
{
    return ship && ship->shipID.shipNumber==event->ownerID.shipNumber &&
        ship->timeCreated==event->ownerCreated && !(ship->flags&(SOF_Hide|SOF_Dead));
}
static Ship *resolveMuzzleOwner(const RmFxEvent *event,int *usedLiveList)
{
    Ship *ship=ShipIDtoShip(event->ownerID,FALSE);
    Node *node;
    *usedLiveList=0;
    if(muzzleOwnerMatches(ship,event)) return ship;
    /* The capture contains live, firing ships absent from the legacy ID table.
       Resolve those against the current outside-ship list, without retaining
       pointers across frames or changing the simulation's/save's ID table.
       ID AND creation stamp must match; hidden/dead owners remain rejected. */
    for(node=universe.ShipList.head;node;node=node->next) {
        ship=(Ship *)listGetStructOfNode(node);
        if(muzzleOwnerMatches(ship,event)) { *usedLiveList=1; return ship; }
    }
    return NULL;
}

static int loadPackage(void)
{
    sdword size; unsigned char *cursor;
    if(attempted) return package!=NULL;
    attempted=1;
    if(!fileExists("RemasteredFX\\Kushan.rfx",FF_IgnoreBIG)) return 0;
    size=fileSizeGet("RemasteredFX\\Kushan.rfx",FF_IgnoreBIG);
    if(size<=0 || size>128*1024*1024) return 0;
    if(fileLoadAlloc("RemasteredFX\\Kushan.rfx",&package,FF_IgnoreBIG|FF_ReturnNULLOnFail)!=size || !package) return 0;
    if(!rmFxValidate(package,(size_t)size)) { memFree(package); package=NULL; return 0; }
    header=(RmFxHeader *)package; cursor=(unsigned char *)(header+1);
    mappings=(RmFxMapping *)cursor; cursor+=header->mappings*sizeof(*mappings);
    effects=(RmFxEffect *)cursor; cursor+=header->effects*sizeof(*effects);
    layers=(RmFxLayer *)cursor; cursor+=header->layers*sizeof(*layers);
    keys=(RmFxKey *)cursor; cursor+=header->keys*sizeof(*keys); pixels=cursor;
    return 1;
}
static RmFxMapping *mappingIndex(const Ship *ship,unsigned int index,float *scale)
{
    unsigned int i,lod; float nativeScale; const char *name; char qualified[32];
    lodinfo *levels;
    /* Classic combat/utility FX are authoritative again. Retain this importer
       for future work, but do not suppress HW1 effects on native RM hulls. */
    if (useClassicShipFx) return NULL;
    if(!ship || (ship->shiprace!=R1 && ship->shiprace!=R2 && ship->shiprace!=P1 && ship->shiprace!=P2) || !ship->staticinfo || !loadPackage()) return NULL;
    levels=ship->staticinfo->staticheader.LOD;
    if(!levels) return NULL;
    for(i=0;i<(unsigned int)levels->nLevels;i++)
        if((levels->level[i].flags&LM_LODType)==LT_Mesh && meshNativeVisualParameters(levels->level[i].pData,&nativeScale,&lod)) break;
    if(i==(unsigned int)levels->nLevels) return NULL; /* classic hull keeps classic FX */
    name=ShipTypeToStr(ship->shiptype);
    if(ship->shiprace==R2) { snprintf(qualified,sizeof(qualified),"Taiidan_%s",name); name=qualified; }
    for(i=0;i<header->mappings;i++) if(mappings[i].gun==index && !strcmp(mappings[i].ship,name)) {
        *scale=nativeScale*mappings[i].scale; return &mappings[i];
    }
    return NULL;
}
static RmFxMapping *mappingFor(const Ship *ship,Gun *gun,float *scale)
{
    return gun && gun->gunstatic ? mappingIndex(ship,gun->gunstatic->gunindex,scale) : NULL;
}
void rmWeaponFxDiagnosticShot(Ship *ship,Gun *gun,bool32 visible)
{
    if(diagnosticShip(ship) && gun && gun->gunstatic)
        diagnosticRow("shot",ship,ship->shipID.shipNumber,gun->gunstatic->gunindex,-1,
            visible?"owner_on_render_list":"owner_not_on_render_list",0,&ship->posinfo.position,NULL,NULL,0,0,0);
}
bool32 rmWeaponFxProjectileAvailable(const Ship *ship,Gun *gun)
{
    float scale; RmFxMapping *m=mappingFor(ship,gun,&scale);
    return m && m->projectile!=RMFX_NONE;
}
static int spawn(uint32_t id,float scale,const vector *position,const vector *direction,const vector *velocity)
{
    unsigned int i,activeLayers=0;
    if(id==RMFX_NONE) return 0;
    for(i=0;i<RMFX_EVENTS;i++)
        if(events[i].active && (universe.totaltimeelapsed-events[i].born)*events[i].timeScale<effects[events[i].effect].duration)
            activeLayers+=effects[events[i].effect].count;
    if(activeLayers+effects[id].count>4096) return 0;
    for(i=0;i<RMFX_EVENTS;i++) if(!events[i].active || (universe.totaltimeelapsed-events[i].born)*events[i].timeScale>=effects[events[i].effect].duration) {
        events[i].effect=id; events[i].born=universe.totaltimeelapsed; events[i].scale=scale;
        events[i].position=*position; events[i].direction=*direction; events[i].velocity=*velocity;
        events[i].timeScale=1;
        events[i].active=1; events[i].attached=0; events[i].diagnostic=0;events[i].framed=0; return (int)i+1;
    }
    return 0; /* bounded pool: caller retains original effect */
}
bool32 rmFxShipDeath(Ship *ship)
{
    float scale;RmFxMapping *m;vector direction={0,0,1};int slot;
    if(!RMFX_ENABLED || !ship)return FALSE;
    m=mappingIndex(ship,56,&scale);
    if(!m || m->projectile==RMFX_NONE)return FALSE;
    matGetVectFromMatrixCol3(direction,ship->rotinfo.coordsys);
    if(vecMagnitudeSquared(direction)<.00001f)return FALSE;
    vecNormalize(&direction);
    slot=spawn(m->projectile,scale,&ship->posinfo.position,&direction,&ship->posinfo.velocity);
    if(!slot)return FALSE;
    events[slot-1].framed=1;
    matGetVectFromMatrixCol1(events[slot-1].frameRight,ship->rotinfo.coordsys);
    matGetVectFromMatrixCol2(events[slot-1].frameUp,ship->rotinfo.coordsys);
#ifdef HW_ENABLE_D3D12_BACKEND
    {
        HWModernDynamicLightEmitter light;memset(&light,0,sizeof(light));
        light.source=HW_MODERN_LIGHT_EXPLOSION;light.shape=HW_MODERN_LIGHT_POINT;
        light.position[0]=ship->posinfo.position.x;light.position[1]=ship->posinfo.position.y;light.position[2]=ship->posinfo.position.z;
        light.color[0]=1;light.color[1]=.65f;light.color[2]=.2f;
        light.radius=fmaxf(3,ship->staticinfo->staticheader.staticCollInfo.collspheresize*.25f);
        light.intensity=light.radius*24;hwModernGraphicsTriggerDynamicLight(&light,.4f);
    }
#endif
    return TRUE;
}
bool32 rmWeaponFxMuzzle(Ship *ship,Gun *gun,const vector *position,const vector *direction)
{
    if(ship&&gun&&gun->gunstatic&&rmPresentationOwnsGunFx(ship,gun->gunstatic->gunindex))return TRUE;
    float scale; RmFxMapping *m=mappingFor(ship,gun,&scale); int slot; unsigned int index;
    vector relative,zero={0,0,0};
    if(!m || !RMFX_ENABLED || !etgFireEffectsEnabled) {
        if(diagnosticShip(ship)) diagnosticRow("spawn_rejected",ship,ship->shipID.shipNumber,gun->gunstatic->gunindex,-1,
            !m?"no_native_mapping":"effects_disabled",0,position,direction,NULL,0,0,0);
        return FALSE;
    }
    if(!ship->gunInfo) return FALSE;
    for(index=0;index<(unsigned int)ship->gunInfo->numGuns;index++)
        if(&ship->gunInfo->guns[index]==gun) break;
    if(index==(unsigned int)ship->gunInfo->numGuns) return FALSE;
    slot=spawn(m->muzzle,scale,position,direction,&zero);
    if(!slot) {
        if(diagnosticShip(ship)) diagnosticRow("spawn_rejected",ship,ship->shipID.shipNumber,index,-1,
            m->muzzle==RMFX_NONE?"no_muzzle_clip":"pool_or_layer_budget",0,position,direction,NULL,0,0,0);
        return FALSE;
    }
    /* Resolve the live ship by ID each draw; never retain a ship/gun pointer.
       The creation stamp also rejects an ID reused after deletion. */
    events[slot-1].attached=1; events[slot-1].ownerID=ship->shipID;
    /* Match the beam projectile's normalized visual clock. RM's five-second
       charge must reach its peak during HW1's firing window, not afterwards.
       Ordinary projectile flashes and impacts keep their authored seconds. */
    if(gun->gunstatic->bulletType==BULLET_Beam && gun->gunstatic->bulletlifetime>.0001f)
        events[slot-1].timeScale=effects[m->muzzle].duration/gun->gunstatic->bulletlifetime;
    events[slot-1].ownerCreated=ship->timeCreated; events[slot-1].gun=index;
    events[slot-1].velocity=ship->posinfo.velocity; /* launch steam inherits emission-time motion */
    vecSub(relative,*position,ship->posinfo.position);
    matMultiplyVecByMat(&events[slot-1].localPosition,&relative,&ship->rotinfo.coordsys);
    events[slot-1].diagnostic=diagnosticShip(ship); events[slot-1].nextDiagnostic=universe.totaltimeelapsed;
    if(events[slot-1].diagnostic) diagnosticRow("spawn",ship,ship->shipID.shipNumber,index,slot-1,
        "attached",0,position,direction,NULL,effects[m->muzzle].count,0,0);
    return TRUE;
}
bool32 rmWeaponFxImpact(Bullet *bullet,const vector *position,const vector *velocity)
{
    float scale; RmFxMapping *m=mappingFor(bullet->owner,bullet->gunowner,&scale); vector direction;
    if(!m || !RMFX_ENABLED || !etgHitEffectsEnabled) return FALSE;
    vecCopyAndNegate(direction,bullet->bulletheading);
    return spawn(m->impact,scale,position,&direction,velocity);
}
bool32 rmWeaponFxMissileImpact(Missile *missile,const vector *position,const vector *velocity)
{
    float scale; RmFxMapping *m; vector direction;
    Ship *ship=missile->owner;
    /* All native R1 mine/missile launchers use a single weapon-fire family.
       No pointer to a launching gun is added to the saved Missile layout. */
    if(!ship || (ship->shiptype!=MissileDestroyer && ship->shiptype!=MinelayerCorvette && ship->shiptype!=P1MissileCorvette) ||
       !ship->gunInfo || ship->gunInfo->numGuns<=0 || !RMFX_ENABLED || !etgHitEffectsEnabled) return FALSE;
    m=mappingFor(ship,&ship->gunInfo->guns[0],&scale);
    if(!m) return FALSE;
    direction.x=-missile->rotinfo.coordsys.m13;
    direction.y=-missile->rotinfo.coordsys.m23;
    direction.z=-missile->rotinfo.coordsys.m33;
    return spawn(m->impact,scale,position,&direction,velocity);
}
static void uploadTextures(void)
{
    unsigned int i; unsigned char *cursor=pixels;
    if(uploaded) return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glGenTextures(header->textures,textures);
    for(i=0;i<header->textures;i++) {
        uint32_t dim[2]; memcpy(dim,cursor,sizeof(dim)); cursor+=sizeof(dim);
        textureWidth[i]=dim[0]; textureHeight[i]=dim[1];
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,dim[0],dim[1],0,GL_RGBA,GL_UNSIGNED_BYTE,cursor);
        cursor+=(size_t)dim[0]*dim[1]*4;
    }
    glPopAttrib(); uploaded=1;
}
static void sample(RmFxCurve curve,float phase,float *out)
{
    unsigned int i,j; const RmFxKey *a=&keys[curve.first],*b=a; float t=0;
    for(i=1;i<curve.count;i++) {
        b=&keys[curve.first+i]; if(phase<=b->time) break; a=b;
    }
    if(b->time>a->time) t=fminf(1,fmaxf(0,(phase-a->time)/(b->time-a->time)));
    for(j=0;j<4;j++) out[j]=a->value[j]+(b->value[j]-a->value[j])*t;
}
static void drawEffect(uint32_t id,float age,float scale,const vector *position,const vector *direction,
                       const vector *velocity,float beamLength,Camera *camera,int muzzle,
                       unsigned int *submitted,unsigned int *positive,float *maxColour,
                       const RmFxEvent *emission)
{
    unsigned int i; vector forward,right,up,side,axisU,axisV,center,p,emitterRight,emitterUp;
    vector reference={0,0,1};
    RmFxEffect *effect=&effects[id];
    vecSub(forward,camera->lookatpoint,camera->eyeposition); vecNormalize(&forward);
    vecCrossProduct(right,forward,camera->upvector); vecNormalize(&right); vecCrossProduct(up,right,forward);
    vecCrossProduct(side,*direction,forward); if(vecMagnitudeSquared(side)<.00001f) side=right; else vecNormalize(&side);
    if(fabsf(direction->z)>.95f) { reference.y=1; reference.z=0; }
    vecCrossProduct(emitterRight,*direction,reference); vecNormalize(&emitterRight);
    vecCrossProduct(emitterUp,emitterRight,*direction);
    if(emission && emission->framed) {emitterRight=emission->frameRight;emitterUp=emission->frameUp;}
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE); glDisable(GL_ALPHA_TEST);
    /* Fog adds a constant colour to black additive texels, exposing the quad.
       No ship/UI scissor rectangle may leak into this world-space pass. */
    glDisable(GL_FOG); glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for(i=0;i<effect->count;i++) {
        RmFxLayer *l=&layers[effect->first+i]; float time=age-l->delay,phase;
        vector layerPosition=*position,layerDirection=*direction,layerVelocity=*velocity;
        vector layerRight=emitterRight,layerUp=emitterUp;
        unsigned int blend=l->blend & 255u;
        float width[4],length[4],colour[4],offset[4],repeat[4],scroll[4],s=scale*l->scale;
        float u0,v0,u1,v1; unsigned int frame; int j;
        if(time<0 || (!l->loop && time>=l->duration)) continue;
        if(emission && (l->blend & RMFX_DETACHED_PARTICLE)) {
            vector launchReference={0,0,1};
            layerPosition=emission->position;layerDirection=emission->direction;
            layerVelocity.x=layerVelocity.y=layerVelocity.z=0;
            if(l->blend & RMFX_INHERIT_VELOCITY) layerVelocity=emission->velocity;
            /* Delayed particles are emitted along the launch-time trajectory,
               then drift freely. Flashes continue following the live muzzle. */
            layerPosition.x+=layerVelocity.x*l->delay;
            layerPosition.y+=layerVelocity.y*l->delay;
            layerPosition.z+=layerVelocity.z*l->delay;
            if(fabsf(layerDirection.z)>.95f) {launchReference.y=1;launchReference.z=0;}
            vecCrossProduct(layerRight,layerDirection,launchReference);vecNormalize(&layerRight);
            vecCrossProduct(layerUp,layerRight,layerDirection);
        }
        phase=l->loop?fmodf(time,l->duration)/l->duration:fminf(1,time/l->duration);
        sample(l->curves[0],phase,width); sample(l->curves[1],phase,length); sample(l->curves[2],phase,colour);
        sample(l->curves[3],phase,offset); sample(l->curves[4],phase,repeat);
        if(l->kind!=3)sample(l->curves[5],phase,scroll);else memset(scroll,0,sizeof(scroll));
        /* RM Z is the emitter's forward axis. Convert into the gun's world basis. */
        center=layerPosition;
        center.x+=layerVelocity.x*time+s*(layerRight.x*(offset[0]+l->velocity[0]*time)+layerUp.x*(offset[1]+l->velocity[1]*time)+layerDirection.x*(offset[2]+l->velocity[2]*time));
        center.y+=layerVelocity.y*time+s*(layerRight.y*(offset[0]+l->velocity[0]*time)+layerUp.y*(offset[1]+l->velocity[1]*time)+layerDirection.y*(offset[2]+l->velocity[2]*time));
        center.z+=layerVelocity.z*time+s*(layerRight.z*(offset[0]+l->velocity[0]*time)+layerUp.z*(offset[1]+l->velocity[1]*time)+layerDirection.z*(offset[2]+l->velocity[2]*time));
        if(l->kind==1) {
            float extent=beamLength>0?beamLength:length[0]*s;
            vecScalarMultiply(axisV,side,width[0]*s*.5f);
            vecScalarMultiply(axisU,layerDirection,extent*.5f); vecAddTo(center,axisU);
        } else { vecScalarMultiply(axisU,right,width[0]*s); vecScalarMultiply(axisV,up,width[0]*s); }
        frame=(unsigned int)(time*l->fps); if(l->loop) frame%=l->frames; else if(frame>=l->frames) frame=l->frames-1;
        u0=(float)(frame/l->rows)/l->columns; v0=(float)(frame%l->rows)/l->rows;
        u1=u0+1.0f/l->columns; v1=v0+1.0f/l->rows;
        if(l->frames>1) {
            /* Linear filtering must not sample the neighbouring atlas cell. */
            float du=.5f/textureWidth[l->texture],dv=.5f/textureHeight[l->texture];
            u0+=du; u1-=du; v0+=dv; v1-=dv;
        }
        if(l->frames==1 && l->kind==1) { u0=scroll[0]; u1=u0+repeat[0]; }
        glBindTexture(GL_TEXTURE_2D,textures[l->texture]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,l->kind==1 && l->frames==1?GL_REPEAT:GL_CLAMP_TO_EDGE);
        /* World FX must respect opaque geometry regardless of imported flags. */
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        glBlendFunc(blend==2?GL_ONE:GL_SRC_ALPHA,blend==1?GL_ONE_MINUS_SRC_ALPHA:GL_ONE);
        if(drawingResourceDust) {
            /* Additive ONE/ONE layers encode coverage in RGB, not alpha. */
            float energy=resourceDustStyle.brightness*(blend==2?resourceDustStyle.opacity:1);
            colour[0]*=energy*resourceDustStyle.tint[0];
            colour[1]*=energy*resourceDustStyle.tint[1];
            colour[2]*=energy*resourceDustStyle.tint[2];
            colour[3]=fminf(1,colour[3]*resourceDustStyle.opacity);
        }
        glColor4f(colour[0],colour[1],colour[2],colour[3]);
        if(l->kind==3) {
            unsigned vertex;
            glBegin(GL_TRIANGLES);
            for(vertex=0;vertex<l->curves[5].count;vertex+=2) {
                const RmFxKey *v=&keys[l->curves[5].first+vertex],*uv=v+1;
                float size=width[0]*s;
                p.x=center.x+size*(layerRight.x*v->value[0]+layerUp.x*v->value[1]+layerDirection.x*v->value[2]);
                p.y=center.y+size*(layerRight.y*v->value[0]+layerUp.y*v->value[1]+layerDirection.y*v->value[2]);
                p.z=center.z+size*(layerRight.z*v->value[0]+layerUp.z*v->value[1]+layerDirection.z*v->value[2]);
                glTexCoord2f(u0+uv->value[0]*(u1-u0),v0+uv->value[1]*(v1-v0));glVertex3f(p.x,p.y,p.z);
            }
            glEnd();continue;
        }
        if(submitted) {
            float energy=fmaxf(colour[0],fmaxf(colour[1],colour[2]));
            ++*submitted;
            if(energy>.0001f && width[0]>.0001f && (blend==2 || colour[3]>.0001f)) ++*positive;
            *maxColour=fmaxf(*maxColour,energy);
        }
        glBegin(GL_TRIANGLE_STRIP);
        for(j=0;j<4;j++) {
            float x=(j&1)?1.f:-1.f,y=(j&2)?1.f:-1.f;
            p.x=center.x+x*axisU.x+y*axisV.x; p.y=center.y+x*axisU.y+y*axisV.y; p.z=center.z+x*axisU.z+y*axisV.z;
            glTexCoord2f((j&1)?u1:u0,(j&2)?v1:v0); glVertex3f(p.x,p.y,p.z);
        }
        glEnd();
    }
    glPopAttrib();
}
bool32 rmWeaponFxDrawBullet(Bullet *bullet,Camera *camera)
{
    float scale,beamLength=0,age=bullet->timelived; vector zero={0,0,0},origin=bullet->posinfo.position,direction=bullet->bulletheading;
    RmFxMapping *m;
    if(!RMFX_ENABLED || !etgBulletEffectsEnabled || bullet->bulletType==BULLET_SpecialBurst) return FALSE;
    m=mappingFor(bullet->owner,bullet->gunowner,&scale);
    if(!m || m->projectile==RMFX_NONE) return FALSE;
    if(bullet->bulletType==BULLET_Beam) {
        if(bullet->timelived<=UNIVERSE_UPDATE_PERIOD) return TRUE;
        beamLength=bullet->beamtraveldist;
        if(beamLength<=0) return TRUE;
        /* A visual clip follows HW1's beam lifetime, not RM gameplay timing. */
        if(bullet->totallifetime>0)
            age=fminf(.9999f,bullet->timelived/bullet->totallifetime)*effects[m->projectile].duration;
        {
            vector local,world,end;
            matrix orientation=bullet->owner->rotinfo.coordsys;
            if(meshRemasteredGunMuzzle(bullet->owner,bullet->gunowner->gunstatic->gunindex,&local))
            {
                /* Keep the simulation's collision endpoint, but draw from the
                   currently animated RM muzzle rather than a legacy pivot. */
                vecScalarMultiply(end,direction,beamLength); vecAddTo(end,origin);
                matMultiplyMatByVec(&world,&orientation,&local);
                vecAdd(origin,world,bullet->owner->posinfo.position);
                vecSub(direction,end,origin); beamLength=sqrtf(vecMagnitudeSquared(direction));
                if(beamLength<=.0001f) return TRUE;
                vecNormalize(&direction);
            }
        }
    }
    uploadTextures();
    drawEffect(m->projectile,age,scale,&origin,&direction,&zero,beamLength,camera,0,NULL,NULL,NULL,NULL);
    return TRUE;
}
static bool32 drawExhaust(RmFxMapping *m,float scale,vector direction,const vector *segments,unsigned int count,Camera *camera)
{
    unsigned i,j;vector zero={0,0,0};
    if(vecMagnitudeSquared(direction)<.00001f)return FALSE;
    vecNormalize(&direction);uploadTextures();
#ifdef HW_ENABLE_D3D12_BACKEND
    if(m->muzzle!=RMFX_NONE) {
        HWModernDynamicLightEmitter light;float width[4],colour[4];
        RmFxLayer *glow=&layers[effects[m->muzzle].first];
        sample(glow->curves[0],0,width);sample(glow->curves[2],0,colour);
        memset(&light,0,sizeof(light));light.source=HW_MODERN_LIGHT_ENGINE;
        light.shape=count>1?HW_MODERN_LIGHT_LINE:HW_MODERN_LIGHT_POINT;
        light.position[0]=segments[0].x;light.position[1]=segments[0].y;light.position[2]=segments[0].z;
        light.endPosition[0]=segments[count>8?8:count-1].x;light.endPosition[1]=segments[count>8?8:count-1].y;light.endPosition[2]=segments[count>8?8:count-1].z;
        light.color[0]=colour[0];light.color[1]=colour[1];light.color[2]=colour[2];
        light.radius=fmaxf(.65f,width[0]*scale*.36f);light.intensity=light.radius*1.15f;
        hwModernGraphicsSubmitDynamicLight(&light);
    }
#endif
    if(m->muzzle!=RMFX_NONE)
        drawEffect(m->muzzle,0,scale,&segments[0],&direction,&zero,0,camera,0,NULL,NULL,NULL,NULL);
    if(m->projectile==RMFX_NONE || count<2)return TRUE;
    /* Reuse the simulation's world-space trail history. RM pixels, authored
       widths and faction colour gradient replace Classic's procedural plume,
       without changing flight, trail sampling or saved ship layouts. */
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    for(i=0;i<effects[m->projectile].count;++i) {
        RmFxLayer *l=&layers[effects[m->projectile].first+i];float width[4],colour[4];
        sample(l->curves[0],0,width);glBindTexture(GL_TEXTURE_2D,textures[l->texture]);
        glBegin(GL_TRIANGLE_STRIP);
        for(j=0;j<count;++j) {
            vector tangent,view,side,p;float phase=(float)j/(count-1),half=width[0]*scale*.5f;
            if(j+1<count) {vecSub(tangent,segments[j+1],segments[j]);}
            else {vecSub(tangent,segments[j],segments[j-1]);}
            vecSub(view,camera->eyeposition,segments[j]);vecCrossProduct(side,tangent,view);
            if(vecMagnitudeSquared(side)<.00001f)side=camera->upvector;else vecNormalize(&side);
            sample(l->curves[2],phase,colour);glColor4f(colour[0],colour[1],colour[2],colour[3]*(1-phase));
            vecScalarMultiply(side,side,half);vecSub(p,segments[j],side);
            glTexCoord2f(phase,0);glVertex3f(p.x,p.y,p.z);
            vecAdd(p,segments[j],side);glTexCoord2f(phase,1);glVertex3f(p.x,p.y,p.z);
        }
        glEnd();
    }
    glPopAttrib();return TRUE;
}
bool32 rmFxDrawEngine(Ship *ship,unsigned int engine,const vector *segments,unsigned int count,Camera *camera)
{
    float scale;RmFxMapping *m;vector direction;
    if(!RMFX_ENABLED || !ship || !camera || !segments || !count || engine>=8)return FALSE;
    m=mappingIndex(ship,48+engine,&scale);if(!m)return FALSE;
    if(ship->flags&(SOF_Disabled|SOF_Clamped|SOF_Dead|SOF_Hide))return TRUE;
    matGetVectFromMatrixCol3(direction,ship->rotinfo.coordsys);
    return drawExhaust(m,scale,direction,segments,count,camera);
}
static RmFxMapping *namedMapping(const char *name,unsigned index)
{
    unsigned i;
    /* Harvestable dust has its separate direct lookup and remains unchanged. */
    if (useClassicShipFx) return NULL;
    if(!loadPackage())return NULL;
    for(i=0;i<header->mappings;++i)
        if(mappings[i].gun==index && !strcmp(mappings[i].ship,name))return &mappings[i];
    return NULL;
}
bool32 rmFxDrawMissileTrail(Missile *missile,const vector *segments,unsigned int count,Camera *camera)
{
    RmFxMapping *m;vector direction;unsigned race;
    if(!RMFX_ENABLED || !missile || !camera || !segments || !count)return FALSE;
    if(missile->race!=R1 && missile->race!=R2 && missile->race!=P1)return FALSE;
    race=missile->race==R1?0:missile->race==R2?1:2;
    m=namedMapping("MissileExhaust",race);if(!m)return FALSE;
    matGetVectFromMatrixCol3(direction,missile->rotinfo.coordsys);
    return drawExhaust(m,1,direction,segments,count,camera);
}
bool32 rmFxDrawUtilityEffect(Effect *effect,Camera *camera)
{
    RmFxMapping *m;Ship *ship;vector direction,zero={0,0,0};float length;
    if(!RMFX_ENABLED || !effect || !camera || !(ship=effectOwnerShip(effect)) || ship->rceffect!=effect)return FALSE;
    /* Harvesting belongs to HW1's ETG resource effect; repair keeps its
       existing RM visual. Do not suppress the original collector effect. */
    if(ship->shiptype!=RepairCorvette)return FALSE;
    m=namedMapping("Utility",ship->shiptype==RepairCorvette?2:ship->shiprace==R1?0:1);
    if(!m || m->projectile==RMFX_NONE)return FALSE;
    length=((real32*)effect->variable)[SCALESHIFT(ETG_ResourceLengthParam)];
    if(!isfinite(length) || length<=0)return TRUE;
    matGetVectFromMatrixCol3(direction,effect->rotinfo.coordsys);
    if(vecMagnitudeSquared(direction)<.00001f)return FALSE;
    vecNormalize(&direction);uploadTextures();
    drawEffect(m->projectile,fmodf(effect->timeElapsed,effects[m->projectile].duration),1,
        &effect->posinfo.position,&direction,&zero,length,camera,0,NULL,NULL,NULL,NULL);
    return TRUE;
}
bool32 rmFxDrawLegacyEffect(Effect *effect,unsigned int kind,float radius,Camera *camera)
{
    RmFxMapping *m;unsigned i,j;float extent=0,age;vector direction,zero={0,0,0};
    if(!RMFX_ENABLED || !effect || !camera || radius<=0 || !isfinite(radius))return FALSE;
    m=namedMapping("LegacyFX",kind);if(!m || m->projectile==RMFX_NONE)return FALSE;
    for(i=0;i<effects[m->projectile].count;++i) {
        RmFxLayer *l=&layers[effects[m->projectile].first+i];
        for(j=0;j<l->curves[0].count;++j)
            extent=fmaxf(extent,fabsf(keys[l->curves[0].first+j].value[0])*l->scale);
    }
    if(extent<=0)return FALSE;
    age=effect->timeElapsed;
    if(kind<3)age=fmodf(fmaxf(age,0),effects[m->projectile].duration);
    if(age>=effects[m->projectile].duration)return TRUE;
    matGetVectFromMatrixCol3(direction,effect->rotinfo.coordsys);
    if(vecMagnitudeSquared(direction)<.00001f)direction.z=1;
    vecNormalize(&direction);uploadTextures();
    drawEffect(m->projectile,age,radius/extent,&effect->posinfo.position,&direction,&zero,0,camera,0,NULL,NULL,NULL,NULL);
    return TRUE;
}
bool32 rmFxDrawResourceDust(DustCloud *dust,Camera *camera)
{
    RmResourceDustStyle *style=rmFxResourceDustStyle();
    unsigned i,j;float radius,extent=0;RmFxMapping *m=NULL;
    cloudSystem *system; vector zero={0,0,0},forward={0,0,1};
    if(!dust || !camera || dust->objtype!=OBJ_DustType || !dust->stub ||
       dust->resourcevalue<=0 || (dust->flags&(SOF_Dead|SOF_Hide)) || !loadPackage()) return FALSE;
    for(i=0;i<header->mappings;++i) if(!strcmp(mappings[i].ship,"HarvestableDust") &&
         mappings[i].gun==(unsigned)dust->dustcloudtype) {m=&mappings[i];break;}
    if(!m || m->projectile==RMFX_NONE) return FALSE;
    system=(cloudSystem *)dust->stub;radius=system->radius*system->healthFactor;
    if(!isfinite(radius) || radius<=0) return TRUE;
    /* Use authored RM sprites/curves at native pixel resolution. Harvesting
       changes only the resource instance footprint, never its source asset. */
    for(i=0;i<effects[m->projectile].count;++i) {
        RmFxLayer *l=&layers[effects[m->projectile].first+i];float width=0,offset=0;
        for(j=0;j<l->curves[0].count;++j) width=fmaxf(width,fabsf(keys[l->curves[0].first+j].value[0]));
        for(j=0;j<l->curves[3].count;++j) {
            RmFxKey *k=&keys[l->curves[3].first+j];
            offset=fmaxf(offset,sqrtf(k->value[0]*k->value[0]+k->value[1]*k->value[1]+k->value[2]*k->value[2]));
        }
        extent=fmaxf(extent,l->scale*(width+offset));
    }
    if(extent<=0) return FALSE;
    uploadTextures();
    /* Hold the persistent RM emitter in its fully populated state: no
       periodic restart/pop as the camera moves and no distance LOD reduction. */
    drawingResourceDust=1;
    drawEffect(m->projectile,effects[m->projectile].duration*style->phase,radius/extent*style->size,
        &dust->posinfo.position,&forward,&zero,0,camera,0,NULL,NULL,NULL,NULL);
    drawingResourceDust=0;
    return TRUE;
}
void rmWeaponFxDrawTransient(Camera *camera)
{
    rmPresentationDraw(camera);
    if(rmWeaponFxDiagnosticsEnabled) {
        Node *node;
        for(node=universe.ShipList.head;node;node=node->next) {
            Ship *ship=(Ship *)listGetStructOfNode(node);
            if(ship->shiptype==HeavyCorvette)
                rmWeaponFxDiagnosticFireCheck(ship,NULL,"live_combat_state");
        }
    }
    unsigned int i; if(!package || !RMFX_ENABLED) return;
    uploadTextures();
    for(i=0;i<RMFX_EVENTS;i++) if(events[i].active) {
        float age=(universe.totaltimeelapsed-events[i].born)*events[i].timeScale;
        unsigned int submitted=0,positive=0; float maxColour=0;
        int logFrame=events[i].diagnostic && universe.totaltimeelapsed>=events[i].nextDiagnostic;
        Ship *diagnosticOwner=NULL;
        vector origin=events[i].position,direction=events[i].direction,velocity=events[i].velocity;
        if(age<0 || age>=effects[events[i].effect].duration) {
            if(events[i].diagnostic) diagnosticRow("end",NULL,events[i].ownerID.shipNumber,events[i].gun,i,
                "clip_expired",age,NULL,NULL,NULL,0,0,0);
            events[i].active=0; continue;
        }
        if(events[i].attached) {
            int usedLiveList;
            Ship *ship=resolveMuzzleOwner(&events[i],&usedLiveList); vector local,world;
            if(!ship || ship->timeCreated!=events[i].ownerCreated || !ship->gunInfo ||
               events[i].gun>=(unsigned int)ship->gunInfo->numGuns) {
                if(events[i].diagnostic) diagnosticRow("end",ship,events[i].ownerID.shipNumber,events[i].gun,i,
                    !ship?"owner_missing_or_hidden":"owner_stamp_or_gun_changed",age,NULL,NULL,NULL,0,0,0);
                events[i].active=0; continue;
            }
            diagnosticOwner=ship;
            if(usedLiveList && logFrame)
                diagnosticRow("owner_fallback",ship,events[i].ownerID.shipNumber,events[i].gun,i,
                    "resolved_from_live_ship_list",age,&ship->posinfo.position,NULL,camera,0,0,0);
            local=events[i].localPosition;
            if(!meshRemasteredGunMuzzle(ship,events[i].gun,&local) && logFrame)
                diagnosticRow("joint_fallback",ship,events[i].ownerID.shipNumber,events[i].gun,i,
                    "native_joint_unavailable",age,&origin,&direction,camera,0,0,0);
            matMultiplyMatByVec(&world,&ship->rotinfo.coordsys,&local);
            vecAdd(origin,world,ship->posinfo.position);
            matMultiplyMatByVec(&direction,&ship->rotinfo.coordsys,&ship->gunInfo->guns[events[i].gun].gunheading);
            if(vecMagnitudeSquared(direction)<.00001f) {
                if(events[i].diagnostic) diagnosticRow("end",ship,events[i].ownerID.shipNumber,events[i].gun,i,
                    "zero_live_heading",age,&origin,&direction,camera,0,0,0);
                events[i].active=0; continue;
            }
            vecNormalize(&direction);
            velocity.x=velocity.y=velocity.z=0; /* no snapshot-motion extrapolation */
        }
        drawEffect(events[i].effect,age,events[i].scale,&origin,&direction,&velocity,0,camera,events[i].attached,
                   logFrame?&submitted:NULL,logFrame?&positive:NULL,logFrame?&maxColour:NULL,&events[i]);
        if(logFrame) {
            diagnosticRow("draw",diagnosticOwner,events[i].ownerID.shipNumber,events[i].gun,i,
                positive?"positive_layers_submitted":"zero_or_delayed_layers",age,&origin,&direction,camera,submitted,positive,maxColour);
            events[i].nextDiagnostic=universe.totaltimeelapsed+.1f;
        }
    }
}
void rmWeaponFxReset(void) {
    rmPresentationReset();
    meshNativeVisualStopPresentation(NULL);
    if(diagnosticFile) { fputs("# transient reset\n",diagnosticFile); fflush(diagnosticFile); }
    memset(events,0,sizeof(events));
}
void rmWeaponFxShutdown(void)
{
    if(uploaded) glDeleteTextures(header->textures,textures);
    if(package) memFree(package);
    package=NULL; header=NULL; attempted=uploaded=0; memset(textures,0,sizeof(textures)); rmWeaponFxReset();
    if(diagnosticFile) fclose(diagnosticFile);
    diagnosticFile=NULL; diagnosticAttempted=0; diagnosticRows=0;
}
