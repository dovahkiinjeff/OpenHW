#ifndef HW_REMASTERED_WEAPON_FX_DATA_H
#define HW_REMASTERED_WEAPON_FX_DATA_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#define RMFX_NONE UINT32_MAX
#define RMFX_NO_DEPTH_TEST 256u /* source metadata in the blend field */
#define RMFX_DETACHED_PARTICLE 512u /* spray leaves the weapon after emission */
#define RMFX_INHERIT_VELOCITY 1024u
typedef struct { char magic[8]; uint32_t mappings,effects,layers,keys,textures; } RmFxHeader;
typedef struct { char ship[32]; uint32_t gun,projectile,muzzle,impact; float scale; } RmFxMapping;
typedef struct { uint32_t first,count; float duration; } RmFxEffect;
typedef struct { uint32_t first,count; } RmFxCurve;
typedef struct {
    uint32_t kind,texture,blend,loop,columns,rows,frames,start;
    float duration,delay,scale,velocity[3],fps;
    RmFxCurve curves[6]; /* width/radius, length, colour, offset, UV repeat, UV offset.
       kind 3 replaces UV-offset with immutable vertex pairs: xyz, uv. */
} RmFxLayer;
typedef struct { float time,value[4]; } RmFxKey;
static int rmFxRange(size_t offset,size_t count,size_t stride,size_t size)
{ return offset<=size && count<=(size-offset)/stride; }
/* Validate before taking ANY pointer into the package or uploading textures. */
static int rmFxValidate(const void *bytes,size_t size)
{
    const unsigned char *data=(const unsigned char *)bytes; RmFxHeader h;
    size_t offset=sizeof(h),i,j; RmFxMapping m; RmFxEffect e; RmFxLayer l; RmFxKey k;
    if(!bytes || size<sizeof(h)) return 0; memcpy(&h,data,sizeof(h));
    if(memcmp(h.magic,"HWRMFX01",8) || h.mappings>1024 || h.effects>1024 || h.layers>32768 || h.keys>2000000 || h.textures>256) return 0;
    if(!rmFxRange(offset,h.mappings,sizeof(m),size)) return 0;
    for(i=0;i<h.mappings;i++) {
        memcpy(&m,data+offset+i*sizeof(m),sizeof(m));
        if(!memchr(m.ship,0,sizeof(m.ship)) || m.gun>=64 || !isfinite(m.scale) || m.scale<=0 || m.scale>1000) return 0;
        if((m.projectile!=RMFX_NONE && m.projectile>=h.effects) || (m.muzzle!=RMFX_NONE && m.muzzle>=h.effects) || (m.impact!=RMFX_NONE && m.impact>=h.effects)) return 0;
    }
    offset+=h.mappings*sizeof(m);
    if(!rmFxRange(offset,h.effects,sizeof(e),size)) return 0;
    for(i=0;i<h.effects;i++) {
        memcpy(&e,data+offset+i*sizeof(e),sizeof(e));
        if(e.first>h.layers || e.count>h.layers-e.first || e.count>512 || !isfinite(e.duration) || e.duration<=0 || e.duration>180) return 0;
    }
    offset+=h.effects*sizeof(e);
    if(!rmFxRange(offset,h.layers,sizeof(l),size)) return 0;
    for(i=0;i<h.layers;i++) {
        memcpy(&l,data+offset+i*sizeof(l),sizeof(l));
        if(l.kind<1 || l.kind>3 || l.texture>=h.textures || !l.columns || !l.rows || l.columns>128 || l.rows>128 || !l.frames || l.frames>l.columns*l.rows) return 0;
        if(!isfinite(l.duration) || l.duration<=0 || l.duration>180 || !isfinite(l.delay) || l.delay<0 || l.delay>180 || !isfinite(l.scale) || l.scale<=0 || l.scale>100000 || !isfinite(l.fps) || l.fps<0 || l.fps>1000) return 0;
        for(j=0;j<3;j++) if(!isfinite(l.velocity[j]) || fabsf(l.velocity[j])>100000) return 0;
        for(j=0;j<6;j++) {
            uint32_t limit=(l.kind==3 && j==5)?65532:128;
            if(!l.curves[j].count || l.curves[j].count>limit || l.curves[j].first>h.keys || l.curves[j].count>h.keys-l.curves[j].first) return 0;
        }
        if(l.kind==3 && l.curves[5].count%6) return 0;
    }
    offset+=h.layers*sizeof(l);
    if(!rmFxRange(offset,h.keys,sizeof(k),size)) return 0;
    for(i=0;i<h.keys;i++) {
        memcpy(&k,data+offset+i*sizeof(k),sizeof(k));
        if(!isfinite(k.time) || k.time<0 || k.time>1) return 0;
        for(j=0;j<4;j++) if(!isfinite(k.value[j]) || fabsf(k.value[j])>100000) return 0;
    }
    offset+=h.keys*sizeof(k);
    for(i=0;i<h.textures;i++) {
        uint32_t dimensions[2]; size_t count;
        if(!rmFxRange(offset,2,sizeof(uint32_t),size)) return 0;
        memcpy(dimensions,data+offset,sizeof(dimensions)); offset+=sizeof(dimensions);
        if(!dimensions[0] || !dimensions[1] || dimensions[0]>4096 || dimensions[1]>4096) return 0;
        count=(size_t)dimensions[0]*dimensions[1];
        if(!rmFxRange(offset,count,4,size)) return 0; offset+=count*4;
    }
    return offset==size;
}
#endif
