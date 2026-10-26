#pragma once
#include <vector>
namespace hwmodern {
// The exact 80-byte light record consumed by the path tracer and rasterizer.
struct ResolvedSceneLight {
    float position[3]; unsigned type;
    float direction[3]; float radius;
    float color[3]; float intensity;
    float endPosition[3]; float coneCos;
    float edgeCos,source,padding[2];
};
static_assert(sizeof(ResolvedSceneLight)==80,"SceneLight GPU layout");
constexpr unsigned resolvedSceneLightCapacity=512;
}
