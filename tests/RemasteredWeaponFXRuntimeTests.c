#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "RemasteredWeaponFX.h"
#include "RemasteredWeaponFXData.h"
#include "NativeVisualRegistry.h"
#include "File.h"
#include "Memory.h"
#include "Mesh.h"
#include "ETG.h"
#include "Universe.h"
#include "SinglePlayer.h"
#include "Clouds.h"
#include "glinc.h"
#include "NativeGunPosePolicy.h"
#include "ModernGraphics.h"
static unsigned engineLights,deathLights;
static unsigned presentationStops;
void rmPresentationReset(void) {}
void rmPresentationDraw(Camera *camera) {(void)camera;}
bool32 rmPresentationOwnsGunFx(const void *ship,unsigned gun){(void)ship;(void)gun;return FALSE;}
void meshNativeVisualStopPresentation(const void *ship) { assert(!ship);++presentationStops; }
void hwModernGraphicsSubmitDynamicLight(const HWModernDynamicLightEmitter *light) {
    assert(light->radius>0 && isfinite(light->intensity));
    if(light->source==HW_MODERN_LIGHT_ENGINE)++engineLights;
}
void hwModernGraphicsTriggerDynamicLight(const HWModernDynamicLightEmitter *light,float duration) {
    assert(light->radius>0 && duration>0 && isfinite(light->intensity));
    if(light->source==HW_MODERN_LIGHT_EXPLOSION)++deathLights;
}
Universe universe;
bool32 singlePlayerGame=FALSE;
MissionEnum spGetCurrentMission(void){return (MissionEnum)0;}
ubyte UNIVERSE_UPDATE_RATE_SHIFT=2;
bool32 etgFireEffectsEnabled=TRUE,etgBulletEffectsEnabled=TRUE,etgHitEffectsEnabled=TRUE;
#if ETG_DISABLEABLE
bool32 etgEffectsEnabled=TRUE;
#endif
static const char *path;
static unsigned vertices,uploads,deleted,pushes;
static int nativeHull=1;
static Ship *liveShip;
static vector muzzleLocal;
static double sumX;
static vector resolvedHeading;
static vector *liveHeading;
static unsigned fogDisabled,scissorDisabled,depthDisabled;
Ship *ShipIDtoShip(ShipID id,bool32 inside) { return liveShip && liveShip->shipID.shipNumber==id.shipNumber ? liveShip : NULL; }
bool32 fileExists(char *name,udword flags) { return TRUE; }
sdword fileSizeGet(char *name,udword flags) {
    FILE *f=fopen(path,"rb"); long size; assert(f); fseek(f,0,SEEK_END); size=ftell(f); fclose(f); return (sdword)size;
}
sdword fileLoadAlloc(char *name,void **data,udword flags) {
    sdword size=fileSizeGet(name,flags); FILE *f=fopen(path,"rb");
    *data=malloc(size); assert(*data && f); assert(fread(*data,1,size,f)==size); fclose(f); return size;
}
void memFree(void *p) { free(p); }
char *ShipTypeToStr(ShipType type) {
    return type==P1StandardCorvette?"P1StandardCorvette":
        type==P1MissileCorvette?"P1MissileCorvette":"HeavyInterceptor";
}
int meshNativeVisualParameters(const void *mesh,float *scale,unsigned int *lod) { *scale=10; *lod=0; return nativeHull; }
bool32 meshRemasteredGunMuzzle(const void *ship,unsigned int gun,vector *out) { *out=muzzleLocal; return TRUE; }
void matMultiplyMatByVec(vector *out,matrix *m,vector *in) {
    out->x=m->m11*in->x+m->m12*in->y+m->m13*in->z;
    out->y=m->m21*in->x+m->m22*in->y+m->m23*in->z;
    out->z=m->m31*in->x+m->m32*in->y+m->m33*in->z;
    if(in==liveHeading) resolvedHeading=*out;
}
void matMultiplyVecByMat(vector *out,vector *in,matrix *m) {
    out->x=m->m11*in->x+m->m21*in->y+m->m31*in->z;
    out->y=m->m12*in->x+m->m22*in->y+m->m32*in->z;
    out->z=m->m13*in->x+m->m23*in->y+m->m33*in->z;
}
void vecNormalize(vector *v) { float s=sqrtf(v->x*v->x+v->y*v->y+v->z*v->z); assert(s>0); v->x/=s;v->y/=s;v->z/=s; }
void hwglPushAttrib(GLbitfield mask) { ++pushes; }
void hwglPopAttrib(void) { assert(pushes); --pushes; }
void hwglGenTextures(GLsizei n,GLuint *t) { int i; for(i=0;i<n;i++) t[i]=i+1; }
void hwglDeleteTextures(GLsizei n,const GLuint *t) { deleted+=n; }
void hwglBindTexture(GLenum target,GLuint texture) { assert(texture); }
void hwglTexParameteri(GLenum a,GLenum b,GLint c) {}
void hwglTexImage2D(GLenum target,GLint level,GLint format,GLsizei w,GLsizei h,GLint border,GLenum pixelsFormat,GLenum type,const GLvoid *p) { assert(w>0 && h>0 && p); ++uploads; }
void hwglTexEnvi(GLenum a,GLenum b,GLint c) {}
void hwglEnable(GLenum cap) {}
void hwglDisable(GLenum cap) {
    if(cap==GL_FOG) ++fogDisabled;
    if(cap==GL_SCISSOR_TEST) ++scissorDisabled;
    if(cap==GL_DEPTH_TEST) ++depthDisabled;
}
int hwDiagnosticsDisabled(void) { return 0; }
void hwglDepthFunc(GLenum func) { assert(func==GL_LEQUAL); }
void hwglDepthMask(GLboolean mask) { assert(mask==GL_FALSE); }
void hwglBlendFunc(GLenum a,GLenum b) {}
void hwglColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a) { assert(isfinite(r+g+b+a)); }
void hwglBegin(GLenum mode) { assert(mode==GL_TRIANGLE_STRIP || mode==GL_TRIANGLES); }
void hwglEnd(void) {}
void hwglTexCoord2f(GLfloat u,GLfloat v) { assert(isfinite(u+v)); }
void hwglVertex3f(GLfloat x,GLfloat y,GLfloat z) { assert(isfinite(x+y+z)); ++vertices; sumX+=x; }
int main(int argc,char **argv)
{
    Ship ship={0}; ShipStaticInfo info={0}; lodinfo lods={0}; GunInfo guninfo={0}; Gun *gun=&guninfo.guns[0]; GunStatic gunstatic={0};
    Bullet bullet={0}; Camera camera={0}; vector position={0,0,0},direction={0,0,1},velocity={0,0,0}; unsigned before;
    assert(argc==2 || argc==3); path=argv[1];
    rmWeaponFxDiagnosticsEnabled=argc==3 && strcmp(argv[2],"--raiders");
    ship.shiprace=R1; ship.shiptype=HeavyInterceptor; ship.staticinfo=&info;
    info.staticheader.LOD=&lods; lods.nLevels=1; lods.level[0].flags=LT_Mesh; lods.level[0].pData=&info;
    gun->gunstatic=&gunstatic; gun->gunheading.z=1; gunstatic.gunindex=0;
    {
        float azimuth,declination;
        GunType types[]={GUN_Fixed,GUN_Gimble,GUN_MissileLauncher,GUN_MineLauncher};
        unsigned i;
        /* Captured saved/new fixed guns contain enormous unused angles.
           Also cover NaNs: neither may poison a native muzzle query. */
        gun->angle=NAN; gun->declination=-1.696377554920081e30f;
        for(i=0;i<sizeof(types)/sizeof(types[0]);i++) {
            gunstatic.guntype=types[i];
            nativeGunMechanicalAngles(gun,&azimuth,&declination);
            assert(azimuth==0 && declination==0);
        }
        gunstatic.guntype=GUN_NewGimble; gun->angle=.3f; gun->declination=-.2f;
        nativeGunMechanicalAngles(gun,&azimuth,&declination);
        assert(fabs(azimuth-.3f)<.0001f && fabs(declination+.2f)<.0001f);
        {
            float heading[3];
            gun->curgunorientation.m13=.6f;gun->curgunorientation.m33=.8f;
            nativeGunTrackingHeading(gun,heading);
            assert(fabs(heading[0]-.6f)<.0001f && fabs(heading[2]-.8f)<.0001f);
            /* Predicted projectile heading stays untouched. */
            assert(gun->gunheading.x==0 && gun->gunheading.z==1);
            gunstatic.guntype=GUN_Fixed;nativeGunTrackingHeading(gun,heading);
            assert(heading[0]==0 && heading[2]==1);
        }
        gunstatic.guntype=GUN_Fixed; gun->angle=gun->declination=0;
    }
    guninfo.numGuns=1; ship.gunInfo=&guninfo; liveShip=&ship;
    liveHeading=&gun->gunheading;
    ship.shipID.shipNumber=42; ship.timeCreated=1;
    ship.shiplink.structptr=&ship;
    universe.ShipList.head=universe.ShipList.tail=&ship.shiplink;
    universe.ShipList.num=1;
    ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m22=ship.rotinfo.coordsys.m33=1;
    camera.eyeposition.z=-100; camera.upvector.y=1;
    bullet.owner=&ship; bullet.gunowner=gun; bullet.bulletheading=direction; bullet.timelived=.15f;
    assert(rmWeaponFxProjectileAvailable(&ship,gun));
    rmWeaponFxDiagnosticShot(&ship,gun,TRUE);
    rmWeaponFxDiagnosticLight(&ship,gun,&position,&direction,120.f,3840.f);
    rmWeaponFxDiagnosticBinding(&ship,0,"test_binding_failure,CSV_safe");
    ship.shiprace=R2;
    if(argc==3 && !strcmp(argv[2],"--system"))assert(rmWeaponFxProjectileAvailable(&ship,gun));
    else assert(!rmWeaponFxProjectileAvailable(&ship,gun));
    ship.shiprace=P3;assert(!rmWeaponFxProjectileAvailable(&ship,gun));ship.shiprace=R1;
    nativeHull=0; assert(!rmWeaponFxProjectileAvailable(&ship,gun)); nativeHull=1;
    assert(rmWeaponFxDrawBullet(&bullet,&camera)); assert(vertices>0 && uploads>0 && pushes==0);
    assert(fogDisabled && scissorDisabled && !depthDisabled);
    assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
    universe.totaltimeelapsed=.1f; before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices>before && pushes==0);
    assert(depthDisabled); /* authored muzzle glow is not clipped by its own gun */
    {
        double center;
        vertices=0; sumX=0; rmWeaponFxDrawTransient(&camera); assert(vertices); center=sumX/vertices;
        /* Reproduce the capture: a live firing owner missing from the legacy
           ID table must still render, and still follow the moving gun. */
        liveShip=NULL;
        /* Same clip age: a moved ship and changed velocity must translate the
           flash exactly once, with no inherited-velocity drift. */
        ship.posinfo.position.x=123; ship.posinfo.velocity.x=9000;
        vertices=0; sumX=0; rmWeaponFxDrawTransient(&camera);
        assert(vertices && fabs(sumX/vertices-center-123)<.001);
        muzzleLocal.x=7;
        vertices=0; sumX=0; rmWeaponFxDrawTransient(&camera);
        assert(vertices && fabs(sumX/vertices-center-130)<.001);
        /* Ship rotation and turret aiming are sampled live, not frozen at fire. */
        ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m33=0;
        ship.rotinfo.coordsys.m13=1; ship.rotinfo.coordsys.m31=-1;
        rmWeaponFxDrawTransient(&camera);
        assert(fabs(resolvedHeading.x-1)<.001 && fabs(resolvedHeading.z)<.001);
        gun->gunheading.x=1; gun->gunheading.z=0;
        rmWeaponFxDrawTransient(&camera);
        assert(fabs(resolvedHeading.x)<.001 && fabs(resolvedHeading.z+1)<.001);
        liveShip=&ship;
        /* ID reuse is rejected even if the numeric ID matches. */
        ship.timeCreated=2; vertices=0; rmWeaponFxDrawTransient(&camera); assert(!vertices);
        ship.timeCreated=1;
        assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
        universe.ShipList.head=universe.ShipList.tail=NULL; universe.ShipList.num=0;
        liveShip=NULL; before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices==before); liveShip=&ship;
        universe.ShipList.head=universe.ShipList.tail=&ship.shiplink; universe.ShipList.num=1;
        /* A list fallback cannot resurrect hidden, dead or reused owners. */
        assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
        ship.flags|=SOF_Hide; before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices==before);
        ship.flags&=~SOF_Hide;
        assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
        ship.flags|=SOF_Dead; before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices==before);
        ship.flags&=~SOF_Dead;
        assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
        liveShip=NULL; ship.timeCreated=3;
        before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices==before);
        ship.timeCreated=1; liveShip=&ship;
    }
    assert(rmWeaponFxImpact(&bullet,&position,&velocity));
    before=presentationStops;rmWeaponFxReset();assert(presentationStops==before+1);
    before=vertices; rmWeaponFxDrawTransient(&camera); assert(vertices==before);
    /* A beam's charge clip runs through its full curve during the Classic
       firing window, then expires. Non-beam clips do not use this clock. */
    gunstatic.bulletType=BULLET_Beam;gunstatic.bulletlifetime=.5f;
    universe.totaltimeelapsed=0;
    assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
    universe.totaltimeelapsed=.1f;vertices=0;rmWeaponFxDrawTransient(&camera);assert(vertices);
    universe.totaltimeelapsed=.51f;vertices=0;rmWeaponFxDrawTransient(&camera);assert(!vertices);
    gunstatic.bulletType=BULLET_Projectile;rmWeaponFxReset();
    bullet.owner=NULL; assert(!rmWeaponFxDrawBullet(&bullet,&camera));
    rmWeaponFxShutdown(); assert(deleted==uploads);
    /* A fresh reload must not retain stale texture handles or events. */
    assert(rmWeaponFxProjectileAvailable(&ship,gun)); rmWeaponFxShutdown();
    if(argc==3 && (!strcmp(argv[2],"--raiders") || !strcmp(argv[2],"--system"))) {
        unsigned type,lod,index;
        ship.shiprace=P1;ship.timeCreated=1;
        ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m22=ship.rotinfo.coordsys.m33=1;
        ship.rotinfo.coordsys.m13=ship.rotinfo.coordsys.m31=0;
        gun->gunheading.x=0;gun->gunheading.z=1;muzzleLocal.x=0;
        for(type=0;type<2;++type) {
            ship.shiptype=type?P1MissileCorvette:P1StandardCorvette;
            for(index=0;index<2;++index) for(lod=0;lod<5;++lod) {
                double center;
                rmWeaponFxReset();ship.currentLOD=(ubyte)lod;gunstatic.gunindex=index;
                ship.posinfo.position.x=0;ship.posinfo.velocity.x=20;
                universe.totaltimeelapsed=0;
                assert(rmWeaponFxProjectileAvailable(&ship,gun));
                assert(rmWeaponFxMuzzle(&ship,gun,&position,&direction));
                universe.totaltimeelapsed=.25f;vertices=0;sumX=0;
                rmWeaponFxDrawTransient(&camera);assert(vertices);center=sumX/vertices;
                ship.posinfo.position.x=123;ship.posinfo.velocity.x=9000;
                vertices=0;sumX=0;rmWeaponFxDrawTransient(&camera);assert(vertices);
                /* Steam stays in its emission frame; gun flashes stay attached.
                   Camera LOD may never switch either launcher back to ETG. */
                assert(fabs(sumX/vertices-center-(type?0:123))<.001);
                if(type) {
                    Missile missile={0};
                    missile.owner=&ship;missile.rotinfo.coordsys.m33=1;
                    rmWeaponFxReset();universe.totaltimeelapsed=0;
                    assert(rmWeaponFxMissileImpact(&missile,&position,&velocity));
                    universe.totaltimeelapsed=.1f;vertices=0;
                    rmWeaponFxDrawTransient(&camera);
                    assert(vertices>0 && vertices<=24*4);
                    universe.totaltimeelapsed=2;vertices=0;
                    rmWeaponFxDrawTransient(&camera);assert(!vertices);
                }
            }
        }
        rmWeaponFxShutdown();
        puts("Both Raider corvette weapons retain native FX at LOD0-4; steam detaches and flashes follow the gun");
    }
    if(argc==3 && !strcmp(argv[2],"--system")) {
        unsigned lod,kind;vector trail[3]={{0,0,0},{0,0,-10},{0,0,-20}};
        cloudSystem cloud={0};DustCloud dust={0};
        Missile missile={0};Effect utility={0};float parameters[4]={100,1,1,0};
        ship.shiprace=R1;ship.flags=0;ship.shiptype=HeavyInterceptor;
        ship.rotinfo.coordsys.m11=ship.rotinfo.coordsys.m22=ship.rotinfo.coordsys.m33=1;
        ship.rotinfo.coordsys.m13=ship.rotinfo.coordsys.m31=0;
        for(lod=0;lod<5;++lod) {
            ship.currentLOD=lod;vertices=0;
            assert(rmFxDrawEngine(&ship,0,trail,3,&camera));assert(vertices && !pushes);
        }
        missile.rotinfo.coordsys.m33=1;
        for(kind=0;kind<3;++kind) {
            missile.race=kind==0?R1:kind==1?R2:P1;vertices=0;
            assert(rmFxDrawMissileTrail(&missile,trail,3,&camera));assert(vertices && !pushes);
        }
        utility.owner=&ship;utility.variable=(ubyte*)parameters;utility.rotinfo.coordsys.m33=1;
        utility.timeElapsed=.25f;ship.rceffect=&utility;
        for(kind=0;kind<3;++kind) {
            ship.shiptype=kind==2?RepairCorvette:ResourceCollector;ship.shiprace=kind==1?R2:R1;
            vertices=0;
            if(kind==2){assert(rmFxDrawUtilityEffect(&utility,&camera));assert(vertices && !pushes);}
            else {assert(!rmFxDrawUtilityEffect(&utility,&camera));assert(!vertices);}
        }
        ship.rceffect=NULL;assert(!rmFxDrawUtilityEffect(&utility,&camera));
        for(kind=0;kind<7;++kind) {
            vertices=0;assert(rmFxDrawLegacyEffect(&utility,kind,100,&camera));assert(vertices && !pushes);
        }
        ship.shiprace=R1;ship.shiptype=HeavyInterceptor;
        rmWeaponFxReset();universe.totaltimeelapsed=0;
        assert(rmFxShipDeath(&ship));universe.totaltimeelapsed=.1f;vertices=0;
        rmWeaponFxDrawTransient(&camera);assert(vertices && !pushes);
        assert(engineLights>=8 && deathLights>0);
        dust.objtype=OBJ_DustType;dust.stub=&cloud;dust.resourcevalue=100;cloud.radius=100;cloud.healthFactor=1;
        for(kind=0;kind<4;++kind) {
            dust.dustcloudtype=kind;vertices=0;
            assert(rmFxDrawResourceDust(&dust,&camera));assert(vertices && !pushes);
        }
        dust.resourcevalue=0;assert(!rmFxDrawResourceDust(&dust,&camera));
        dust.resourcevalue=1;dust.flags=SOF_Hide;assert(!rmFxDrawResourceDust(&dust,&camera));
        rmWeaponFxShutdown();puts("RM exhaust at LOD0-4, death and all four harvestable resource cloud effects passed");
    }
}
