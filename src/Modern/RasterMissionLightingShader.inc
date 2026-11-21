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
Texture2D<float4> ShipControl : register(t7);
Texture2D<float4> ShipNormal : register(t8);
Texture2D<float4> ShipIdleControl : register(t9);
Texture2D<float4> ShipIdleNormal : register(t10);
Texture2D<float4> ShipSky : register(t11);
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
    float4 ShipSkyRows[3];
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
float3 MissionShade(float3 albedo,float3 eye,float3 normal,float3 emission,float2 uv,float engineBlend) {
    float3 n=normal*rsqrt(max(dot(normal,normal),1e-12));
    // Evaluate derivatives outside per-pixel light/normal branches.
    float sunVisibility=MissionVisibility(eye,n);
    bool native=(MissionEmissionFlags&4u)!=0;
    float4 control=native?ShipControl.Sample(LegacySampler,uv):0;
    if(engineBlend>=0 && (MissionEmissionFlags&16u)!=0)
        control=lerp(ShipIdleControl.Sample(LegacySampler,uv),control,saturate(engineBlend));
    if((MissionEmissionFlags&8u)!=0){
        float3 map=ShipNormal.Sample(LegacySampler,uv).xyz*2-1;
        if(engineBlend>=0 && (MissionEmissionFlags&32u)!=0)
            map=lerp(ShipIdleNormal.Sample(LegacySampler,uv).xyz*2-1,map,saturate(engineBlend));
        n = ShipMappedNormal(n, ddx(eye), ddy(eye), ddx(uv), ddy(uv), map);
    }
    float3 v=normalize(-eye),highlights=0;
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
        if(native && ndl>0 && ShipSkyRows[0].w>0)
            highlights+=light.color*light.intensity*attenuation*ShipSkyRows[0].w*
                ShipHighlight(control,n,v,l,light.type==1?1.0:(light.type==4?0.75:0.25));
    }
    // The compatibility foreground is sRGB; the presenter linearizes it once.
    float3 radiance=pow(max(albedo,0),2.2)*lighting+emission;
    float weight=0;float3 sky=0;
    if(native && (MissionEmissionFlags&64u)!=0 && ShipSkyRows[1].w*ShipSkyRows[2].w>0){
        weight=ShipSkyRows[0].w*ShipSkyRows[2].w*ShipEnvironmentWeight(control,dot(n,v));
        if(weight>0){
            float3 d=reflect(-v,n);
            d=normalize(float3(dot(ShipSkyRows[0].xyz,d),dot(ShipSkyRows[1].xyz,d),dot(ShipSkyRows[2].xyz,d)));
            sky=ShipSkyReflection(ShipSky,LegacySampler,d,ShipReflectionMasks(control).x)*ShipSkyRows[1].w;
        }
    }
    radiance=ShipComposeReflection(radiance,ShipReflectedLayer(sky,weight,highlights))*MissionParameters.z;
    return pow(max(radiance,0),1.0/2.2);
}
)"
