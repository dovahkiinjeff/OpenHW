// Remastered sob_ship defaults. Upgraded native material pages store original
// reflection sharpness in red and the environment mask in alpha [129,253].
// Alpha 128 marks a bay (its shader omits the environment pass). Older pages
// used constant alpha 255 and still provide a direct-specular strength in blue.
float2 ShipReflectionMasks(float4 control)
{
    if (control.a > 254.0 / 255.0)
        return float2(saturate((0.52 - control.g) / 0.39), control.b);
    return float2(control.r, saturate((control.a * 255.0 - 129.0) / 124.0));
}
// Solve the UV tangent frame in view space. Both projection Y conventions
// and mirrored UV islands must point a normal-map texel in the same direction.
float3 ShipMappedNormal(float3 n, float3 dx, float3 dy, float2 uvDx, float2 uvDy, float3 map)
{
    float orientation = sign(uvDx.x * uvDy.y - uvDx.y * uvDy.x);
    float3 t = (dx * uvDy.y - dy * uvDx.y) * orientation;
    t -= n * dot(n, t);
    float tLength2 = dot(t, t);
    if (tLength2 < 1e-20) return n;
    t *= rsqrt(tLength2);
    float3 b = cross(n, t);
    float3 authoredB = (dy * uvDx.x - dx * uvDy.x) * orientation;
    b *= dot(b, authoredB) < 0.0 ? -1.0 : 1.0;
    return normalize(t * map.x + b * map.y + n * map.z);
}
float ShipFresnel(float nv)
{
    return saturate(3.8 * pow(1.1 * (1.0 - saturate(abs(nv))), 2.1));
}
float ShipEnvironmentWeight(float4 control, float nv)
{
    if (control.a < 128.5 / 255.0) return 0;
    float2 masks = ShipReflectionMasks(control);
    return lerp(0.15 * masks.y, 1.0, 0.35 * masks.x * ShipFresnel(nv));
}
float ShipHighlight(float4 control, float3 n, float3 eye, float3 light, float glossScale)
{
    float exponent = max(1.0, (2.0 / max(0.001,control.g * control.g) - 2.0) * glossScale);
    float strength = lerp(control.b, 1.0, saturate(1.5 * ShipReflectionMasks(control).x * ShipFresnel(dot(n,eye))));
    return strength * pow(saturate(dot(eye,reflect(-light,n))),exponent) * saturate(dot(n,light)*1000.0);
}
float3 ShipComposeReflection(float3 base, float4 reflection)
{
    return base * (1.0 - saturate(reflection.a)) + reflection.rgb;
}
float2 ShipSkyUV(float3 d, bool front)
{
    float2 uv = 0.5 + (0.5 / 1.0625) * d.xy / max(0.001, 1.0 + (front ? d.z : -d.z));
    return float2(0.5 * (uv.x + (front ? 0.0 : 1.0)),uv.y);
}
float3 ShipSkyTap(Texture2D<float4> sky, SamplerState samp, float3 d)
{
    if (d.z >= 0.035) return sky.SampleLevel(samp,ShipSkyUV(d,true),0).rgb;
    if (d.z <= -0.035) return sky.SampleLevel(samp,ShipSkyUV(d,false),0).rgb;
    return lerp(sky.SampleLevel(samp,ShipSkyUV(d,false),0).rgb,
                sky.SampleLevel(samp,ShipSkyUV(d,true),0).rgb,smoothstep(-0.035,0.035,d.z));
}
float3 ShipSkyReflection(Texture2D<float4> sky, SamplerState samp, float3 d, float sharpness)
{
    // The mission atlas may contain only the base level. Keep the soft lobe
    // world-locked and blend it with the sharp sample as the RM shader does.
    float3 sharp = ShipSkyTap(sky,samp,d);
    if (sharpness >= 0.995) return sharp;
    // Frisvad's orthonormal basis avoids switching axes as the camera moves.
    float signZ = d.z >= 0 ? 1 : -1;
    float a = -1.0 / (signZ + d.z), b = d.x*d.y*a;
    float3 t = float3(1.0+signZ*d.x*d.x*a,signZ*b,-signZ*d.x)*0.22;
    float3 u = float3(b,signZ+d.y*d.y*a,-d.y)*0.22;
    float3 soft = sharp * 0.25 + 0.1875 * (
        ShipSkyTap(sky,samp,normalize(d+t)) + ShipSkyTap(sky,samp,normalize(d-t)) +
        ShipSkyTap(sky,samp,normalize(d+u)) + ShipSkyTap(sky,samp,normalize(d-u)));
    return max(lerp(soft,sharp,sharpness),0.0);
}
float4 ShipReflectedLayer(float3 env, float weight, float3 highlights)
{
    // RM first adds 65%, then mixes the surface toward the environment.
    float mixWeight = saturate(0.35 * weight);
    return float4(highlights*(1.0-mixWeight) + env*(0.65*weight*(1.0-mixWeight)+mixWeight), mixWeight);
}
