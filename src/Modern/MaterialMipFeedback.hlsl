// A separate compute pass performs hardware sampler feedback. The DXR pass
// records only actual whole-material minimum LOD requests using ordinary UAV
// atomics, avoiding feedback instructions in hit shaders. This is a whole-mip
// cache hint; UV-cell coverage is deliberately not inferred from this reduction.
Texture2DArray<float4> MaterialTextures[32768] : register(t5);
FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP> MaterialFeedback[32768] : register(u0, space2);
StructuredBuffer<uint> MaterialMipRequests : register(t0, space3);
SamplerState MaterialSampler : register(s0);

[numthreads(64,1,1)]
void MaterialMipFeedbackCS(uint3 id : SV_DispatchThreadID)
{
    uint index = id.x;
    if (index >= 32768u) return;
    uint request = MaterialMipRequests[index];
    [branch] if (request == 0xffffffffu) return;
    uint width, height, slices, mips;
    MaterialTextures[NonUniformResourceIndex(index)].GetDimensions(0, width, height, slices, mips);
    float lod = min(asfloat(request), (float)(max(1u, mips) - 1u));
    // All four layers share the same mip residency group. Recording their
    // reduced minimum may keep extra warm detail; it cannot remove active mips.
    [unroll] for (uint channel = 0; channel < 4; ++channel)
        MaterialFeedback[NonUniformResourceIndex(index)].WriteSamplerFeedbackLevel(
            MaterialTextures[NonUniformResourceIndex(index)],
            MaterialSampler, float3(0.5, 0.5, channel), lod);
}
