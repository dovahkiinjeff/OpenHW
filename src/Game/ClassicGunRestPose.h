#ifndef HW_CLASSIC_GUN_REST_POSE_H
#define HW_CLASSIC_GUN_REST_POSE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

/* Metadata only: original HW1 GEO/PEO disk layout. No geometry/textures loaded
   into the renderer and no native joint-name guesses. */
static uint32_t classicGunPoseU32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int classicGunRestPose(const void *bytes,size_t size,const char *name,
    float *concatenated,float *local) {
    const unsigned char *data=(const unsigned char *)bytes;
    uint32_t count,i,index=0,matches=0,depth=0;
    float result[16],own[16],current[16],next[16];
    if(!data || !name || !concatenated || !local || size<68 ||
       (memcmp(data,"RMF97ba",7) && memcmp(data,"RMF99ba",7)) || classicGunPoseU32(data+8)!=0x402) return 0;
    count=classicGunPoseU32(data+40);
    if(!count || count>4096 || count>(size-68)/112) return 0;
    for(i=0;i<count;++i) {
        uint32_t offset=classicGunPoseU32(data+68+112*i);
        if(!offset || offset>=size || !memchr(data+offset,0,size-offset)) return 0;
        if(!strcmp((const char *)data+offset,name)) {index=i;++matches;}
    }
    if(matches!=1) return 0;
    for(i=0;i<16;++i) result[i]=(i%5==0)?1.f:0.f;
    for(;;) {
        const unsigned char *object=data+68+112*index;
        uint32_t parent=classicGunPoseU32(object+36),c,r,k;
        if(++depth>count) return 0;
        for(i=0;i<16;++i) {
            uint32_t bits=classicGunPoseU32(object+48+4*i);
            memcpy(&current[i],&bits,4);
            if(!isfinite(current[i])) return 0;
        }
        if(depth==1) memcpy(own,current,sizeof(own));
        for(c=0;c<4;++c) for(r=0;r<4;++r) {
            next[c*4+r]=0;
            for(k=0;k<4;++k) next[c*4+r]+=current[k*4+r]*result[c*4+k];
        }
        memcpy(result,next,sizeof(result));
        if(!parent) break;
        if(parent<68 || (parent-68)%112 || (parent-68)/112>=count) return 0;
        index=(parent-68)/112;
    }
    memcpy(concatenated,result,sizeof(result));memcpy(local,own,sizeof(own));return 1;
}
#endif
