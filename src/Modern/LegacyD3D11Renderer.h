#pragma once
#include "ModernGraphics.h"
// Independent Shader Model 5 renderer. No D3D12 device or DXR dependency.
namespace hwlegacyd3d11 {
bool active();
bool initialize(void *window, unsigned width, unsigned height);
void shutdown();
void beginFrame(unsigned width, unsigned height);
bool frameOpen();
void present(bool swapFrame=true);
void setVSync(int mode);
void beginScene(const float *view);
void endScene();
void setSun(bool valid,const float *direction,const float *color,const float *ambient,
    float sunStrength,float ambientStrength);
void setFrameLighting(const HWModernMapLightEmitter *map,unsigned mapCount,
    const HWModernMissionLightEmitter *mission,unsigned missionCount,
    const HWModernDynamicLightEmitter *dynamic,unsigned dynamicCount,bool replaceMap,
    float environment,float authored,float reflectivity,float roughness,float exposure,
    float fxStrength,float shadowStrength);
void registerMaterial(const void *identity, const char *key, unsigned width,
    unsigned height, const unsigned *surface, const unsigned *emission,
    const unsigned *normal, const unsigned *orm);
bool aliasMaterial(const void *identity, const char *key);
void unregisterMaterial(const void *identity);
}
