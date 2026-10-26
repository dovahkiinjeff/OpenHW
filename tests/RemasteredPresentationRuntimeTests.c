#include "RemasteredPresentation.h"
#include "RemasteredFxRenderer.h"
#include "NativeVisualRegistry.h"
#include "RemasteredPcmStream.h"
#include "File.h"
#include "Memory.h"
#include "Mesh.h"
#include "Ships.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "mainrgn.h"
#include "soundlow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

Universe universe;
Camera *mrCamera;
bool32 soundinited=TRUE;
bool32 enableSFX=TRUE;
static unsigned fxDraws,fxStarts;static HwRmFxDraw lastFx;
int rmFxRendererStartup(HwRmFxResources *resources){++fxStarts;return resources!=NULL;}
void rmFxRendererShutdown(void){}
void rmFxRendererBegin(Camera *camera){(void)camera;}
void rmFxRendererEnd(void){}
void rmFxRendererDraw(void *user,const HwRmFxDraw *draw){(void)user;lastFx=*draw;++fxDraws;}
static Ship owner;
static int nativeIdentity,nativeAlive=1,loads,prepares;
static unsigned long long token=3;
static MeshNativePresentationVisit eventSink;
static MeshNativeWeaponVisit weaponSink;
static MeshNativePresentationPrepare prepareSink;
static void *sinkUser;
static unsigned char bank[2048];static unsigned bankBytes;
static unsigned char graph[512],resources[37];static unsigned graphBytes;
static int fxEnabled;
static HwRmPresentationEvents *deathEvents;static unsigned snapshotsFreed;
static unsigned hullDraws,hullsFreed;static int hullIdentity;
static unsigned chargeStarts;
void meshRemasteredNotifyWeaponFire(const void *ship,unsigned gun,double clock){(void)ship;(void)gun;(void)clock;++chargeStarts;}
int hwRmVisualInstanceEventGun(const HwRmVisualInstance *instance,unsigned component,const HwRmPresentationEvent *event,unsigned *gun){
    (void)instance;(void)component;if(event->markers&&!strcmp(event->markers,"TestMuzzle")){*gun=0;return 1;}return 0;
}
void *meshRemasteredDeathHullCreate(const void *ship){return ship==&owner?&hullIdentity:NULL;}
void meshRemasteredDeathHullDraw(void *hull,void *instance,const matrix *rotation,const vector *position){(void)instance;(void)rotation;(void)position;if(hull==&hullIdentity)++hullDraws;}
void meshRemasteredDeathHullFree(void *hull){if(hull==&hullIdentity)++hullsFreed;}
int hwRmVisualInstanceSampleSourceClip(HwRmVisualInstance *instance,const char *clip,double phase,char *error,size_t capacity){
    (void)instance;(void)clip;(void)phase;(void)error;(void)capacity;return 0;}
static int damageControlled;static float damageMagnitude;static unsigned damageUpdates;
int hwRmVisualInstancePresentationState(const HwRmVisualInstance *instance,unsigned component,unsigned timeline){
    (void)instance;(void)component;(void)timeline;return damageControlled?(damageMagnitude>0): -1;
}
int hwRmVisualInstanceSetPresentationState(HwRmVisualInstance *instance,unsigned component,const char *state,float magnitude,double clock,HwRmVisualPresentationVisit visit,void *user){
    (void)instance;(void)component;(void)clock;(void)visit;(void)user;
    if(strcmp(state,"Damage"))return 0;damageMagnitude=magnitude;++damageUpdates;return 1;
}
typedef struct FakeSnapshot {HwRmPresentationPlayer *player;} FakeSnapshot;
typedef struct FakeVisitor {HwRmVisualPresentationVisit visit;void *user;} FakeVisitor;
static void visitSnapshot(void *context,const HwRmPresentationEvent *event,double clock){FakeVisitor *visitor=context;visitor->visit(visitor->user,0,event,clock);}
HwRmVisualInstance *hwRmVisualInstanceSnapshot(const HwRmVisualInstance *instance){FakeSnapshot *copy;
    if(instance!=(HwRmVisualInstance*)&nativeIdentity||!deathEvents)return NULL;copy=calloc(1,sizeof(*copy));
    copy->player=hwRmPresentationPlayerCreate(deathEvents);return (HwRmVisualInstance*)copy;}
void hwRmVisualInstanceFree(HwRmVisualInstance *instance){if(instance){FakeSnapshot *copy=(FakeSnapshot*)instance;hwRmPresentationPlayerFree(copy->player);free(copy);++snapshotsFreed;}}
const HwRmPresentationEvents *hwRmVisualInstancePresentation(const HwRmVisualInstance *instance,unsigned component){(void)instance;return component==0?deathEvents:NULL;}
int hwRmVisualInstanceStartPresentation(HwRmVisualInstance *instance,unsigned component,const char *name,double clock,HwRmVisualPresentationVisit visit,void *user){
    FakeVisitor visitor={visit,user};(void)component;return hwRmPresentationPlayerStart(((FakeSnapshot*)instance)->player,name,clock,visitSnapshot,&visitor);}
int hwRmVisualInstanceUpdatePresentation(HwRmVisualInstance *instance,double clock,HwRmVisualPresentationVisit visit,void *user){
    FakeVisitor visitor={visit,user};return hwRmPresentationPlayerUpdate(((FakeSnapshot*)instance)->player,clock,visitSnapshot,&visitor);}
void hwRmVisualInstanceStopPresentation(HwRmVisualInstance *instance){hwRmPresentationPlayerStop(((FakeSnapshot*)instance)->player);}
static HwRmWeaponPresentation *nativeWeapon;
HwRmSubsystemInstance *hwRmVisualInstanceSubsystem(HwRmVisualInstance *instance){(void)instance;return NULL;}
int hwRmSubsystemJointTransform(HwRmSubsystemInstance *instance,unsigned component,const char *joint,float scale,HwRmSubsystemPoseCallback pose,void *user,float *matrix,char *error,size_t capacity){
    (void)instance;(void)component;(void)joint;(void)scale;(void)pose;(void)user;(void)matrix;(void)error;(void)capacity;return 0;}
const HwRmWeaponPresentation *hwRmVisualInstanceWeaponPresentation(const HwRmVisualInstance *instance,unsigned gun){return instance==(HwRmVisualInstance*)&nativeIdentity&&gun==0?nativeWeapon:NULL;}
static const unsigned char wav[]={
    'R','I','F','F',52,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
    1,0,2,0,68,172,0,0,16,177,2,0,4,0,16,0,'d','a','t','a',16,0,0,0,
    16,39,240,216,16,39,240,216,16,39,240,216,16,39,240,216
};
static void integer(unsigned value){unsigned i;for(i=0;i<4;++i)bank[bankBytes++]=(unsigned char)(value>>(8*i));}
static void number(float value){unsigned bits;memcpy(&bits,&value,4);integer(bits);}
static void makeBank(void){
    unsigned i,p;static const char strings[]="\0shot\0remasteredaudio/sfx/etg/shot.fda";
    memcpy(bank,"OWSFX001",8);bankBytes=8;
    integer(2);integer(1);integer(1);integer(2);integer(sizeof(strings));
    integer(0);integer(0);integer(~0u);integer(0);integer(0);integer(1);integer(0);
    integer(1);integer(6);integer(0);integer(~0u);integer(1);integer(0);integer(1);
    integer(1);
    integer((1u<<2)|(1u<<9)|(1u<<19));integer(~0u);integer(0);integer(2);integer(0);
    for(i=0;i<30;++i)number(i==2?.5f:i==9?3.f:0.f);for(i=0;i<28;++i)number(0);
    for(p=0;p<2;++p){number((float)p*100);number(1.f-p);number(0);number(0);for(i=0;i<8;++i)number(1);}
    memcpy(bank+bankBytes,strings,sizeof(strings));bankBytes+=sizeof(strings);
}
static int isBank(const char *name){return !strcmp(name,"RemasteredAudio\\sfx\\effects.sfxbank");}
static int isGraph(const char *name){return fxEnabled&&!strcmp(name,"RemasteredFX\\Native\\effects.fxbank");}
static int isResources(const char *name){return fxEnabled&&!strcmp(name,"RemasteredFX\\Native\\effects.fxresources");}
bool32 fileExists(char *name,udword flags){(void)flags;return isGraph(name)||isResources(name)||isBank(name)||!strcmp(name,"remasteredaudio/sfx/etg/shot.fda");}
sdword fileSizeGet(char *name,udword flags){(void)flags;return isGraph(name)?graphBytes:isResources(name)?sizeof(resources):isBank(name)?(sdword)bankBytes:fileExists(name,0)?sizeof(wav):-1;}
sdword fileLoadAlloc(char *name,void **bytes,udword flags){sdword size=fileSizeGet(name,flags);*bytes=NULL;if(size<=0)return -1;*bytes=malloc(size);if(!*bytes)return -1;memcpy(*bytes,isGraph(name)?graph:isResources(name)?resources:isBank(name)?bank:wav,size);++loads;return size;}
static void makeFx(void){static const char strings[]="\0flash\0duration\0radius";
    memcpy(bank,"OWFX0001",8);bankBytes=8;integer(1);integer(2);integer(2);integer(sizeof(strings));
    integer(1);integer(HW_RM_FX_RING);integer(0);integer(2);
    integer(7);integer(HW_RM_FX_FLOAT);integer(0);integer(0);integer(1);
    integer(16);integer(HW_RM_FX_FLOAT);integer(0);integer(1);integer(1);
    number(2);number(3);memcpy(bank+bankBytes,strings,sizeof(strings));bankBytes+=sizeof(strings);
    memcpy(graph,bank,bankBytes);graphBytes=bankBytes;
    memset(resources,0,sizeof(resources));memcpy(resources,"OWFXRS01",8);resources[32]=1;
    makeBank();
}
static void makeDeathEvents(void){char error[256];unsigned e;static const char strings[]="\0Death\0flash";
    memcpy(bank,"OWEVENT2",8);bankBytes=8;integer(1);integer(2);integer(sizeof(strings));
    integer(1);integer(~0u);number(1);number(0);number(0);integer(0);
    for(e=0;e<2;++e){integer(0);number(e*.5f);number(1);integer(0);integer(0);integer(7);integer(0);integer(0);
        integer(0);number(1);integer(0);integer(0);integer(0);integer(0);}
    memcpy(bank+bankBytes,strings,sizeof(strings));bankBytes+=sizeof(strings);
    deathEvents=hwRmPresentationEventsRead(bank,bankBytes,error,sizeof(error));
    if(!deathEvents){fprintf(stderr,"%s\n",error);exit(1);}
}
static void makeDeathTrailEvents(void){char error[256];unsigned e;static const char strings[]="\0Death\0Deathtrail\0flash";
    memcpy(bank,"OWEVENT2",8);bankBytes=8;integer(2);integer(2);integer(sizeof(strings));
    integer(1);integer(~0u);number(1);number(0);number(0);integer(0);
    integer(7);integer(~0u);number(3);number(0);number(0);integer(0);
    for(e=0;e<2;++e){integer(e);number(e?0.5f:0);number(1);integer(0);integer(0);integer(18);integer(0);integer(0);
        integer(0);number(1);integer(0);integer(0);integer(0);integer(0);}
    memcpy(bank+bankBytes,strings,sizeof(strings));bankBytes+=sizeof(strings);
    deathEvents=hwRmPresentationEventsRead(bank,bankBytes,error,sizeof(error));
    if(!deathEvents){fprintf(stderr,"%s\n",error);exit(1);}
}
void memFree(void *value){free(value);}
sdword SNDchannel(sdword handle){return handle<0?-1:(handle&255);}
float soundBusGain(int bus){return bus==HW_AUDIO_SFX?1.f:0.f;}
Ship *ShipIDtoShip(ShipID id,bool32 inside){(void)inside;return nativeAlive&&id.shipNumber==owner.shipID.shipNumber?&owner:NULL;}
void meshNativeVisualSetPresentationSink(MeshNativePresentationVisit event,MeshNativeWeaponVisit weapon,void *user){eventSink=event;weaponSink=weapon;sinkUser=user;prepareSink=NULL;}
void meshNativeVisualSetPresentationPrepare(MeshNativePresentationPrepare prepare){prepareSink=prepare;}
unsigned long long meshNativeVisualInstanceToken(const HwRmVisualInstance *instance){return instance==(HwRmVisualInstance*)&nativeIdentity?token:0;}
HwRmVisualInstance *meshNativeVisualFindInstance(const void *ship,unsigned long long expected,float *scale){if(!nativeAlive||ship!=&owner||expected!=token)return NULL;*scale=1;return (HwRmVisualInstance*)&nativeIdentity;}
bool32 meshRemasteredSynchronizeInstance(const void *ship,void *instance){++prepares;return ship==&owner&&instance==&nativeIdentity;}
bool32 meshRemasteredGunHasSound(const void *ship,unsigned gun){return ship==&owner&&gun==0;}
void *meshRemasteredInstance(const void *ship,float *scale){if(ship!=&owner||!nativeAlive)return NULL;*scale=1;return &nativeIdentity;}
int hwRmVisualInstanceGunHasFx(const HwRmVisualInstance *instance,unsigned gun,HwRmVisualFxAvailable available,void *user){return instance==(HwRmVisualInstance*)&nativeIdentity&&gun==0&&available(user,"flash");}
static int marker(float *matrix){unsigned i;memset(matrix,0,sizeof(float)*16);for(i=0;i<4;++i)matrix[i*5]=1;return 1;}
int hwRmVisualInstanceGunMuzzle(HwRmVisualInstance *instance,unsigned gun,float scale,float *matrix,char *error,size_t capacity){(void)instance;(void)gun;(void)scale;(void)error;(void)capacity;return marker(matrix);}
int hwRmVisualInstancePresentationMarker(HwRmVisualInstance *instance,unsigned component,const HwRmPresentationEvent *event,unsigned random,float scale,float *matrix,char *error,size_t capacity){(void)instance;(void)component;(void)event;(void)random;(void)scale;(void)error;(void)capacity;return marker(matrix);}
void matMultiplyMatByVec(vector *out,matrix *m,vector *in){out->x=m->m11*in->x+m->m12*in->y+m->m13*in->z;out->y=m->m21*in->x+m->m22*in->y+m->m23*in->z;out->z=m->m31*in->x+m->m32*in->y+m->m33*in->z;}
#define CHECK(x) do{if(!(x)){printf("failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
    Camera camera;short output[2];HwRmPresentationEvent event={0};HwRmWeaponPresentationSlot weapon={0};HwRmVisualInstance *instance=(HwRmVisualInstance*)&nativeIdentity;
    memset(&camera,0,sizeof(camera));camera.lookatpoint.y=1;camera.upvector.z=1;mrCamera=&camera;
    memset(&owner,0,sizeof(owner));owner.shipID.shipNumber=7;owner.timeCreated=1;owner.posinfo.position.x=50;
    owner.rotinfo.coordsys.m11=owner.rotinfo.coordsys.m22=owner.rotinfo.coordsys.m33=1;
    makeBank();CHECK(!rmPresentationOwnsGunSound(&owner,0));rmPresentationStartup();
    CHECK(eventSink&&weaponSink&&prepareSink);CHECK(loads==2);CHECK(rmPresentationOwnsGunSound(&owner,0));
    rmPresentationUpdate();event.markers="Root";event.sound="shot";
    eventSink(sinkUser,&owner,instance,1,0,&event,0);
    memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));CHECK(output[0]==0&&output[1]==-2500);
    owner.posinfo.position.x=-25;rmPresentationUpdate();
    memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));CHECK(output[0]==3750&&output[1]==0);
    ++token;rmPresentationUpdate(); /* same Ship address, replaced native instance */
    memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));CHECK(!output[0]&&!output[1]);
    weapon.sound="shot";weaponSink(sinkUser,&owner,instance,1,0,HW_RM_WEAPON_FIRE,&weapon,0);
    memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));CHECK(output[0]==3750&&output[1]==0);
    CHECK(loads==2&&prepares>0);rmPresentationReset();
    memset(output,0,sizeof(output));rmPcmStreamMix(output,sizeof(output));CHECK(!output[0]&&!output[1]);
    rmPresentationShutdown();CHECK(!eventSink&&!weaponSink);CHECK(!rmPresentationOwnsGunSound(&owner,0));
    /* Actual Game consumer -> actual graph -> actual scene, with no sound
       bank enabled. Visibility and moving muzzle attachment cannot depend on
       audio settings, a render-list visit, or subsequent file reads. */
    makeFx();fxEnabled=1;enableSFX=FALSE;universe.totaltimeelapsed=10;
    rmPresentationStartup();CHECK(fxStarts==1&&eventSink&&weaponSink);
    CHECK(rmPresentationOwnsGunFx(&owner,0)&&!rmPresentationOwnsGunFx(&owner,1));
    weapon.sound="";weapon.fx="flash";weaponSink(sinkUser,&owner,instance,2,0,HW_RM_WEAPON_FIRE,&weapon,10);
    rmPresentationDraw(&camera);CHECK(fxDraws==1&&lastFx.age==0&&lastFx.scale==2);
    CHECK(lastFx.matrix[12]==-25);
    {int before=loads;owner.posinfo.position.x=120;universe.totaltimeelapsed=10.5f;
        rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==2&&lastFx.matrix[12]==120&&lastFx.age==.5f);
        ++token;rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==2&&loads==before);}
    event.fx="flash";event.sound="";event.scale=1;event.orphan=1;
    eventSink(sinkUser,&owner,instance,1,0,&event,10.5);
    rmPresentationDraw(&camera);CHECK(fxDraws==3&&lastFx.matrix[12]==120);
    owner.posinfo.position.x=300;universe.totaltimeelapsed=11;rmPresentationUpdate();rmPresentationDraw(&camera);
    CHECK(fxDraws==4&&lastFx.matrix[12]==120); /* orphan presence, not text truth */
    rmPresentationReset();rmPresentationDraw(&camera);CHECK(fxDraws==4);
    {char error[256];const char source[]="hitfx=\"flash\"\nmissfx=\"flash\"\nhit_clamp={.5,1.25}";int projectile=1;float position[3]={5,6,7},heading[3]={0,0,1},velocity[3]={0};
        nativeWeapon=hwRmWeaponPresentationRead(source,sizeof(source)-1,error,sizeof(error));CHECK(nativeWeapon);
        rmPresentationCaptureProjectile(&projectile,&owner,0);
        hwRmWeaponPresentationFree(nativeWeapon);nativeWeapon=NULL;nativeAlive=0;
        CHECK(rmPresentationProjectileImpact(&projectile,position,heading,velocity,55));
        rmPresentationDraw(&camera);CHECK(fxDraws==5&&lastFx.matrix[12]==5&&lastFx.matrix[13]==6&&lastFx.matrix[14]==7&&lastFx.scale==1.25f);
        rmPresentationReleaseProjectile(&projectile,position,heading,velocity);
        CHECK(!rmPresentationProjectileImpact(&projectile,position,heading,velocity,55));
        /* Pointer reuse cannot inherit the destroyed projectile's bindings. */
        nativeAlive=1;nativeWeapon=hwRmWeaponPresentationRead(source,sizeof(source)-1,error,sizeof(error));CHECK(nativeWeapon);
        rmPresentationCaptureProjectile(&projectile,&owner,0);hwRmWeaponPresentationFree(nativeWeapon);nativeWeapon=NULL;
        rmPresentationReleaseProjectile(&projectile,position,heading,velocity);rmPresentationDraw(&camera);CHECK(fxDraws==7);
    }
    rmPresentationReset();fxDraws=0;
    {char error[256];const char source[]="bulletfx=\"flash\"";Bullet projectile={0};
        projectile.rotinfo.coordsys.m11=projectile.rotinfo.coordsys.m22=projectile.rotinfo.coordsys.m33=1;
        projectile.posinfo.position.x=42;nativeWeapon=hwRmWeaponPresentationRead(source,sizeof(source)-1,error,sizeof(error));CHECK(nativeWeapon);
        rmPresentationCaptureProjectile(&projectile,&owner,0);rmPresentationTrackProjectile(&projectile);
        hwRmWeaponPresentationFree(nativeWeapon);nativeWeapon=NULL;nativeAlive=0;
        CHECK(rmPresentationOwnsProjectileFx(&projectile));rmPresentationDraw(&camera);CHECK(fxDraws==1&&lastFx.matrix[12]==42);
        projectile.posinfo.position.x=84;rmPresentationTrackProjectile(&projectile);universe.totaltimeelapsed=11.5f;
        rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==2&&lastFx.matrix[12]==84);
        rmPresentationReleaseProjectile(&projectile,(const float*)&projectile.posinfo.position,(const float*)&projectile.rotinfo.coordsys.m13,(const float*)&projectile.posinfo.velocity);
        CHECK(!rmPresentationOwnsProjectileFx(&projectile));rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==2);
    }
    rmPresentationReset();fxDraws=0;nativeAlive=1;
    {GunInfo guns={0};GunStatic stats={0};float savedTime=universe.totaltimeelapsed;
        guns.numGuns=1;guns.guns[0].gunstatic=&stats;guns.guns[0].gunheading.x=1;owner.gunInfo=&guns;
        weapon.fx="flash";weapon.sound="";
        weaponSink(sinkUser,&owner,instance,1,0,HW_RM_WEAPON_FIRE,&weapon,universe.totaltimeelapsed);
        rmPresentationDraw(&camera);CHECK(lastFx.matrix[8]==1&&lastFx.matrix[9]==0&&lastFx.matrix[10]==0);
        CHECK(lastFx.matrix[12]==owner.posinfo.position.x);
        guns.guns[0].gunheading.x=0;guns.guns[0].gunheading.y=1;
        universe.totaltimeelapsed+=0.1f;rmPresentationUpdate();
        rmPresentationDraw(&camera);CHECK(lastFx.matrix[8]==0&&lastFx.matrix[9]==1&&lastFx.matrix[10]==0);
        CHECK(rmPresentationPrepareIonShot(&owner,0,&owner)==1);
        stats.gunsoundtype=GS_LargeIonCannon;universe.totaltimeelapsed=100;
        CHECK(!rmPresentationPrepareIonShot(&owner,0,&owner)&&chargeStarts==1);
        universe.totaltimeelapsed=101;CHECK(!rmPresentationPrepareIonShot(&owner,0,&owner)&&chargeStarts==1);
        universe.totaltimeelapsed=102;CHECK(rmPresentationPrepareIonShot(&owner,0,&owner)==2&&chargeStarts==1);
        universe.totaltimeelapsed=110;CHECK(!rmPresentationPrepareIonShot(&owner,0,&owner)&&chargeStarts==2);
        universe.totaltimeelapsed=113;CHECK(!rmPresentationPrepareIonShot(&owner,0,&owner)&&chargeStarts==3);
        owner.gunInfo=NULL;universe.totaltimeelapsed=savedTime;rmPresentationReset();fxDraws=0;
    }
    /* A native weapon owns even empty or unavailable visual slots. The
       caller must not substitute a Classic impact when nothing was spawned. */
    {char error[256];const char source[]="hitfx=\"not_in_bank\"";int projectile=1;
        float position[3]={0},heading[3]={0,0,1},velocity[3]={0};
        nativeWeapon=hwRmWeaponPresentationRead(source,sizeof(source)-1,error,sizeof(error));CHECK(nativeWeapon);
        rmPresentationCaptureProjectile(&projectile,&owner,0);
        hwRmWeaponPresentationFree(nativeWeapon);nativeWeapon=NULL;
        CHECK(rmPresentationOwnsProjectileFx(&projectile));
        CHECK(rmPresentationProjectileImpact(&projectile,position,heading,velocity,1));
        CHECK(rmPresentationProjectileImpact(&projectile,position,heading,velocity,0));
        rmPresentationDraw(&camera);CHECK(fxDraws==0);
        rmPresentationReleaseProjectile(&projectile,position,heading,velocity);
        CHECK(!rmPresentationOwnsProjectileFx(&projectile));
    }
    {char error[256];const char source[]="bulletfx=\"flash\"\nhitfx=\"flash\"";Bullet beam={0};float position[3]={100,0,0},heading[3]={0,0,1},velocity[3]={0};
        beam.objtype=OBJ_BulletType;beam.bulletType=BULLET_Beam;beam.beamtraveldist=100;
        beam.rotinfo.coordsys.m11=beam.rotinfo.coordsys.m22=beam.rotinfo.coordsys.m33=1;
        nativeWeapon=hwRmWeaponPresentationRead(source,sizeof(source)-1,error,sizeof(error));CHECK(nativeWeapon);
        rmPresentationCaptureProjectile(&beam,&owner,0);rmPresentationTrackProjectile(&beam);
        hwRmWeaponPresentationFree(nativeWeapon);nativeWeapon=NULL;
        rmPresentationDraw(&camera);CHECK(fxDraws==1&&lastFx.length==100);
        beam.beamtraveldist=25;rmPresentationTrackProjectile(&beam);rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==2&&lastFx.length==25);
        CHECK(rmPresentationProjectileImpact(&beam,position,heading,velocity,1));rmPresentationDraw(&camera);CHECK(fxDraws==4&&lastFx.matrix[12]==100);
        position[0]=200;CHECK(rmPresentationProjectileImpact(&beam,position,heading,velocity,1));
        rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==6&&lastFx.matrix[12]==200);
        rmPresentationReleaseProjectile(&beam,position,heading,velocity);rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==6);
    }
    rmPresentationReset();fxDraws=0;nativeAlive=1;makeDeathEvents();universe.totaltimeelapsed=20;
    owner.posinfo.position.x=100;owner.posinfo.velocity.x=10;
    CHECK(rmPresentationShipDeath(&owner));nativeAlive=0;owner.posinfo.position.x=9999;
    universe.totaltimeelapsed=20.25f;rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==1&&lastFx.matrix[12]==102.5f&&hullDraws==1);
    universe.totaltimeelapsed=20.5f;rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==3&&lastFx.matrix[12]==105);
    universe.totaltimeelapsed=21.5f;rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==5&&lastFx.matrix[12]==115&&snapshotsFreed==0&&hullDraws==2&&hullsFreed==1);
    universe.totaltimeelapsed=22.6f;rmPresentationUpdate();universe.totaltimeelapsed=22.7f;rmPresentationUpdate();CHECK(snapshotsFreed==1);
    rmPresentationReset();CHECK(snapshotsFreed==1);hwRmPresentationEventsFree(deathEvents);deathEvents=NULL;
    /* State-owned attachments are cancelled when their owning state exits.
       Actual threshold/re-entry logic is covered by native HOD tests. */
    {HwRmPresentationEvent damageEvent={0};
        damageControlled=1;damageMagnitude=.5f;nativeAlive=1;fxDraws=0;
        damageEvent.fx="flash";damageEvent.scale=1;
        eventSink(sinkUser,&owner,instance,1,0,&damageEvent,universe.totaltimeelapsed);
        rmPresentationDraw(&camera);CHECK(fxDraws==1);
        damageMagnitude=0;rmPresentationUpdate();rmPresentationDraw(&camera);CHECK(fxDraws==1);
        damageControlled=0;
    }
    rmPresentationReset();makeDeathTrailEvents();nativeAlive=1;universe.totaltimeelapsed=30;
    {unsigned before=hullsFreed;
        CHECK(rmPresentationShipDeath(&owner));nativeAlive=0;
        universe.totaltimeelapsed=31.1f;rmPresentationUpdate();CHECK(hullsFreed==before+1);
        universe.totaltimeelapsed=31.5f;rmPresentationUpdate();rmPresentationDraw(&camera);
        CHECK(lastFx.age>=0&&hullsFreed==before+1);
        universe.totaltimeelapsed=34;rmPresentationUpdate();rmPresentationUpdate();
    }
    hwRmPresentationEventsFree(deathEvents);deathEvents=NULL;
    rmPresentationShutdown();
    rmPcmStreamReset();puts("Native game events to PCM: moving anchors, reload tokens, resets and precache-only I/O passed");return 0;
}
