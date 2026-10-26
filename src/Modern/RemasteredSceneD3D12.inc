#include "DiagnosticPolicy.h"
/* Included inside the native raster namespace. Source material shader bytecode
   is supplied by the local translation tool, not bundled copyrighted GLSL. */
struct BackgroundProgram
{
    std::vector<unsigned char> pixelShader;
    std::unordered_map<unsigned int, ComPtr<ID3D12PipelineState>> pipelines;
};
std::vector<BackgroundProgram> backgroundPrograms(1);

static const char *backgroundVertexShader = R"(
cbuffer BackgroundData : register(b0) {
    float4 Camera0, Camera1, Camera2;
    float4 Object0, Object1, Object2, Object3;
    float4 Projection;
    float4 RMParams[128];
};
struct BackgroundInput {
    float3 position:POSITION; float3 normal:NORMAL;
    float3 tangent:TANGENT; float3 binormal:BINORMAL;
    float4 color:COLOR; float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;
};
struct BackgroundOutput {
    float4 position:SV_Position; float2 uv0:TEXCOORD0; float2 uv1:TEXCOORD1;
    float3 normal:TEXCOORD2; float3 tangent:TEXCOORD3; float3 binormal:TEXCOORD4;
    float3 world:TEXCOORD5; float3 object:TEXCOORD6; float3 normworld:TEXCOORD7;
    float4 color:COLOR0;
};
BackgroundOutput BackgroundVS(BackgroundInput input) {
    BackgroundOutput output;
    float3 position=input.position+input.normal*length(input.position)*Projection.z;
    float4x4 model=transpose(float4x4(Object0,Object1,Object2,Object3));
    float3 world=mul(model,float4(position,1)).xyz;
    float3 eye=float3(dot(Camera0.xyz,world),dot(Camera1.xyz,world),dot(Camera2.xyz,world));
    float w=-eye.z;
    output.position=float4(eye.x*Projection.x+eye.z*Camera0.w,
                         -eye.y*Projection.y-eye.z*Camera1.w,w-0.01,w);
    output.uv0=input.uv0; output.uv1=input.uv1;
    output.normal=input.normal; output.tangent=input.tangent; output.binormal=input.binormal;
    output.normworld=mul((float3x3)model,input.normal);
    output.world=world; output.object=input.position; output.color=input.color;
    return output;
}
)";

bool createBackgroundRoot(ID3D12Device *device)
{
    if (r.backgroundRoot) return true;
    D3D12_DESCRIPTOR_RANGE ranges[HW_BACKGROUND_TEXTURES] = {};
    D3D12_ROOT_PARAMETER roots[HW_BACKGROUND_TEXTURES+1] = {};
    for (unsigned int i=0; i<HW_BACKGROUND_TEXTURES; ++i)
    {
        ranges[i].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[i].NumDescriptors=1; ranges[i].BaseShaderRegister=i;
        roots[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[i].DescriptorTable.NumDescriptorRanges=1;
        roots[i].DescriptorTable.pDescriptorRanges=&ranges[i];
        roots[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    }
    roots[HW_BACKGROUND_TEXTURES].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    roots[HW_BACKGROUND_TEXTURES].Descriptor.ShaderRegister=0;
    roots[HW_BACKGROUND_TEXTURES].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD=D3D12_FLOAT32_MAX; sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters=HW_BACKGROUND_TEXTURES+1; desc.pParameters=roots;
    desc.NumStaticSamplers=1; desc.pStaticSamplers=&sampler;
    desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> blob, errors;
    if (FAILED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors))) return false;
    return SUCCEEDED(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),
        IID_PPV_ARGS(r.backgroundRoot.GetAddressOf())));
}

ID3D12PipelineState *backgroundPipelineFor(ID3D12Device *device, unsigned int id, unsigned int flags)
{
    if (!id || id>=backgroundPrograms.size() || !createBackgroundRoot(device)) return nullptr;
    BackgroundProgram &program=backgroundPrograms[id];
    auto found=program.pipelines.find(flags);
    if (found!=program.pipelines.end()) return found->second.Get();
    ComPtr<ID3DBlob> vs, errors;
    if (FAILED(D3DCompile(backgroundVertexShader,std::strlen(backgroundVertexShader),"RemasteredSceneVS",
        nullptr,nullptr,"BackgroundVS","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors)))
    {
        HW_DIAG_FPRINTF(stderr,"[RemasteredScene] VS compile failed: %s\n",errors ? (char*)errors->GetBufferPointer() : "unknown");
        return nullptr;
    }
    D3D12_INPUT_ELEMENT_DESC inputs[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"BINORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,36,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,64,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,72,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    static_assert(sizeof(HWBackgroundVertex)==80,"background translation vertex stride");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature=r.backgroundRoot.Get();
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};
    desc.PS={program.pixelShader.data(),program.pixelShader.size()};
    desc.InputLayout={inputs,7}; desc.SampleMask=UINT_MAX;
    desc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets=1; desc.RTVFormats[0]=kRasterTargetFormat;
    desc.DSVFormat=kDepthDsvFormat; desc.SampleDesc.Count=1;
    desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode=(flags&12)==4 ? D3D12_CULL_MODE_BACK :
        (flags&12)==8 ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FrontCounterClockwise=FALSE;
    desc.RasterizerState.DepthClipEnable=TRUE;
    auto &blend=desc.BlendState.RenderTarget[0];
    blend.BlendEnable=(flags&3)!=0;
    blend.SrcBlend=((flags&3)==1 || (flags&3)==3) ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE;
    blend.DestBlend=(flags&3)==1 ? D3D12_BLEND_INV_SRC_ALPHA : (flags&3)>=2 ? D3D12_BLEND_ONE : D3D12_BLEND_ZERO;
    blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha=D3D12_BLEND_ONE; blend.DestBlendAlpha=D3D12_BLEND_ZERO;
    // Alpha is foreground coverage, NOT the source material opacity. Background
    // planets/nebulae must never become opaque ship masks in the dust compositor.
    blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN|D3D12_COLOR_WRITE_ENABLE_BLUE;
    // Infinite background distances are unrelated to gameplay depth units.
    // Never allow a planet shell to become a cloud/ship occlusion boundary.
    desc.DepthStencilState.DepthEnable=TRUE;
    desc.DepthStencilState.DepthWriteMask=(flags&3)?D3D12_DEPTH_WRITE_MASK_ZERO:D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DepthStencilState.StencilReadMask=desc.DepthStencilState.StencilWriteMask=255;
    auto &stencil=desc.DepthStencilState.FrontFace;
    stencil.StencilFailOp=stencil.StencilDepthFailOp=stencil.StencilPassOp=D3D12_STENCIL_OP_KEEP;
    stencil.StencilFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.BackFace=stencil;
    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr=device->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&pso));
    if (FAILED(hr)) { HW_DIAG_FPRINTF(stderr,"[RemasteredScene] PSO %u/%u failed: 0x%08lx\n",id,flags,hr); return nullptr; }
    program.pipelines.emplace(flags,pso);
    return pso.Get();
}

bool drawBackgroundCommand(ID3D12Device *device, ID3D12GraphicsCommandList *list,
    const RasterCommand &command, const PersistentVertexUpload &vertices,
    const PersistentVertexUpload &parameters, UINT64 parameterStride, unsigned int width, unsigned int height)
{
    auto pso=backgroundPipelineFor(device,command.backgroundProgram,command.backgroundFlags);
    if (!pso || !vertices.resource || !parameters.resource) return false;
    list->SetGraphicsRootSignature(r.backgroundRoot.Get());
    list->SetPipelineState(pso);
    D3D12_VIEWPORT viewport={0,0,(float)width,(float)height,0,1};
    D3D12_RECT scissor={0,0,(LONG)width,(LONG)height};
    list->RSSetViewports(1,&viewport); list->RSSetScissorRects(1,&scissor);
    for (unsigned int i=0; i<HW_BACKGROUND_TEXTURES; ++i)
    {
        auto found=gTextures.find(command.backgroundTextures[i]);
        TextureRecord &tex=found!=gTextures.end() ? found->second : texture(0);
        if (!ensureTextureGpu(device,list,tex))
        { list->SetGraphicsRootSignature(r.root.Get()); return false; }
        auto handle=r.srvHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr+=(UINT64)tex.descriptorSlot*r.srvStride;
        list->SetGraphicsRootDescriptorTable(i,handle);
    }
    list->SetGraphicsRootConstantBufferView(HW_BACKGROUND_TEXTURES,
        parameters.resource->GetGPUVirtualAddress()+command.backgroundParameterIndex*parameterStride);
    D3D12_VERTEX_BUFFER_VIEW vb={vertices.resource->GetGPUVirtualAddress()+command.firstVertex*sizeof(HWBackgroundVertex),
        (UINT)(command.vertexCount*sizeof(HWBackgroundVertex)),sizeof(HWBackgroundVertex)};
    list->IASetVertexBuffers(0,1,&vb);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced((UINT)command.vertexCount,1,0,0);
    list->SetGraphicsRootSignature(r.root.Get());
    return true;
}
