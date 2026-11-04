float DustProceduralSunVisibility(float3 worldPosition, bool shafts = false)
{
    if (PresenterPadding.y != 3.0) return 1.0;
    float3 center = DustGridMinWidth.xyz + DustGridSizeHeight.xyz*0.5;
    float3 u,v;
    DustSunBasis(u,v);
    float3 offset = worldPosition-center;
    float3 local = float3(dot(offset,u),dot(offset,v),
        dot(offset,normalize(DustSunDirectionValid.xyz)));
    float3 nearUv = local/max(DustGridSizeHeight.xyz,0.0001)+0.5;
    // Blend coverage at cascade edges; a hard near/far switch changes the
    // filtered silhouette instantly when the camera resizes the light grid.
    float3 farUv = local/(DustGridSizeHeight.xyz*4.0)+0.5;
    float farVisibility = 1.0;
    if (all(farUv >= 0.0) && all(farUv <= 1.0))
    {
        float4 farSample = DustFarFroxelField.SampleLevel(SourceSampler,
            clamp(farUv,0.00625,0.99375),0);
        float farEdge = max(abs(farUv.x-0.5),
            max(abs(farUv.y-0.5),abs(farUv.z-0.5)))*2.0;
        farVisibility = lerp(saturate(shafts ? farSample.r : farSample.a),
            1.0,smoothstep(0.8,1.0,farEdge));
    }
    if (all(nearUv >= 0.0) && all(nearUv <= 1.0))
    {
        uint width,height,depth;
        DustFroxelField.GetDimensions(width,height,depth);
        float3 active = float3(DustGridMinWidth.w,DustGridSizeHeight.w,PresenterPadding.x);
        float4 visibility = DustFroxelField.SampleLevel(SourceSampler,
            clamp(nearUv*active,0.5,active-0.5)/float3(width,height,depth),0);
        float nearEdge = max(abs(nearUv.x-0.5),
            max(abs(nearUv.y-0.5),abs(nearUv.z-0.5)))*2.0;
        return lerp(saturate(shafts ? visibility.r : visibility.a),
            farVisibility,smoothstep(0.8,1.0,nearEdge));
    }
    return farVisibility;
}

