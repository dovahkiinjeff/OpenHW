#pragma once

// The frame-generation input already contains the final display-size scene,
// including clouds, lens effects, grain and brightness/gamma. Reuse its exact
// UNORM pixels and apply only the existing native UI replacement policy.
namespace hwmodern {
inline constexpr char frameGenerationCompositeShader[] = R"(
Texture2D<float4> SourceFrame : register(t0);
Texture2D<float4> WorldFrame : register(t1);
Texture2D<float4> CompletedScene : register(t2);
SamplerState SourceSampler : register(s0);
cbuffer CompositeConstants : register(b0) { uint DisplayControls; };
struct VertexOutput { float4 position : SV_Position; float2 texcoord : TEXCOORD0; };
VertexOutput CompositeVS(uint id : SV_VertexID) {
    VertexOutput output;
    float2 corner=float2((id<<1)&2,id&2);
    output.position=float4(corner*float2(2,-2)+float2(-1,1),0,1);
    output.texcoord=float2(corner.x,1-corner.y);
    return output;
}
float4 CompositePS(VertexOutput input) : SV_Target {
    float4 source=SourceFrame.Sample(SourceSampler,input.texcoord);
    float3 world=WorldFrame.Sample(SourceSampler,input.texcoord).rgb;
    float3 difference=abs(source.rgb-world);
    if(max(max(difference.r,difference.g),difference.b)<=(1.5/255.0)) {
        source.rgb=CompletedScene.Load(int3(int2(input.position.xy),0)).rgb;
    } else {
        float brightness=float(DisplayControls&65535u)/1000.0;
        float gamma=float(DisplayControls>>16u)/1000.0;
        source.rgb=saturate(pow(max(source.rgb,0.0)*brightness,1.0/max(gamma,0.01)));
    }
    return source;
}
)";
}
