R"(
struct MissionLight {
    float3 position; uint type;
    float3 direction; float radius;
    float3 color; float intensity;
    float3 endPosition; float coneCos;
    float edgeCos,source; float2 padding;
};
Texture2D<float> MissionShadow : register(t2);
Texture2D<float4> MissionEmission : register(t5);
Texture2D<float4> MissionIdleEmission : register(t6);
cbuffer MissionEmissionParameters : register(b4) {uint MissionEmissionFlags;float MissionEmissionGain,MissionIdleEmissionGain,MissionEmissionPadding;};
SamplerComparisonState MissionShadowSampler : register(s1);
StructuredBuffer<uint> MissionLightIndices : register(t3);
struct MissionInstance { float4 modelView[3]; float4 clipEquation; };
StructuredBuffer<MissionInstance> MissionInstances : register(t4);
cbuffer MissionSelection : register(b3) { uint MissionLightOffset, MissionLightCount; };
cbuffer MissionLighting : register(b2) {
    float4 MissionShadowRows[4];
    float4 MissionParameters; // light count, shadow light index, exposure, strength
    float4 MissionBias; // receiver depth bias, normal world bias, map valid, texel size
    MissionLight MissionLights[512];
};
float MissionVisibility(float3 eye,float3 normal) {
    if(MissionBias.z<0.5 || MissionParameters.w<=0) return 1;
    float4 p=float4(eye+normal*MissionBias.y,1);
    float3 q=float3(dot(MissionShadowRows[0],p),dot(MissionShadowRows[1],p),dot(MissionShadowRows[2],p));
    float2 uv=float2(q.x*0.5+0.5,0.5-q.y*0.5);
    // Receiver-plane correction follows the hull slope at each PCF tap.
    // Hardware comparison filtering removes the old nine-level shade steps.
    float3 dx=ddx(float3(uv,q.z)),dy=ddy(float3(uv,q.z));
    float determinant=dx.x*dy.y-dx.y*dy.x;
    float2 gradient=abs(determinant)>1e-16 ?
        float2(dx.z*dy.y-dy.z*dx.y,dy.z*dx.x-dx.z*dy.x)/determinant : 0;
    if(any(uv<0)||any(uv>1)||q.z<=0||q.z>=1) return 1;
    // The continuous 3x3 box spans four texels on each axis. Pair their
    // weights into four bilinear comparisons instead of nine comparison
    // samples, preserving the footprint and total weight of nine.
    float2 pixel=uv/MissionBias.w-0.5;
    float2 fraction=frac(pixel),lowWeight=2-fraction,highWeight=1+fraction;
    float2 center=(floor(pixel)+0.5)*MissionBias.w;
    float2 lowOffset=(-1+1/lowWeight)*MissionBias.w;
    float2 highOffset=(1+fraction/highWeight)*MissionBias.w;
    float visible=0;
    [unroll] for(int y=0;y<2;y++) [unroll] for(int x=0;x<2;x++)
    {
        float2 sampleUv=center+float2(x?highOffset.x:lowOffset.x,y?highOffset.y:lowOffset.y);
        float weight=(x?highWeight.x:lowWeight.x)*(y?highWeight.y:lowWeight.y);
        visible+=weight*MissionShadow.SampleCmpLevelZero(MissionShadowSampler,sampleUv,q.z+dot(gradient,sampleUv-uv)-MissionBias.x);
    }
    return lerp(1,visible/9,saturate(MissionParameters.w));
}
float3 MissionShade(float3 albedo,float3 eye,float3 normal,float3 emission) {
    float3 n=normal*rsqrt(max(dot(normal,normal),1e-12));
    // Evaluate derivatives outside per-pixel light/normal branches.
    float sunVisibility=MissionVisibility(eye,n);
    float3 lighting=0;
    [loop] for(uint candidate=0;candidate<MissionLightCount;candidate++) {
        uint index=MissionLightIndices[MissionLightOffset+candidate];
        MissionLight light=MissionLights[index];
        if(light.type==0) {lighting+=light.color*light.intensity;continue;}
        float3 l;float attenuation=1;
        if(light.type==1) l=normalize(light.direction);
        else {
            float3 p=light.position;
            if(light.type==4) {
                float3 segment=light.endPosition-p;
                p+=segment*saturate(dot(eye-p,segment)/max(dot(segment,segment),0.0001));
            }
            float3 delta=p-eye;float distance=length(delta);
            if(distance<=0.001 || distance>=light.radius) continue;
            l=delta/distance;float range=saturate(1-distance/light.radius);attenuation=range*range;
            if(light.source>0.5 && light.source<1.5) attenuation*=range*range;
            if(light.type==3) attenuation*=smoothstep(min(light.coneCos,light.edgeCos),
                max(light.coneCos,light.edgeCos)+1e-6,dot(-l,normalize(light.direction)));
        }
        float ndl=saturate(dot(n,l));
        if(ndl>0 && index==(uint)MissionParameters.y) attenuation*=sunVisibility;
        lighting+=light.color*(light.intensity*attenuation*ndl);
    }
    // The compatibility foreground is sRGB; the presenter linearizes it once.
    float3 radiance=(pow(max(albedo,0),2.2)*lighting+emission)*MissionParameters.z;
    return pow(max(radiance,0),1.0/2.2);
}
)"
