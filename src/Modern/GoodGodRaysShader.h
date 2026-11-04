#pragma once

/*
 * Altered HLSL adaptation of three-good-godrays, commit
 * 70ad1c41c78796637064b3bb9246be08e81ee777 (godrays.frag/compositor.frag).
 * Copyright © 2022 Casey Primozic
 * Adapted from original code (https://github.com/N8python/goodGodRays)
 * by https://github.com/n8python
 *
 * This software is provided 'as-is', without any express or implied warranty. In
 * no event will the authors be held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including
 * commercial applications, and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 *    that you wrote the original software. If you use this software in a product,
 *    an acknowledgment in the product documentation would be appreciated but is
 *    not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * OpenHW adaptations: directional mission sun visibility replaces Three's
 * shadow map; the existing fleet-centered support bounds the world segment;
 * sample counts are hard bounded with static interleaved-noise offsets;
 * positive local shadow contrast prevents an unoccluded sky pedestal; the
 * compositor adds linear light instead of mixing away the original scene.
 */
namespace hwmodern {
inline constexpr char goodGodRaysShader[] = R"(
Texture2D<float2> GodRayField : register(t19);

uint GoodGodRaysStepCount()
{
    uint quality = (GodRaySourceExternal >> 6u) & 3u;
    return quality == 0u ? 24u : (quality == 1u ? 40u : (quality == 2u ? 60u : 90u));
}

float GoodGodRaysNoise(float2 pixel)
{
    // Upstream interleaved gradient noise, deliberately independent of frame.
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056,0.00583715))));
}

float GoodGodRaysDirectionalPhase(float3 direction)
{
    float cosine = dot(normalize(DustSunDirectionValid.xyz), direction);
    float floorPhase = DustPhaseHG(0.35,0.0);
    return saturate((DustPhaseHG(0.35,cosine)-floorPhase) /
        max(0.0001,DustPhaseHG(0.35,1.0)-floorPhase));
}

float2 GoodGodRaysEvaluate(float2 uv, float2 pixel)
{
    float3 rayVector = DustRayUv00.xyz +
        uv.x*(DustRayUv10.xyz-DustRayUv00.xyz) +
        uv.y*(DustRayUv01SunStrength.xyz-DustRayUv00.xyz);
    float sceneDistance = DustVisibleSceneDistance(uv,rayVector);
    if (GodRays <= 0.0001 || DustSunDirectionValid.w < 0.5 || PresenterPadding.y != 3.0)
        return float2(0.0,sceneDistance);
    float rayLengthSquared = dot(rayVector,rayVector);
    if (rayLengthSquared <= 0.00000001) return float2(0.0,sceneDistance);
    float3 direction = rayVector*rsqrt(rayLengthSquared);
    if (GoodGodRaysDirectionalPhase(direction) <= 0.0) return float2(0.0,sceneDistance);

    float3 center = DustGridMinWidth.xyz+DustGridSizeHeight.xyz*0.5;
    float radius = max(1.0,DustGridSizeHeight.x*0.48);
    float3 origin = DustCameraPositionMaxDistance.xyz;
    float3 delta = origin-center;
    float b = dot(delta,direction);
    float discriminant = b*b-dot(delta,delta)+radius*radius;
    if (discriminant <= 0.0) return float2(0.0,sceneDistance);
    float root = sqrt(discriminant);
    float entry = max(0.0,-b-root);
    float exit = min(max(0.0,sceneDistance),-b+root);
    if (exit <= entry) return float2(0.0,sceneDistance);

    uint steps = GoodGodRaysStepCount();
    float samplesFloat = float(steps);
    float sampleOffset = GoodGodRaysNoise(pixel);
    float segmentLength = exit-entry;
    float3 sun = normalize(DustSunDirectionValid.xyz);
    float3 referenceAxis = abs(sun.z)<0.9 ? float3(0,0,1) : float3(0,1,0);
    float3 shaftU = normalize(cross(sun,referenceAxis));
    float3 shaftV = cross(sun,shaftU);
    float referenceRadius = min(radius*0.25,
        DustGridSizeHeight.x/max(DustGridMinWidth.w,1.0)*1.5);

    // Upstream bounded illumination integral and Beer-Lambert response. Its
    // maxDensity bounds intensity and terminates already saturated rays.
    // Upstream exposes this limit as a setting. Retain almost the full old
    // authored response instead of halving strong existing shafts at 0.5.
    const float maxDensity = 0.98;
    float densityFactor = segmentLength/max(radius*0.08,0.0001);
    float earlyOutThreshold = -log(1.0-maxDensity)*samplesFloat;
    float illumination = 0.0;
    [loop] for (uint step=0u;step<steps;++step)
    {
        float along = entry+(float(step)+sampleOffset)*segmentLength/samplesFloat;
        float3 p = origin+direction*along;
        float envelope = 1.0-smoothstep(0.65,1.0,length(p-center)/radius);
        if (envelope <= 0.0) continue;
        float visibility = DustProceduralSunVisibility(p,true);
        if (visibility <= 0.0) continue;
        float reference = 0.25*(
            DustProceduralSunVisibility(p+shaftU*referenceRadius,true) +
            DustProceduralSunVisibility(p-shaftU*referenceRadius,true) +
            DustProceduralSunVisibility(p+shaftV*referenceRadius,true) +
            DustProceduralSunVisibility(p-shaftV*referenceRadius,true));
        float shadowAmount = max(0.0,visibility-reference);
        illumination += shadowAmount*densityFactor*envelope;
        if (illumination > earlyOutThreshold) break;
    }
    return float2(clamp(1.0-exp(-illumination/samplesFloat),0.0,maxDensity),sceneDistance);
}

float2 GoodGodRaysPS(VertexOutput input) : SV_Target
{
    return GoodGodRaysEvaluate(input.texcoord,input.position.xy);
}

float GoodGodRaysUpsample(float2 uv, float correctDepth)
{
    uint width,height;
    GodRayField.GetDimensions(width,height);
    if (width == 0u || height == 0u) return 0.0;
    float2 fieldUv = float2(uv.x,1.0-uv.y);
    float2 texelPosition = fieldUv*float2(width,height)-0.5;
    int2 first = int2(floor(texelPosition));
    float2 fraction = frac(texelPosition);
    uint quality = (GodRaySourceExternal >> 6u)&3u;
    int extent = quality >= 2u ? 1 : 0;
    float totalIllumination = 0.0;
    float totalWeight = 0.0;
    float closestDepthError = 1.0e30;
    float closestSpatialError = 1.0e30;
    float closestIllumination = 0.0;
    // Three-good-godrays JBU: 2x2 at low/medium, 4x4 at high/ultra.
    [loop] for (int y=-extent;y<=1+extent;++y)
    {
        [loop] for (int x=-extent;x<=1+extent;++x)
        {
            int2 coordinate = clamp(first+int2(x,y),int2(0,0),int2(width,height)-1);
            float2 data = GodRayField.Load(int3(coordinate,0));
            float relativeDepth = abs(data.y-correctDepth)/max(correctDepth,0.001);
            float2 distance = float2(x,y)-fraction;
            float spatialError = dot(distance,distance);
            if (relativeDepth < closestDepthError ||
                (relativeDepth == closestDepthError && spatialError < closestSpatialError))
            {
                closestDepthError = relativeDepth;
                closestSpatialError = spatialError;
                closestIllumination = data.x;
            }
            // Do not borrow illumination from the opposite side of a hull.
            if (relativeDepth >= 0.15) continue;
            float spatialWeight = exp(-spatialError/(2.0*0.8*0.8));
            float depthWeight = exp(-0.5*relativeDepth*relativeDepth/(0.035*0.035));
            float weight = spatialWeight*depthWeight;
            totalIllumination += data.x*weight;
            totalWeight += weight;
        }
    }
    if (totalWeight > 0.000001) return totalIllumination/totalWeight;
    // Only a depth-compatible nearest candidate may fill an empty footprint.
    // Thin surfaces with no matching low-resolution sample remain unlit;
    // they never trigger an unbounded native-resolution ray march.
    return closestDepthError < 0.15 ? closestIllumination : 0.0;
}

float3 GoodGodRaysContribution(float2 uv)
{
    if ((GodRaySourceExternal&32u)==0u || GodRays<=0.0001 || DustSunDirectionValid.w<0.5)
        return 0.0;
    float3 ray = DustRayUv00.xyz +
        uv.x*(DustRayUv10.xyz-DustRayUv00.xyz) +
        uv.y*(DustRayUv01SunStrength.xyz-DustRayUv00.xyz);
    float lengthSquared = dot(ray,ray);
    if (lengthSquared <= 0.00000001) return 0.0;
    float phase = GoodGodRaysDirectionalPhase(ray*rsqrt(lengthSquared));
    if (phase <= 0.0) return 0.0;
    float correctDepth = DustVisibleSceneDistance(uv,ray);
    float illumination = GoodGodRaysUpsample(uv,correctDepth);
    float3 source = max(DustSunColor.rgb,0.0);
    float peak = max(source.r,max(source.g,source.b));
    float response = 1.0-exp(-peak*max(DustRayUv01SunStrength.w,0.0)*0.25);
    float3 tint = source/max(peak,0.0001);
    if ((GodRaySourceExternal&4u)!=0u)
        tint = float3((GodRaySourceExternal>>8u)&255u,
            (GodRaySourceExternal>>16u)&255u,(GodRaySourceExternal>>24u)&255u)/255.0;
    return tint*(0.18*response*max(GodRays,0.0)*phase*max(illumination,0.0));
}
)";
}
