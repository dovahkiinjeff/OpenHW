// HLSL adaptation of zesterer/openmw-volumetric-clouds, clouds.omwfx.
// Pinned upstream: c8830bcd2de0f6e7355f91880308426203eded3c.
// See ../ThirdParty/OpenMWVolumetricClouds/PROVENANCE.md for attribution,
// intentional changes, and the unresolved upstream redistribution license.
// Host owns finite-volume placement, envelope/grid support, authored color,
// compositing, ray bounds, and motion-vector production.
#ifndef OPENHW_OPENMW_CLOUD_KERNEL
#define OPENHW_OPENMW_CLOUD_KERNEL

// Original red channels are bound by the host at t6, linear wrap sampler s1.
Texture2D<float2> OpenMWCloudNoise : register(t6);
SamplerState OpenMWNoiseSampler : register(s1);

float OpenMWCloudHashOne(int q)
{
    // Preserve the upstream modern-GLSL signed int result and 32-bit overflow.
    // Explicit unsigned arithmetic gives DXC/FXC defined wrapping semantics.
    uint bits = ((396128171u * asuint(q)) ^ 388370683u) * 204766743u;
    return float(asint(bits)) * (1.0 / 4294967295.0);
}

float OpenMWCloudNoise3D(float3 p)
{
    p.z *= 15.0;
    int z = (int)floor(p.z);
    float2 offset0 = float2(OpenMWCloudHashOne(z), OpenMWCloudHashOne(z + 73));
    float2 offset1 = float2(OpenMWCloudHashOne(z + 1), OpenMWCloudHashOne(z + 74));
    return lerp(OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset0, 0.0).r,
                OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset1, 0.0).r,
                frac(p.z));
}

float OpenMWCloudBillowNoise3D(float3 p)
{
    p.z *= 15.0;
    int z = (int)floor(p.z);
    float2 offset0 = float2(OpenMWCloudHashOne(z), OpenMWCloudHashOne(z + 73));
    float2 offset1 = float2(OpenMWCloudHashOne(z + 1), OpenMWCloudHashOne(z + 74));
    return lerp(OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset0, 0.0).g,
                OpenMWCloudNoise.SampleLevel(OpenMWNoiseSampler, p.xy + offset1, 0.0).g,
                frac(p.z));
}

float OpenMWCloudResolvedWeight(float footprint)
{
    // The source LUT is 128 texels per XY repeat and 15 slices per Z unit.
    // Only unresolved detail fades, with no camera/time-dependent random phase.
    return 1.0 - smoothstep(0.5, 2.0, max(0.0, footprint) * 128.0);
}

float3 OpenMWCloudAdvectedPosition(float3 p, float simulationTime, float speed)
{
    // Motion is a single rigid translation. No swirl, curl, rotation, changing
    // shape offsets, or opposite-moving octaves. Speed zero is exactly static.
    if (speed != 0.0)
        p.x += simulationTime * speed * 0.003;
    return p;
}

float OpenMWCloudPattern(float3 p, float coverage, float detail,
                        float billowStrength, float simulationTime,
                        float motionSpeed, float footprint)
{
    // p is a fixed local/world-space position times the authored noise scale;
    // upstream uses world position / 500000 for the large cloud scale.
    p = OpenMWCloudAdvectedPosition(p, simulationTime, motionSpeed);
    coverage = saturate(coverage);
    detail = clamp(detail, 0.0, 3.0);
    billowStrength = max(0.0, billowStrength);
    float3 largePosition = p * float3(0.55, 0.55, 1.0);
    float cloudF = OpenMWCloudNoise3D(largePosition + float3(0.0, 0.0, 0.01));
    float cloudF2 = cloudF;
    if (detail >= 2.0)
        cloudF2 = OpenMWCloudNoise3D(largePosition - float3(0.0, 0.0, 0.01));

    float cloudLarge = (cloudF + cloudF2) * 0.5 - 1.0 + coverage;
    if (cloudLarge <= -0.1)
        return 0.0;

    float smallNoise = 0.0;
    if (detail >= 1.0)
    {
        float resolved = OpenMWCloudResolvedWeight(footprint * (500000.0 / 120000.0));
        if (resolved > 0.0)
            smallNoise += (OpenMWCloudBillowNoise3D(p * (500000.0 / 120000.0)) - 0.5) * resolved;
    }
    if (detail >= 3.0)
    {
        float resolved = OpenMWCloudResolvedWeight(footprint * (500000.0 / 30000.0));
        if (resolved > 0.0)
            smallNoise += (OpenMWCloudBillowNoise3D(p * (500000.0 / 30000.0)) - 0.5) * 0.25 * resolved;
    }

    // Retain the source's coverage threshold, billow erosion, and power-five
    // shaping. Host density supplies world-space extinction; there is no sky
    // altitude band, weather control, water-height dependency, or camera fade.
    float cloudFactor = max(cloudLarge - smallNoise * 0.1 * billowStrength, 0.0) * 3.0;
    float cloudFactor2 = cloudFactor * cloudFactor;
    return cloudFactor2 * cloudFactor2 * cloudFactor;
}

float OpenMWCloudTransmission(float extinction, float segmentLength)
{
    // Source pow(0.9999, length * density * 10000), expressed exponentially.
    // This coefficient is -log(0.9999) * 10000, not an arbitrary contrast curve.
    return exp(-1.0000500033335834 * max(0.0, extinction) * max(0.0, segmentLength));
}

float OpenMWCloudSamplingFactor(float samplingQuality)
{
    return pow(1.25, clamp(samplingQuality, -5.0, 10.0));
}

float OpenMWCloudLogSpace(float distance, float samplingQuality)
{
    return log(max(distance, 0.000001)) * OpenMWCloudSamplingFactor(samplingQuality);
}

float OpenMWCloudWorldSpace(float logarithmicDistance, float samplingQuality)
{
    return exp(logarithmicDistance / OpenMWCloudSamplingFactor(samplingQuality));
}

float OpenMWCloudSegmentBoundary(float entryDistance, float exitDistance,
                                uint segmentIndex, uint segmentCount)
{
    // Finite-volume version of the source logarithmic ray distribution. The
    // shifted domain supports a camera inside a cloud (entry=0), and bounded
    // segment count guarantees the final segment reaches the entire exit.
    float entry = max(0.0, entryDistance);
    float exit = max(entry, exitDistance);
    float fraction = min((float)segmentIndex / (float)max(1u, segmentCount), 1.0);
    float shift = max(1.0, (exit - entry) * 0.25);
    float logEntry = log(shift + entry);
    float logExit = log(shift + exit);
    return min(exit, max(entry, exp(lerp(logEntry, logExit, fraction)) - shift));
}

#endif // OPENHW_OPENMW_CLOUD_KERNEL
