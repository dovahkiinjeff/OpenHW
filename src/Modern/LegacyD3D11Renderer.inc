#include "DiagnosticPolicy.h"
// Included in the recorder translation unit so DX11 consumes exactly the same
// geometry, clipping, textures and draw ordering as the native RTX path.
namespace hwlegacyd3d11 {
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swap;
ComPtr<ID3D11RenderTargetView> target;
ComPtr<ID3D11DepthStencilView> depth;
ComPtr<ID3D11VertexShader> vertexShader, skyVS;
ComPtr<ID3D11PixelShader> pixelShader, skyPS;
ComPtr<ID3D11VertexShader> shadowVS;
ComPtr<ID3D11PixelShader> shadowPS;
ComPtr<ID3D11DepthStencilView> shadowDepth;
ComPtr<ID3D11ShaderResourceView> shadowView;
ComPtr<ID3D11SamplerState> shadowSampler;
ComPtr<ID3D11RasterizerState> shadowRaster;
ComPtr<ID3D11Buffer> shadowBuffer;
ComPtr<ID3D11Buffer> lightingBuffer;
ComPtr<ID3D11InputLayout> layout;
ComPtr<ID3D11VertexShader> backgroundVS;
ComPtr<ID3D11InputLayout> backgroundLayout;
ComPtr<ID3D11Buffer> backgroundVertices,backgroundBuffer;
size_t backgroundCapacity=0;
std::unordered_map<unsigned,ComPtr<ID3D11PixelShader>> backgroundPS;
ComPtr<ID3D11Buffer> vertices, indices, fixedBuffer, drawBuffer;
size_t vertexCapacity=0, indexCapacity=0;
unsigned width=0,height=0;
bool opened=false;
int vsync=1;
Mat4 sceneView=identityMatrix();
bool sunValid=false;
float sunDirection[3]={0,0,1},sunColor[3]={1,1,1},skyAmbient[3]={0.05f,0.05f,0.05f};
float sunIntensity=1,ambientIntensity=1;
struct ShadowData {float rows[16];unsigned enabled;float texel,bias,padding;};
ShadowData shadowData={};
struct LightingData {
 float position[32][4],direction[32][4],color[32][4],extra[32][4];
 unsigned count;float reflectivity,roughness,exposure;float ambient[4];
};
LightingData lightingData={};
float shadowStrength=1;
float environmentStrength=1;
struct Material { ComPtr<ID3D11ShaderResourceView> emission, normal, orm; bool rmMaterial=false; };
std::unordered_map<const void*, Material> materials;
std::unordered_map<std::string, Material> aliases;
struct Texture { ComPtr<ID3D11ShaderResourceView> view; };
std::unordered_map<GLuint,Texture> textures;
std::unordered_map<unsigned long long,ComPtr<ID3D11BlendState>> blends;
std::unordered_map<unsigned,ComPtr<ID3D11DepthStencilState>> depths;
std::unordered_map<unsigned,ComPtr<ID3D11RasterizerState>> rasters;
std::unordered_map<unsigned,ComPtr<ID3D11SamplerState>> samplers;
struct DrawData {
    unsigned textured, env, alpha, alphaFunction;
    float alphaReference; unsigned fog; float blend; unsigned raw;
    float fogColor[4];
    unsigned normalMap, emissionMap, ormMap; float engineBlend;
};
const char *shader=R"(
Texture2D Base:register(t0); Texture2D Normal:register(t1);
Texture2D Emission:register(t2); Texture2D ORM:register(t3);
Texture2D EngineIdleBase:register(t5); Texture2D EngineIdleEmission:register(t6);
Texture2D<float> Shadow:register(t4);SamplerComparisonState ShadowSamp:register(s1);
cbuffer ShadowConstants:register(b2){float4 shadowRows[4];uint shadowEnabled;float shadowTexel,shadowBias,shadowPadding;};
cbuffer LightingConstants:register(b3){float4 localPosition[32],localDirection[32],localColor[32],localExtra[32];uint localCount;float reflectivity,baseRoughness,exposure;float4 localAmbient;};
SamplerState Samp:register(s0);
cbuffer Draw:register(b0) {
 uint textured,env,alpha,alphaFunction;
 float alphaReference; uint fog; float blend; uint raw;
 float4 fogColor; uint normalMap,emissionMap,ormMap; float engineBlend;
};
cbuffer Fixed:register(b1) {
 float4 glowColor,glowParameters; float4 normalRows[3];
 float4 mv[4],mvp[4],matAmbient,matDiffuse,matEmission,ambient,clipPlane,fogParams;
 uint4 flags; float4 lightPosition[8],lightAmbient[8],lightDiffuse[8];
};
struct In {float4 p:POSITION;float4 c:COLOR;float2 uv:TEXCOORD0;
 float4 n:NORMAL;float f:TEXCOORD1;float clip:TEXCOORD2;};
struct Out {float4 p:SV_Position;float4 c:COLOR;float2 uv:TEXCOORD0;
 float3 n:TEXCOORD1;float3 eye:TEXCOORD2;float f:TEXCOORD3;float clip:SV_ClipDistance;
 float4 shadow:TEXCOORD4;};
Out VS(In i) {
 Out o; o.c=i.c;o.uv=i.uv;o.n=i.n.xyz;o.eye=0;o.f=i.f;o.clip=i.clip;o.shadow=0;
 if(raw!=0) {
  float4 e=float4(dot(mv[0],i.p),dot(mv[1],i.p),dot(mv[2],i.p),dot(mv[3],i.p));
  float4 p=float4(dot(mvp[0],i.p),dot(mvp[1],i.p),dot(mvp[2],i.p),dot(mvp[3],i.p));
  // DX12 records bottom-up render targets and flips them in its presenter.
  // DX11 renders directly to the swap chain: keep GL's positive-up clip Y.
  o.p=float4(p.x,p.y,(p.z+p.w)*0.5,p.w);o.eye=e.xyz;
  o.shadow=float4(dot(shadowRows[0],e),dot(shadowRows[1],e),dot(shadowRows[2],e),1);
  o.n=float3(dot(normalRows[0].xyz,i.n.xyz),dot(normalRows[1].xyz,i.n.xyz),dot(normalRows[2].xyz,i.n.xyz));
  if(flags.w!=0)o.clip=dot(clipPlane,e);
  float d=abs(e.z);o.f=1;
  if(flags.z==0x800)o.f=exp(-fogParams.x*d);
  else if(flags.z==0x801)o.f=exp(-pow(fogParams.x*d,2));
  else if(flags.z==0x2601)o.f=(fogParams.z-d)/max(fogParams.z-fogParams.y,1e-5);
 } else o.p=float4(i.p.x,-i.p.y,i.p.z,i.p.w);
 return o;
}
bool alphaPass(float a) {
 if(alphaFunction==0x200)return false;
 if(alphaFunction==0x201)return a<alphaReference;
 if(alphaFunction==0x202)return a==alphaReference;
 if(alphaFunction==0x203)return a<=alphaReference;
 if(alphaFunction==0x204)return a>alphaReference;
 if(alphaFunction==0x205)return a!=alphaReference;
 if(alphaFunction==0x206)return a>=alphaReference;
 return true;
}
Out ShadowVS(In i){Out o=VS(i);o.p=o.shadow;return o;}
void ShadowPS(Out i){if(alpha!=0&&textured!=0&&!alphaPass(Base.Sample(Samp,i.uv).a))discard;}
float visibility(float4 p){
 if(shadowEnabled==0)return 1;
 float2 uv=p.xy*float2(0.5,-0.5)+0.5;
 if(any(uv<0)||any(uv>1)||p.z<0||p.z>1)return 1;
 float result=0;
 [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++)
  result+=Shadow.SampleCmpLevelZero(ShadowSamp,uv+float2(x,y)*shadowTexel,p.z-shadowBias);
 return lerp(1,result/9,saturate(shadowPadding));
}
float4 PS(Out i):SV_Target {
 float4 t=textured!=0?Base.Sample(Samp,i.uv):float4(1,1,1,1);
 if(textured!=0 && engineBlend>=0)t=lerp(EngineIdleBase.Sample(Samp,i.uv),t,saturate(engineBlend));
 float4 result=i.c;
 if(env==0x1e01 && textured!=0) result=t;
 else if(env==0x2101 && textured!=0)result.rgb=lerp(result.rgb,t.rgb,t.a);
 else result*=t;
 if(alpha!=0&&!alphaPass(result.a))discard;
 if(raw!=0 && flags.x!=0 && glowParameters.y<0.5) {
  float3 n=normalize(i.n);
  if(normalMap!=0) {
   // Derivative tangent frame: works for imported HODs without rewriting joints
   // or guessing tangents from the original game's mesh layout.
   float3 dp1=ddx(i.eye),dp2=ddy(i.eye);float2 duv1=ddx(i.uv),duv2=ddy(i.uv);
   float3 a=cross(dp2,n),b=cross(n,dp1);
   float3 tangent=a*duv1.x+b*duv2.x,bitangent=a*duv1.y+b*duv2.y;
   float scale=rsqrt(max(max(dot(tangent,tangent),dot(bitangent,bitangent)),1e-12));
   float3 map=Normal.Sample(Samp,i.uv).xyz*2-1;
   n=normalize(tangent*scale*map.x+bitangent*scale*map.y+n*map.z);
  }
  float3 v=normalize(-i.eye);float3 lighting=ambient.rgb*matAmbient.rgb;
  float3 specular=0;float roughness=max(baseRoughness,0.08),metallic=0,ao=1,specularStrength=1;
  if(ormMap!=0){float3 m=ORM.Sample(Samp,i.uv).rgb;ao=m.r;roughness=max(m.g,0.08);if(ormMap==2)specularStrength=m.b;else metallic=m.b;}
  float3 albedo=t.rgb*(flags.y!=0?i.c.rgb:matDiffuse.rgb);
  for(uint k=0;k<8;k++)if(lightAmbient[k].w>0.5) {
   float3 l=normalize(lightPosition[k].xyz-i.eye*lightPosition[k].w);
   float shade=k==0?visibility(i.shadow):1;
   float ndl=saturate(dot(n,l))*shade;lighting+=lightAmbient[k].rgb+lightDiffuse[k].rgb*ndl;
   float3 h=normalize(l+v);float exponent=max(2.0,2/(roughness*roughness)-2);
   specular+=lightDiffuse[k].rgb*pow(saturate(dot(n,h)),exponent)*ndl*lerp(0.04,albedo,metallic);
  }
  lighting+=localAmbient.rgb;
  for(uint j=0;j<localCount;j++) {
   float3 delta=localPosition[j].xyz-i.eye;
   float distance=length(delta);float3 l=delta/max(distance,1e-5);
   float attenuation=pow(saturate(1-distance/max(localPosition[j].w,1)),2);
   if(localExtra[j].y>1.5)attenuation*=smoothstep(localExtra[j].x,localDirection[j].w,dot(-l,localDirection[j].xyz));
   float3 radiance=localColor[j].rgb*localColor[j].w*attenuation;
   float ndl=saturate(dot(n,l));lighting+=radiance*ndl;
   float3 h=normalize(l+v);float exponent=max(2.0,2/(roughness*roughness)-2);
   specular+=radiance*pow(saturate(dot(n,h)),exponent)*ndl*lerp(0.04,albedo,metallic);
  }
  result.rgb=(albedo*lighting*ao+specular*reflectivity*specularStrength+matEmission.rgb)*exposure;
  if(emissionMap!=0) {
   float3 emission=Emission.Sample(Samp,i.uv).rgb;
   if(engineBlend>=0)emission=lerp(EngineIdleEmission.Sample(Samp,i.uv).rgb,emission,saturate(engineBlend));
   result.rgb+=emission;
  }
 }
 if(raw!=0&&glowParameters.y>0.5) {
  float glow=pow(saturate(abs(dot(normalize(i.n),normalize(i.eye)))),glowParameters.x);
  result=glowColor;result.g*=min(glow,0.92);result.a*=glow;
  if(textured!=0) {
   if(env==0x1e01)result=t;
   else if(env==0x2101)result.rgb=lerp(result.rgb,t.rgb,t.a);
   else result*=t;
  }
 }
 if(fog!=0)result.rgb=lerp(fogColor.rgb,result.rgb,saturate(i.f));
 if(blend<0.5)result.a=1;
 return result;
}
)";
bool compile(const char *source,const char *entry,const char *profile,ComPtr<ID3DBlob>&code) {
 ComPtr<ID3DBlob> errors;
 HRESULT hr=D3DCompile(source,strlen(source),"OpenHW DX11",nullptr,nullptr,entry,profile,
    D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
 if(FAILED(hr))HW_DIAG_FPRINTF(stderr,"[DX11] %s: %s\n",entry,errors?static_cast<const char*>(errors->GetBufferPointer()):"compile failed");
 return SUCCEEDED(hr);
}
ComPtr<ID3D11ShaderResourceView> rgba(unsigned w,unsigned h,const unsigned *pixels) {
 ComPtr<ID3D11ShaderResourceView> view;if(!pixels||!w||!h)return view;
 D3D11_TEXTURE2D_DESC d={};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
 d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
 d.MipLevels=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
 ComPtr<ID3D11Texture2D> tex;
 if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&tex))&&SUCCEEDED(device->CreateShaderResourceView(tex.Get(),nullptr,&view))){
  context->UpdateSubresource(tex.Get(),0,nullptr,pixels,w*4,0);context->GenerateMips(view.Get());
 }
 return view;
}
bool targets(unsigned w,unsigned h) {
 context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();depth.Reset();
 if(width && FAILED(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0)))return false;
 ComPtr<ID3D11Texture2D> back,depthTex;
 if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&back)))||FAILED(device->CreateRenderTargetView(back.Get(),nullptr,&target)))return false;
 D3D11_TEXTURE2D_DESC d={};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
 d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
 if(FAILED(device->CreateTexture2D(&d,nullptr,&depthTex))||FAILED(device->CreateDepthStencilView(depthTex.Get(),nullptr,&depth)))return false;
 width=w;height=h;return true;
}
bool upload(ComPtr<ID3D11Buffer>&buffer,size_t &capacity,const void *data,size_t size,unsigned bind) {
 if(!size)return true;
 if(size>capacity) {
  buffer.Reset();capacity=(size+65535)&~size_t(65535);
  D3D11_BUFFER_DESC d={};d.ByteWidth=static_cast<UINT>(capacity);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=bind;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
  if(FAILED(device->CreateBuffer(&d,nullptr,&buffer))){capacity=0;return false;}
 }
 D3D11_MAPPED_SUBRESOURCE mapped={};if(FAILED(context->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
 memcpy(mapped.pData,data,size);context->Unmap(buffer.Get(),0);return true;
}
ID3D11ShaderResourceView *texture(GLuint name) {
 auto it=gTextures.find(name);if(it==gTextures.end())return nullptr;
 TextureRecord &r=it->second;auto &t=textures[name];
 if(t.view&&!r.dirty)return t.view.Get();t.view.Reset();
 if(r.compressed&&!r.compressedMips.empty()) {
  D3D11_TEXTURE2D_DESC d={};d.Width=r.width;d.Height=r.height;d.ArraySize=1;d.MipLevels=static_cast<UINT>(r.compressedMips.size());d.Format=r.gpuFormat;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  std::vector<D3D11_SUBRESOURCE_DATA> data(d.MipLevels);
  const unsigned block=(r.gpuFormat==DXGI_FORMAT_BC1_UNORM||r.gpuFormat==DXGI_FORMAT_BC1_UNORM_SRGB||r.gpuFormat==DXGI_FORMAT_BC4_UNORM)?8:16;
  for(unsigned m=0;m<d.MipLevels;m++){data[m].pSysMem=r.compressedMips[m].bytes.data();data[m].SysMemPitch=((r.compressedMips[m].width+3)/4)*block;}
  ComPtr<ID3D11Texture2D> tex;if(SUCCEEDED(device->CreateTexture2D(&d,data.data(),&tex)))device->CreateShaderResourceView(tex.Get(),nullptr,&t.view);
 } else if(!r.pixels.empty()) t.view=rgba(r.width,r.height,reinterpret_cast<const unsigned*>(r.pixels.data()));
 r.dirty=false;return t.view.Get();
}
D3D11_BLEND blendFactor(GLenum v) {
 switch(v){case GL_ZERO:return D3D11_BLEND_ZERO;case GL_ONE:return D3D11_BLEND_ONE;
 case GL_SRC_ALPHA:return D3D11_BLEND_SRC_ALPHA;case GL_ONE_MINUS_SRC_ALPHA:return D3D11_BLEND_INV_SRC_ALPHA;
 case GL_DST_ALPHA:return D3D11_BLEND_DEST_ALPHA;case GL_ONE_MINUS_DST_ALPHA:return D3D11_BLEND_INV_DEST_ALPHA;
 case GL_SRC_COLOR:return D3D11_BLEND_SRC_COLOR;case GL_ONE_MINUS_SRC_COLOR:return D3D11_BLEND_INV_SRC_COLOR;
 case GL_DST_COLOR:return D3D11_BLEND_DEST_COLOR;case GL_ONE_MINUS_DST_COLOR:return D3D11_BLEND_INV_DEST_COLOR;
 default:return D3D11_BLEND_ONE;}
}
void states(const RasterDrawState&s) {
 unsigned long long bk=(unsigned long long(s.blend)<<32)|(unsigned long long(s.blendSrc)<<16)|s.blendDst;
 auto &bs=blends[bk];if(!bs){D3D11_BLEND_DESC d={};auto &r=d.RenderTarget[0];r.BlendEnable=s.blend;r.SrcBlend=blendFactor(s.blendSrc);r.DestBlend=blendFactor(s.blendDst);r.BlendOp=D3D11_BLEND_OP_ADD;r.SrcBlendAlpha=D3D11_BLEND_ONE;r.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;r.BlendOpAlpha=D3D11_BLEND_OP_ADD;r.RenderTargetWriteMask=15;device->CreateBlendState(&d,&bs);}
 context->OMSetBlendState(bs.Get(),nullptr,~0u);
 unsigned dk=s.depthFunc|(s.depthTest<<16)|(s.depthWrite<<17);auto &ds=depths[dk];
 if(!ds){D3D11_DEPTH_STENCIL_DESC d={};d.DepthEnable=s.depthTest;d.DepthWriteMask=s.depthWrite?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=static_cast<D3D11_COMPARISON_FUNC>(s.depthFunc-GL_NEVER+1);device->CreateDepthStencilState(&d,&ds);}context->OMSetDepthStencilState(ds.Get(),0);
 unsigned rk=s.cullMode|(s.cullFace<<16)|(s.scissor<<17)|(s.polygonMode==GL_LINE?1<<18:0);auto &rs=rasters[rk];
 if(!rs){D3D11_RASTERIZER_DESC d={};d.FillMode=s.polygonMode==GL_LINE?D3D11_FILL_WIREFRAME:D3D11_FILL_SOLID;d.CullMode=!s.cullFace?D3D11_CULL_NONE:s.cullMode==GL_FRONT?D3D11_CULL_FRONT:D3D11_CULL_BACK;d.FrontCounterClockwise=TRUE;d.DepthClipEnable=TRUE;d.ScissorEnable=s.scissor;device->CreateRasterizerState(&d,&rs);}context->RSSetState(rs.Get());
 D3D11_VIEWPORT vp={float(s.viewport[0]),float(height-s.viewport[1]-s.viewport[3]),float(s.viewport[2]),float(s.viewport[3]),0,1};context->RSSetViewports(1,&vp);
 D3D11_RECT rect={s.scissorBox[0],LONG(height)-s.scissorBox[1]-s.scissorBox[3],s.scissorBox[0]+s.scissorBox[2],LONG(height)-s.scissorBox[1]};context->RSSetScissorRects(1,&rect);
 unsigned sk=0;auto ti=gTextures.find(s.texture);if(ti!=gTextures.end())sk=(ti->second.wrapS==GL_REPEAT?1:0)|(ti->second.wrapT==GL_REPEAT?2:0)|(ti->second.magFilter==GL_NEAREST?4:0);
 auto &ss=samplers[sk];if(!ss){D3D11_SAMPLER_DESC d={};d.Filter=sk&4?D3D11_FILTER_MIN_MAG_MIP_POINT:D3D11_FILTER_ANISOTROPIC;d.MaxAnisotropy=8;d.AddressU=sk&1?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;d.AddressV=sk&2?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;d.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;d.MaxLOD=D3D11_FLOAT32_MAX;device->CreateSamplerState(&d,&ss);}ID3D11SamplerState*p=ss.Get();context->PSSetSamplers(0,1,&p);
}
GpuFixedFunctionState fixedFor(const RasterCommand&c) {
 GpuFixedFunctionState f={};if(c.gpuFixedFunctionEnabled&&c.fixedStateIndex<gFrameFixedStates.size())f=gFrameFixedStates[c.fixedStateIndex];
 if(c.state.worldScene&&sunValid&&f.flags[0]) {
  memset(f.lightPosition,0,sizeof(f.lightPosition));memset(f.lightAmbient,0,sizeof(f.lightAmbient));memset(f.lightDiffuse,0,sizeof(f.lightDiffuse));
  auto eye=transform(sceneView,sunDirection[0],sunDirection[1],sunDirection[2],0);
  for(unsigned i=0;i<3;i++){f.lightPosition[0][i]=eye[i];f.lightDiffuse[0][i]=sunColor[i]*sunIntensity;f.lightModelAmbient[i]=skyAmbient[i]*ambientIntensity*environmentStrength;}
  f.lightAmbient[0][3]=1;
 }
 return f;
}
void drawGeometry(const RasterCommand&c) {
 if(c.indexCount)context->DrawIndexed(static_cast<UINT>(c.indexCount),static_cast<UINT>(c.firstIndex),static_cast<INT>(c.firstVertex));
 else context->Draw(static_cast<UINT>(c.vertexCount),static_cast<UINT>(c.firstVertex));
}
void prepareDraw(const RasterCommand&c) {
 auto fixed=fixedFor(c);
 Material m;auto mi=materials.find(c.state.materialIdentity);if(fixed.flags[0]&&mi!=materials.end())m=mi->second;
 DrawData draw={};draw.textured=c.state.texture2D;draw.env=c.state.texEnvMode;draw.alpha=c.state.alphaTest;draw.alphaFunction=c.state.alphaFunc;draw.alphaReference=c.state.alphaRef;draw.fog=c.state.fog;draw.blend=c.state.blend;draw.raw=c.gpuFixedFunctionEnabled;memcpy(draw.fogColor,c.state.fogColor,16);draw.normalMap=bool(m.normal);draw.emissionMap=bool(m.emission);draw.ormMap=bool(m.orm);
 draw.engineBlend=c.state.engineBlend;
 Material idle;auto idleIt=materials.find(c.state.engineIdleMaterial);if(idleIt!=materials.end())idle=idleIt->second;
 ID3D11ShaderResourceView *idleViews[]={texture(c.state.engineIdleTexture),idle.emission.Get()};
 context->PSSetShaderResources(5,2,idleViews);
 draw.ormMap=m.orm?(m.rmMaterial?2u:1u):0u;
 context->UpdateSubresource(drawBuffer.Get(),0,nullptr,&draw,0,0);context->UpdateSubresource(fixedBuffer.Get(),0,nullptr,&fixed,0,0);
 ID3D11ShaderResourceView *views[]={texture(c.state.texture),m.normal.Get(),m.emission.Get(),m.orm.Get()};context->PSSetShaderResources(0,4,views);
}
bool caster(const RasterCommand&c) {
 return c.type==RasterCommand::Draw&&c.state.worldScene&&c.gpuFixedFunctionEnabled&&c.state.depthWrite&&!c.state.blend&&c.topologyType==D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE&&gFrameFixedStates[c.fixedStateIndex].flags[0]!=0;
}
void renderShadows() {
 shadowData={};shadowData.texel=1.0f/2048;shadowData.bias=0.0004f;shadowData.padding=shadowStrength;
 if(!sunValid)return;
 auto light=transform(sceneView,sunDirection[0],sunDirection[1],sunDirection[2],0);
 float z[3]={-light[0],-light[1],-light[2]};normalize3(z);
 float up[3]={0,1,0};if(std::fabs(z[1])>0.95f){up[1]=0;up[0]=1;}
 float x[3]={up[1]*z[2]-up[2]*z[1],up[2]*z[0]-up[0]*z[2],up[0]*z[1]-up[1]*z[0]};normalize3(x);
 float y[3]={z[1]*x[2]-z[2]*x[1],z[2]*x[0]-z[0]*x[2],z[0]*x[1]-z[1]*x[0]};
 float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};bool found=false;
 for(const auto&c:gCommands)if(caster(c)) {
  const auto&f=gFrameFixedStates[c.fixedStateIndex];
  for(size_t v=c.firstVertex;v<c.firstVertex+c.vertexCount;v++) {
   const auto&p=gFrameVertices[v].position;float eye[3]={};
   for(int r=0;r<3;r++)for(int j=0;j<4;j++)eye[r]+=f.modelViewRows[r*4+j]*p[j];
   const float*axes[]={x,y,z};for(int a=0;a<3;a++){float q=0;for(int j=0;j<3;j++)q+=eye[j]*axes[a][j];lo[a]=std::min(lo[a],q);hi[a]=std::max(hi[a],q);}
   found=true;
  }
 }
 if(!found)return;
 const float*axes[]={x,y,z};
 for(int a=0;a<3;a++) {
  float extent=std::max(hi[a]-lo[a],10.0f)*1.05f;
  float center=(hi[a]+lo[a])*0.5f;
  if(a<2){float step=extent/2048;center=std::floor(center/step)*step;}
  float scale=(a<2?2.0f:1.0f)/extent;
  for(int j=0;j<3;j++)shadowData.rows[a*4+j]=axes[a][j]*scale;
  shadowData.rows[a*4+3]=-center*scale+(a==2?0.5f:0.0f);
 }
 shadowData.rows[15]=1;shadowData.enabled=1;
 context->UpdateSubresource(shadowBuffer.Get(),0,nullptr,&shadowData,0,0);
 ID3D11ShaderResourceView*nullView=nullptr;context->PSSetShaderResources(4,1,&nullView);
 context->OMSetRenderTargets(0,nullptr,shadowDepth.Get());context->ClearDepthStencilView(shadowDepth.Get(),D3D11_CLEAR_DEPTH,1,0);
 context->IASetInputLayout(layout.Get());context->VSSetShader(shadowVS.Get(),nullptr,0);context->PSSetShader(shadowPS.Get(),nullptr,0);
 for(const auto&c:gCommands)if(caster(c)) {
  states(c.state);context->RSSetState(shadowRaster.Get());D3D11_VIEWPORT vp={0,0,2048,2048,0,1};context->RSSetViewports(1,&vp);
  prepareDraw(c);context->IASetPrimitiveTopology(c.topology);drawGeometry(c);
 }
}
void drawBackground(const RasterCommand&c) {
 auto &ps=backgroundPS[c.backgroundProgram];
 if(!ps&&c.backgroundProgram<backgroundPrograms.size()){
  const auto&code=backgroundPrograms[c.backgroundProgram].pixelShader;
  device->CreatePixelShader(code.data(),code.size(),nullptr,&ps);
 }
 if(!ps)return;
 RasterDrawState s=c.state;s.depthTest=true;s.depthFunc=GL_LEQUAL;s.depthWrite=(c.backgroundFlags&3)==0;
 s.blend=(c.backgroundFlags&3)!=0;s.blendSrc=(c.backgroundFlags&3)==1||(c.backgroundFlags&3)==3?GL_SRC_ALPHA:GL_ONE;s.blendDst=(c.backgroundFlags&3)==1?GL_ONE_MINUS_SRC_ALPHA:(c.backgroundFlags&3)>=2?GL_ONE:GL_ZERO;
 s.cullFace=(c.backgroundFlags&12)!=0;s.cullMode=(c.backgroundFlags&12)==4?GL_BACK:GL_FRONT;states(s);
 UINT stride=sizeof(HWBackgroundVertex),offset=0;ID3D11Buffer*vb=backgroundVertices.Get();context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
 context->UpdateSubresource(backgroundBuffer.Get(),0,nullptr,&gBackgroundConstants[c.backgroundParameterIndex],0,0);
 ID3D11Buffer*cb=backgroundBuffer.Get();context->VSSetConstantBuffers(0,1,&cb);context->PSSetConstantBuffers(0,1,&cb);
 ID3D11ShaderResourceView*views[HW_BACKGROUND_TEXTURES];for(unsigned t=0;t<HW_BACKGROUND_TEXTURES;t++)views[t]=texture(c.backgroundTextures[t]);context->PSSetShaderResources(0,HW_BACKGROUND_TEXTURES,views);
 context->IASetInputLayout(backgroundLayout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(backgroundVS.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->Draw(static_cast<UINT>(c.vertexCount),static_cast<UINT>(c.firstVertex));
}
}
bool active(){return device&&swap&&target;}
bool frameOpen(){return opened;}
void setVSync(int mode){vsync=mode!=0;}
void beginScene(const float *view){if(view)memcpy(sceneView.m,view,sizeof(sceneView.m));g.draw.worldScene=true;}
void endScene(){g.draw.worldScene=false;g.draw.materialIdentity=nullptr;}
void setSun(bool valid,const float*d,const float*c,const float*a,float strength,float ambientStrength){sunValid=valid&&d&&c;if(d)memcpy(sunDirection,d,12);normalize3(sunDirection);if(c)memcpy(sunColor,c,12);if(a)memcpy(skyAmbient,a,12);sunIntensity=strength;ambientIntensity=ambientStrength;}
void setFrameLighting(const HWModernMapLightEmitter *map,unsigned mapCount,
 const HWModernMissionLightEmitter *mission,unsigned missionCount,const HWModernDynamicLightEmitter *dynamic,unsigned dynamicCount,bool replaceMap,
 float environment,float authored,float reflectivity,float roughness,float exposure,float fxStrength,float shadow) {
 lightingData={};lightingData.reflectivity=reflectivity;lightingData.roughness=roughness;lightingData.exposure=exposure;shadowStrength=shadow;environmentStrength=environment;
 auto add=[&](const float*p,const float*d,const float*c,float intensity,float radius,float cone,float edge,unsigned type){
  if(type==3){for(unsigned j=0;j<3;j++)lightingData.ambient[j]+=c[j]*intensity;return;}
  if(lightingData.count>=32||intensity<=0||radius<=0)return;
  unsigned i=lightingData.count++;auto ep=transform(sceneView,p[0],p[1],p[2],1);auto ed=transform(sceneView,d[0],d[1],d[2],0);
  for(unsigned j=0;j<3;j++){lightingData.position[i][j]=ep[j];lightingData.direction[i][j]=ed[j];lightingData.color[i][j]=c[j];}
  normalize3(lightingData.direction[i]);lightingData.position[i][3]=radius;lightingData.color[i][3]=intensity;
  lightingData.direction[i][3]=std::cos(cone*0.01745329252f);lightingData.extra[i][0]=std::cos((cone+std::max(edge,0.01f))*0.01745329252f);lightingData.extra[i][1]=float(type);
 };
 if(!replaceMap)for(unsigned i=0;map&&i<mapCount;i++){const auto&l=map[i];if(l.type!=0&&l.type!=5)add(l.renderPosition,l.renderDirection,l.color,l.intensity*authored,250000,l.coneAngle,l.edgeAngle,l.type==2?2:1);}
 for(unsigned i=0;mission&&i<missionCount;i++){const auto&l=mission[i];if(l.enabled)add(l.position,l.direction,l.color,l.intensity*authored,l.radius,l.coneAngle,l.edgeAngle,l.type);}
 for(unsigned i=0;dynamic&&i<dynamicCount;i++){const auto&l=dynamic[i];float p[3];for(unsigned j=0;j<3;j++)p[j]=l.shape==HW_MODERN_LIGHT_LINE?(l.position[j]+l.endPosition[j])*0.5f:l.position[j];add(p,l.direction,l.color,l.intensity*fxStrength,l.radius,0,0,1);}
}
void shutdown(){if(context){context->ClearState();context->Flush();}materials.clear();aliases.clear();textures.clear();blends.clear();depths.clear();rasters.clear();samplers.clear();backgroundPS.clear();backgroundVS.Reset();backgroundLayout.Reset();backgroundVertices.Reset();backgroundBuffer.Reset();shadowVS.Reset();shadowPS.Reset();shadowDepth.Reset();shadowView.Reset();shadowSampler.Reset();shadowRaster.Reset();shadowBuffer.Reset();lightingBuffer.Reset();target.Reset();depth.Reset();vertices.Reset();indices.Reset();fixedBuffer.Reset();drawBuffer.Reset();layout.Reset();vertexShader.Reset();pixelShader.Reset();skyVS.Reset();skyPS.Reset();swap.Reset();context.Reset();device.Reset();width=height=0;vertexCapacity=indexCapacity=backgroundCapacity=0;opened=false;}
bool initialize(void *window,unsigned w,unsigned h) {
 shutdown();DXGI_SWAP_CHAIN_DESC d={};d.BufferDesc.Width=w;d.BufferDesc.Height=h;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;d.OutputWindow=static_cast<HWND>(window);d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 D3D_FEATURE_LEVEL feature=D3D_FEATURE_LEVEL_11_0;
 wchar_t debugValue[4]={};UINT creationFlags=GetEnvironmentVariableW(L"HW_DX11_DEBUG",debugValue,4)&&debugValue[0]==L'1'?D3D11_CREATE_DEVICE_DEBUG:0;
 HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,creationFlags,&feature,1,D3D11_SDK_VERSION,&d,&swap,&device,nullptr,&context);
 if(hr==DXGI_ERROR_SDK_COMPONENT_MISSING && creationFlags){
  HW_DIAG_FPRINTF(stderr,"[DX11] Optional debug layer unavailable; testing without it.\n");
  hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&feature,1,D3D11_SDK_VERSION,&d,&swap,&device,nullptr,&context);
 }
 if(FAILED(hr)){HW_DIAG_FPRINTF(stderr,"[DX11] Device creation failed: 0x%08lx\n",hr);shutdown();return false;}
 ComPtr<ID3DBlob> vs,ps,sv,sp,shv,shp,bgv;
 // Preserve the shared background shader source and its RTX orientation.
 const std::string skySource=std::string(missionSkyShader)+R"(
 MissionSkyVertexOutput DX11SkyVS(uint id:SV_VertexID){
  MissionSkyVertexOutput o=MissionSkyVS(id);o.position.y=-o.position.y;return o;
 })";
 const std::string backgroundSource=std::string(backgroundVertexShader)+R"(
 BackgroundOutput DX11BackgroundVS(BackgroundInput i){
  BackgroundOutput o=BackgroundVS(i);o.position.y=-o.position.y;return o;
 })";
 if(!compile(shader,"VS","vs_5_0",vs)||!compile(shader,"PS","ps_5_0",ps)||!compile(skySource.c_str(),"DX11SkyVS","vs_5_0",sv)||!compile(missionSkyShader,"MissionSkyPS","ps_5_0",sp)){shutdown();return false;}
 if(!compile(shader,"ShadowVS","vs_5_0",shv)||!compile(shader,"ShadowPS","ps_5_0",shp)||!compile(backgroundSource.c_str(),"DX11BackgroundVS","vs_5_0",bgv)){shutdown();return false;}
 if(FAILED(device->CreateVertexShader(shv->GetBufferPointer(),shv->GetBufferSize(),nullptr,&shadowVS))||FAILED(device->CreatePixelShader(shp->GetBufferPointer(),shp->GetBufferSize(),nullptr,&shadowPS))||FAILED(device->CreateVertexShader(bgv->GetBufferPointer(),bgv->GetBufferSize(),nullptr,&backgroundVS))){shutdown();return false;}
 if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertexShader))||FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixelShader))||FAILED(device->CreateVertexShader(sv->GetBufferPointer(),sv->GetBufferSize(),nullptr,&skyVS))||FAILED(device->CreatePixelShader(sp->GetBufferPointer(),sp->GetBufferSize(),nullptr,&skyPS))){shutdown();return false;}
 D3D11_INPUT_ELEMENT_DESC elements[]={
 {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"NORMAL",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,56,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,60,D3D11_INPUT_PER_VERTEX_DATA,0}};
 if(FAILED(device->CreateInputLayout(elements,6,vs->GetBufferPointer(),vs->GetBufferSize(),&layout))){shutdown();return false;}
 D3D11_INPUT_ELEMENT_DESC bgElements[]={
 {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"BINORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,36,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,64,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,72,D3D11_INPUT_PER_VERTEX_DATA,0}};
 if(FAILED(device->CreateInputLayout(bgElements,7,bgv->GetBufferPointer(),bgv->GetBufferSize(),&backgroundLayout))){shutdown();return false;}
 D3D11_BUFFER_DESC cb={};cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cb.ByteWidth=sizeof(GpuFixedFunctionState);
 if(FAILED(device->CreateBuffer(&cb,nullptr,&fixedBuffer))){shutdown();return false;}cb.ByteWidth=sizeof(DrawData);
 if(FAILED(device->CreateBuffer(&cb,nullptr,&drawBuffer))||!targets(w,h)){shutdown();return false;}
 cb.ByteWidth=sizeof(BackgroundConstants);if(FAILED(device->CreateBuffer(&cb,nullptr,&backgroundBuffer))){shutdown();return false;}
 cb.ByteWidth=sizeof(ShadowData);if(FAILED(device->CreateBuffer(&cb,nullptr,&shadowBuffer))){shutdown();return false;}
 cb.ByteWidth=sizeof(LightingData);if(FAILED(device->CreateBuffer(&cb,nullptr,&lightingBuffer))){shutdown();return false;}
 lightingData={};lightingData.reflectivity=1;lightingData.roughness=0.65f;lightingData.exposure=1;
 D3D11_TEXTURE2D_DESC sd={};sd.Width=sd.Height=2048;sd.MipLevels=sd.ArraySize=1;sd.Format=DXGI_FORMAT_R32_TYPELESS;sd.SampleDesc.Count=1;sd.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> st;D3D11_DEPTH_STENCIL_VIEW_DESC dd={};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
 D3D11_SHADER_RESOURCE_VIEW_DESC vd={};vd.Format=DXGI_FORMAT_R32_FLOAT;vd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;vd.Texture2D.MipLevels=1;
 if(FAILED(device->CreateTexture2D(&sd,nullptr,&st))||FAILED(device->CreateDepthStencilView(st.Get(),&dd,&shadowDepth))||FAILED(device->CreateShaderResourceView(st.Get(),&vd,&shadowView))){shutdown();return false;}
 D3D11_SAMPLER_DESC ss={};ss.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;ss.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;ss.BorderColor[0]=ss.BorderColor[1]=ss.BorderColor[2]=ss.BorderColor[3]=1;ss.MaxLOD=D3D11_FLOAT32_MAX;
 D3D11_RASTERIZER_DESC sr={};sr.FillMode=D3D11_FILL_SOLID;sr.CullMode=D3D11_CULL_NONE;sr.DepthClipEnable=TRUE;sr.DepthBias=100;sr.SlopeScaledDepthBias=1;
 if(FAILED(device->CreateSamplerState(&ss,&shadowSampler))||FAILED(device->CreateRasterizerState(&sr,&shadowRaster))){shutdown();return false;}
 HW_DIAG_FPRINTF(stderr,"[DX11] Shader Model 5 renderer: per-pixel lighting, normal/ORM/emission maps.\n");return true;
}
void registerMaterial(const void *id,const char *key,unsigned w,unsigned h,const unsigned*,const unsigned *e,const unsigned *n,const unsigned *o){if(!active()||!id)return;Material m;m.emission=rgba(w,h,e);m.normal=rgba(w,h,n);m.orm=rgba(w,h,o);m.rmMaterial=o&&key&&std::strstr(key,"|rm-material");materials[id]=m;if(key&&*key)aliases[key]=m;}
bool aliasMaterial(const void*id,const char*key){if(!key)return false;auto it=aliases.find(key);if(it==aliases.end())return false;materials[id]=it->second;return true;}
void unregisterMaterial(const void*id){materials.erase(id);}
void beginFrame(unsigned w,unsigned h){if(!active()||opened)return;if(!w||!h)return;if((w!=width||h!=height)&&!targets(w,h))return;hwglBeginFrame(w,h,0);opened=true;}
void present(bool swapFrame) {
 if(!active()||!opened)return;
 if(!upload(vertices,vertexCapacity,gFrameVertices.data(),gFrameVertices.size()*sizeof(RasterVertex),D3D11_BIND_VERTEX_BUFFER)||!upload(indices,indexCapacity,gFrameIndices.data(),gFrameIndices.size()*4,D3D11_BIND_INDEX_BUFFER)||!upload(backgroundVertices,backgroundCapacity,gBackgroundVertices.data(),gBackgroundVertices.size()*sizeof(HWBackgroundVertex),D3D11_BIND_VERTEX_BUFFER)){hwglEndFrame();opened=false;return;}
 ID3D11RenderTargetView *rt=target.Get();context->OMSetRenderTargets(1,&rt,depth.Get());
 UINT stride=sizeof(RasterVertex),offset=0;ID3D11Buffer *vb=vertices.Get();context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);
 ID3D11Buffer *cb[]={drawBuffer.Get(),fixedBuffer.Get()};context->VSSetConstantBuffers(0,2,cb);context->PSSetConstantBuffers(0,2,cb);
 ID3D11Buffer*scb=shadowBuffer.Get();context->VSSetConstantBuffers(2,1,&scb);context->PSSetConstantBuffers(2,1,&scb);
 context->UpdateSubresource(lightingBuffer.Get(),0,nullptr,&lightingData,0,0);ID3D11Buffer*lcb=lightingBuffer.Get();context->PSSetConstantBuffers(3,1,&lcb);
 renderShadows();context->UpdateSubresource(shadowBuffer.Get(),0,nullptr,&shadowData,0,0);
 context->OMSetRenderTargets(1,&rt,depth.Get());
 for(const auto &c:gCommands) {
  if(c.type==RasterCommand::Clear){if(c.clearMask&GL_COLOR_BUFFER_BIT)context->ClearRenderTargetView(target.Get(),c.clearColor);if(c.clearMask&GL_DEPTH_BUFFER_BIT)context->ClearDepthStencilView(depth.Get(),D3D11_CLEAR_DEPTH,c.clearDepth,0);continue;}
  if(c.type==RasterCommand::BackgroundMesh){drawBackground(c);continue;}
  context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->VSSetConstantBuffers(0,2,cb);context->PSSetConstantBuffers(0,2,cb);
  RasterDrawState drawState=c.state;
  // DrawPixels is a pixel transfer, not a polygon: its recorder quad must
  // never inherit ship/UI face culling from the caller.
  if(c.type==RasterCommand::Draw&&!c.gpuFixedFunctionEnabled)drawState.cullFace=false;
  states(drawState);
  if(c.type==RasterCommand::MissionSky){DrawData sky={};memcpy(&sky,c.skyConstants,sizeof(c.skyConstants));context->UpdateSubresource(drawBuffer.Get(),0,nullptr,&sky,0,0);context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(skyVS.Get(),nullptr,0);context->PSSetShader(skyPS.Get(),nullptr,0);ID3D11ShaderResourceView *view=texture(c.state.texture);context->PSSetShaderResources(0,1,&view);context->Draw(3,0);continue;}
  prepareDraw(c);ID3D11ShaderResourceView*shadow=shadowView.Get();context->PSSetShaderResources(4,1,&shadow);ID3D11SamplerState*ss=shadowSampler.Get();context->PSSetSamplers(1,1,&ss);
  context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(c.topology);context->VSSetShader(vertexShader.Get(),nullptr,0);context->PSSetShader(pixelShader.Get(),nullptr,0);
  drawGeometry(c);
 }
 if(swapFrame){HRESULT hr=swap->Present(vsync,0);if(FAILED(hr))HW_DIAG_FPRINTF(stderr,"[DX11] Present failed: 0x%08lx\n",hr);}
 // Deleted/transient texture names must not retain GPU allocations indefinitely.
 for(auto it=textures.begin();it!=textures.end();)if(gTextures.find(it->first)==gTextures.end())it=textures.erase(it);else ++it;
 hwglEndFrame();opened=false;
}
}
