#pragma once
#include <cmath>
namespace hwmodern {
inline bool canSkipSurfaceAnyHit(float opacity, bool hasMaterial,
                                  bool textureResolved, unsigned int minimumAlpha)
{
    // Conservative: unresolved textures and any transparency retain any-hit.
    return std::isfinite(opacity) && opacity >= 1.0f &&
        (!hasMaterial || (textureResolved && minimumAlpha == 255u));
}
}
