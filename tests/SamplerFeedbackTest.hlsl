Texture2DArray<float4> Textures[2] : register(t0);
FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP> Feedback[2] : register(u0, space2);
RWStructuredBuffer<float4> Output : register(u0);
SamplerState LinearWrap : register(s0);
cbuffer Parameters : register(b0) { uint Mode; uint SampleCount; };
[numthreads(64,1,1)]
void TestCS(uint3 tid : SV_DispatchThreadID) {
    uint i=tid.x;
    if(i>=SampleCount)return;
    uint channel=i%4;
    float2 uv=float2(float(i%197)/31.0-2.5, float((i*31)%241)/39.0-1.25);
    float lod=Mode==0 ? float((i/4)%43)*.25 : 3.25;
    if(Mode==4)lod=channel==3 ? 1.25 : 6.25;
    if(Mode==2 || Mode==4)Feedback[0].WriteSamplerFeedbackLevel(Textures[0], LinearWrap, float3(uv,channel), lod);
    if(Mode==3)Feedback[1].WriteSamplerFeedbackLevel(Textures[1], LinearWrap, float3(uv,channel), lod);
    float4 a=Textures[0].SampleLevel(LinearWrap,float3(uv,channel),lod);
    float4 b=Textures[1].SampleLevel(LinearWrap,float3(uv,channel),lod);
    Output[i]=any(asuint(a)!=asuint(b)) ? 1.0 : 0.0;
}
