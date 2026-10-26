namespace {
constexpr unsigned missionShadowSize=2048;
struct MissionLightingFrame {
    float rows[16]={};
    float parameters[4]={};
    float bias[4]={};
    hwmodern::ResolvedSceneLight lights[hwmodern::resolvedSceneLightCapacity]={};
};
static_assert(sizeof(MissionLightingFrame)<65536,"Raster lighting CBV capacity");
MissionLightingFrame missionLighting;
Mat4 missionView=identityMatrix();
bool missionLightingEnabled=false;
float missionReceiverBias=0,missionNormalBias=0;
std::vector<std::array<unsigned,2>> missionSelections;
std::vector<unsigned> missionIndices;
std::vector<std::array<float,6>> missionLocalBounds;
const bool missionThreadsEnabled=[] {const char*v=std::getenv("HW_FRAME_WORKERS");return !v || std::strcmp(v,"0");}();
hwmodern::FrameWorker missionPreparationWorker(L"OpenHW raster lighting preparation");
hwmodern::FrameWorker missionSerialPreparationWorker(L"OpenHW serial raster preparation",false);
bool missionShadowPrepared=false;
struct MissionLightBounds { float low[3],high[3];unsigned light; };
struct MissionLightNode { float low[3],high[3];unsigned first,count,left,right; };
std::vector<MissionLightBounds> missionFiniteLights;
std::vector<MissionLightNode> missionLightTree;
std::vector<unsigned> missionInfiniteLights;
struct MissionShadowInstance { float modelView[12],clipEquation[4]; };
static_assert(sizeof(MissionShadowInstance)==64,"Shadow instance GPU layout");
struct MissionShadowBatch { size_t command;unsigned first,count; };
std::vector<MissionShadowInstance> missionShadowInstances;
std::vector<MissionShadowBatch> missionShadowBatches;
std::vector<size_t> missionShadowOrder;
unsigned buildMissionLightNode(unsigned first,unsigned count) {
    MissionLightNode node={};node.first=first;node.count=count;
    for(unsigned a=0;a<3;++a){node.low[a]=FLT_MAX;node.high[a]=-FLT_MAX;}
    for(unsigned i=first;i<first+count;++i)for(unsigned a=0;a<3;++a){node.low[a]=std::min(node.low[a],missionFiniteLights[i].low[a]);node.high[a]=std::max(node.high[a],missionFiniteLights[i].high[a]);}
    const unsigned index=unsigned(missionLightTree.size());missionLightTree.push_back(node);
    if(count>8){
        unsigned axis=0;for(unsigned a=1;a<3;++a)if(node.high[a]-node.low[a]>node.high[axis]-node.low[axis])axis=a;
        const unsigned middle=first+count/2;
        std::nth_element(missionFiniteLights.begin()+first,missionFiniteLights.begin()+middle,missionFiniteLights.begin()+first+count,
            [axis](const auto&a,const auto&b){return double(a.low[axis])+a.high[axis]<double(b.low[axis])+b.high[axis];});
        const unsigned left=buildMissionLightNode(first,count/2),right=buildMissionLightNode(middle,count-count/2);
        missionLightTree[index].left=left;missionLightTree[index].right=right;missionLightTree[index].count=0;
    }
    return index;
}
void buildMissionLightTree(unsigned count) {
    missionFiniteLights.clear();missionInfiniteLights.clear();missionLightTree.clear();
    missionFiniteLights.reserve(count);missionLightTree.reserve(count*2);
    for(unsigned i=0;i<count;++i){
        const auto&l=missionLighting.lights[i];MissionLightBounds bounds={};bounds.light=i;
        bool finite=l.type>1 && std::isfinite(l.radius);
        for(unsigned a=0;a<3;++a){const float end=l.type==4?l.endPosition[a]:l.position[a];
            bounds.low[a]=std::min(l.position[a],end)-l.radius;bounds.high[a]=std::max(l.position[a],end)+l.radius;
            finite=finite && std::isfinite(l.position[a]) && std::isfinite(end) && std::isfinite(bounds.low[a]) && std::isfinite(bounds.high[a]);}
        if(!finite)missionInfiniteLights.push_back(i);
        else if(l.radius>0)missionFiniteLights.push_back(bounds);
    }
    if(!missionFiniteLights.empty())buildMissionLightNode(0,unsigned(missionFiniteLights.size()));
}
bool missionCaster(const RasterCommand&c) {
    if(c.type!=RasterCommand::Draw || !c.state.worldScene || !c.gpuFixedFunctionEnabled ||
       !c.state.depthWrite || c.state.blend || c.topologyType!=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
       c.fixedStateIndex>=gFrameFixedStates.size())return false;
    const auto&f=gFrameFixedStates[c.fixedStateIndex];
    return f.engineGlowParameters[1]<0.5f && (f.flags[0]!=0 || f.legacyShade.parameters[0]>0.5f);
}
auto missionShadowKey(const RasterCommand&c) {
    const bool alpha=c.state.alphaTest && c.state.texture2D;
    unsigned reference=0;if(alpha)std::memcpy(&reference,&c.state.alphaRef,sizeof(reference));
    return std::make_tuple(reinterpret_cast<uintptr_t>(c.immutableGeometry.get()),c.firstVertex,c.vertexCount,
        c.firstIndex,c.indexCount,c.topology,c.state.polygonMode==GL_LINE,alpha,
        alpha?c.state.texture:0u,alpha?c.state.alphaFunc:0u,reference);
}
void prepareMissionShadowBatches(size_t count) {
    missionShadowOrder.clear();missionShadowInstances.clear();missionShadowBatches.clear();
    for(size_t i=0;i<count;++i)if(missionCaster(gCommands[i]))missionShadowOrder.push_back(i);
    std::stable_sort(missionShadowOrder.begin(),missionShadowOrder.end(),[](size_t a,size_t b){return missionShadowKey(gCommands[a])<missionShadowKey(gCommands[b]);});
    for(size_t index:missionShadowOrder){
        const auto&c=gCommands[index];const auto&f=gFrameFixedStates[c.fixedStateIndex];
        if(missionShadowBatches.empty() || missionShadowKey(gCommands[missionShadowBatches.back().command])!=missionShadowKey(c))
            missionShadowBatches.push_back({index,unsigned(missionShadowInstances.size()),0});
        ++missionShadowBatches.back().count;
        MissionShadowInstance instance={};std::memcpy(instance.modelView,f.modelViewRows,48);
        if(f.flags[3])std::memcpy(instance.clipEquation,f.clipEquationEye,16);else instance.clipEquation[3]=1;
        missionShadowInstances.push_back(instance);
    }
}
// Conservative draw bounds remove only lights whose finite support cannot
// touch any fragment. Keep source order and every potentially contributing
// light; this is spatial culling, not a light-count/brightness quality limit.
bool missionEyeBounds(const RasterCommand&c,float low[3],float high[3],std::array<float,6>*local=nullptr) {
    float center[3],extent[3],lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    if(c.immutableGeometry) {
        std::memcpy(lo,c.immutableGeometry->boundsMin,12);std::memcpy(hi,c.immutableGeometry->boundsMax,12);
    } else {
        for(size_t i=c.firstVertex;i<c.firstVertex+c.vertexCount && i<gFrameVertices.size();++i)
            for(unsigned a=0;a<3;++a){lo[a]=std::min(lo[a],gFrameVertices[i].position[a]);hi[a]=std::max(hi[a],gFrameVertices[i].position[a]);}
    }
    if(local){std::memcpy(local->data(),lo,12);std::memcpy(local->data()+3,hi,12);}
    if(lo[0]>hi[0] || c.fixedStateIndex>=gFrameFixedStates.size())return false;
    for(unsigned a=0;a<3;++a){center[a]=(lo[a]+hi[a])*.5f;extent[a]=(hi[a]-lo[a])*.5f;}
    const auto&f=gFrameFixedStates[c.fixedStateIndex];
    for(unsigned a=0;a<3;++a) {
        float p=f.modelViewRows[a*4+3],radius=0;
        for(unsigned b=0;b<3;++b){p+=f.modelViewRows[a*4+b]*center[b];radius+=std::fabs(f.modelViewRows[a*4+b])*extent[b];}
        radius+=std::fabs(p)*1e-6f+.001f;
        low[a]=p-radius;high[a]=p+radius;
        if(!std::isfinite(low[a]) || !std::isfinite(high[a]))return false;
    }
    return true;
}
bool missionLightTouches(const hwmodern::ResolvedSceneLight&light,const float low[3],const float high[3]) {
    if(light.type<=1)return true; // ambient and directional support is infinite
    if(!std::isfinite(light.radius))return true;
    if(light.radius<=0)return false;
    float distance2=0;
    for(unsigned a=0;a<3;++a) {
        const float start=light.position[a],end=light.type==4?light.endPosition[a]:start;
        if(!std::isfinite(start)||!std::isfinite(end))return true;
        const float gap=std::max(std::max(low[a]-std::max(start,end),std::min(start,end)-high[a]),0.0f);
        distance2+=gap*gap;
    }
    return distance2<=light.radius*light.radius;
}
void queryMissionLights(unsigned nodeIndex,const float low[3],const float high[3]) {
    const auto&node=missionLightTree[nodeIndex];
    for(unsigned a=0;a<3;++a)if(node.high[a]<low[a] || node.low[a]>high[a])return;
    if(!node.count){queryMissionLights(node.left,low,high);queryMissionLights(node.right,low,high);return;}
    for(unsigned i=node.first;i<node.first+node.count;++i){const unsigned light=missionFiniteLights[i].light;
        if(missionLightTouches(missionLighting.lights[light],low,high))missionIndices.push_back(light);}
}
void prepareMissionSelections(size_t count) {
    const unsigned lightCount=missionLightingEnabled?unsigned(missionLighting.parameters[0]):0;
    missionIndices.clear();
    // Prefix is the full reference list: an exact, bounded fallback for
    // pathological geometry or a frame with more than 16 MiB of indices.
    for(unsigned i=0;i<lightCount;++i)missionIndices.push_back(i);
    missionSelections.assign(missionLightingEnabled?count:0,{0,lightCount});
    missionLocalBounds.resize(missionLightingEnabled?count:0);
    buildMissionLightTree(lightCount);
    if(missionLightingEnabled)for(size_t index=0;index<count;++index) {
        const auto&c=gCommands[index];float low[3],high[3];
        if(!missionCaster(c))continue;
        if(!missionEyeBounds(c,low,high,&missionLocalBounds[index]) || missionIndices.size()+lightCount>4*1024*1024)continue;
        const unsigned offset=unsigned(missionIndices.size());
        missionIndices.insert(missionIndices.end(),missionInfiniteLights.begin(),missionInfiniteLights.end());
        if(!missionLightTree.empty())queryMissionLights(0,low,high);
        std::sort(missionIndices.begin()+offset,missionIndices.end());
        missionSelections[index]={offset,unsigned(missionIndices.size())-offset};
    }
    if(missionIndices.empty())missionIndices.push_back(0);
}
void missionNormalize(float v[3]) {
    float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(length>1e-8f)for(unsigned i=0;i<3;++i)v[i]/=length;
}
void missionCross(float out[3],const float a[3],const float b[3]) {
    out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];
}
bool fitMissionShadow(size_t count) {
    missionLighting.bias[2]=0;
    const unsigned light=static_cast<unsigned>(missionLighting.parameters[1]);
    if(!missionLightingEnabled || light>=static_cast<unsigned>(missionLighting.parameters[0]) || missionLighting.parameters[3]<=0)return false;
    float z[3],x[3],y[3],up[3];
    for(unsigned i=0;i<3;++i){z[i]=-missionLighting.lights[light].direction[i];up[i]=missionView.m[4+i];}
    missionNormalize(z);float dot=z[0]*up[0]+z[1]*up[1]+z[2]*up[2];
    if(std::fabs(dot)>.95f)for(unsigned i=0;i<3;++i)up[i]=missionView.m[i];
    missionCross(x,up,z);missionNormalize(x);missionCross(y,z,x);
    const float*axes[]={x,y,z};
    float lo[3]={FLT_MAX,FLT_MAX,FLT_MAX},hi[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};bool found=false;
    for(size_t index=0;index<count;++index) {
        const auto&c=gCommands[index];if(!missionCaster(c))continue;
        const float*low=missionLocalBounds[index].data();const float*high=low+3;
        if(low[0]>high[0])continue;
        const auto&f=gFrameFixedStates[c.fixedStateIndex];
        for(unsigned corner=0;corner<8;++corner) {
            float eye[3]={};
            for(unsigned row=0;row<3;++row) {
                eye[row]=f.modelViewRows[4*row+3];
                for(unsigned a=0;a<3;++a)eye[row]+=f.modelViewRows[4*row+a]*((corner&(1u<<a))?high[a]:low[a]);
            }
            for(unsigned a=0;a<3;++a) {float q=eye[0]*axes[a][0]+eye[1]*axes[a][1]+eye[2]*axes[a][2];lo[a]=std::min(lo[a],q);hi[a]=std::max(hi[a],q);}
            found=true;
        }
    }
    if(!found)return false;
    for(unsigned a=0;a<3;++a) {
        const float extent=std::max(hi[a]-lo[a],10.0f)*1.05f;
        if(!std::isfinite(extent))return false;
        float center=(hi[a]+lo[a])*.5f;
        if(a<2) {float translation=0;for(unsigned j=0;j<3;++j)translation+=missionView.m[12+j]*axes[a][j];
            const float step=extent/missionShadowSize;center=std::floor((center-translation)/step)*step+translation;}
        const float scale=(a<2?2.0f:1.0f)/extent;
        for(unsigned j=0;j<3;++j)missionLighting.rows[a*4+j]=axes[a][j]*scale;
        missionLighting.rows[a*4+3]=-center*scale+(a==2?.5f:0);
        if(a==2)missionLighting.bias[0]=std::max(0.00001f,missionReceiverBias*scale);
    }
    missionLighting.rows[15]=1;missionLighting.bias[1]=missionNormalBias;
    missionLighting.bias[2]=1;missionLighting.bias[3]=1.0f/missionShadowSize;return true;
}
void prepareMissionCpu(size_t count) {
    prepareMissionSelections(count);
    missionShadowPrepared=fitMissionShadow(count);
    if(missionShadowPrepared)prepareMissionShadowBatches(count);
    else {missionShadowBatches.clear();missionShadowInstances.clear();}
}
bool ensureMissionShadow(ID3D12Device*device) {
    if(r.missionShadow)return true;
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width=d.Height=missionShadowSize;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear={};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,r.missionShadowState,&clear,IID_PPV_ARGS(&r.missionShadow))))return false;
    D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;hd.NumDescriptors=1;
    if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&r.missionShadowDsv)))){r.missionShadow.Reset();return false;}
    D3D12_DEPTH_STENCIL_VIEW_DESC dv={};dv.Format=DXGI_FORMAT_D32_FLOAT;dv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(r.missionShadow.Get(),&dv,r.missionShadowDsv->GetCPUDescriptorHandleForHeapStart());
    try {r.missionShadowSlot=gTextureDescriptorSlots.allocate();}
    catch(const std::exception&) {r.missionShadowDsv.Reset();r.missionShadow.Reset();return false;}
    D3D12_SHADER_RESOURCE_VIEW_DESC sv={};sv.Format=DXGI_FORMAT_R32_FLOAT;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2D.MipLevels=1;
    auto cpu=r.srvHeap->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=static_cast<SIZE_T>(r.missionShadowSlot)*r.srvStride;
    device->CreateShaderResourceView(r.missionShadow.Get(),&sv,cpu);return true;
}
bool missionGeometry(ID3D12Device*device,ID3D12GraphicsCommandList*list,const RasterCommand&c,unsigned instances) {
    const auto&vu=r.vertexUploads[r.activeSlot];const auto&iu=r.indexUploads[r.activeSlot];
    D3D12_VERTEX_BUFFER_VIEW v={};v.StrideInBytes=sizeof(RasterVertex);v.SizeInBytes=static_cast<UINT>(c.vertexCount*sizeof(RasterVertex));
    D3D12_INDEX_BUFFER_VIEW i={};i.Format=DXGI_FORMAT_R32_UINT;i.SizeInBytes=static_cast<UINT>(c.indexCount*sizeof(unsigned));
    if(c.immutableGeometry) {const auto&e=c.immutableGeometry;if(!ensureImmutableGpu(device,list,e))return false;v.BufferLocation=e->vertices->GetGPUVirtualAddress();i.BufferLocation=e->indices->GetGPUVirtualAddress();}
    else {if(!vu.resource)return false;v.BufferLocation=vu.resource->GetGPUVirtualAddress()+c.firstVertex*sizeof(RasterVertex);if(c.indexCount && iu.resource)i.BufferLocation=iu.resource->GetGPUVirtualAddress()+c.firstIndex*sizeof(unsigned);}
    list->IASetVertexBuffers(0,1,&v);list->IASetPrimitiveTopology(c.topology);
    if(c.indexCount){list->IASetIndexBuffer(&i);list->DrawIndexedInstanced(static_cast<UINT>(c.indexCount),instances,0,0,0);}
    else {list->IASetIndexBuffer(nullptr);list->DrawInstanced(static_cast<UINT>(c.vertexCount),instances,0,0);}
    return true;
}
void bindMissionLighting(ID3D12GraphicsCommandList*list) {
    missionEmissionBindingsValid=false;
    list->SetGraphicsRootConstantBufferView(5,r.missionLightingUploads[r.activeSlot].resource->GetGPUVirtualAddress());
    auto srv=r.srvHeap->GetGPUDescriptorHandleForHeapStart();
    srv.ptr+=static_cast<UINT64>(r.missionShadow?r.missionShadowSlot:texture(0).descriptorSlot)*r.srvStride;
    list->SetGraphicsRootDescriptorTable(6,srv);
    list->SetGraphicsRootShaderResourceView(7,r.missionIndexUploads[r.activeSlot].resource->GetGPUVirtualAddress());
    const auto&instances=r.missionInstanceUploads[r.activeSlot];
    list->SetGraphicsRootShaderResourceView(9,(instances.resource?instances.resource:r.missionIndexUploads[r.activeSlot].resource)->GetGPUVirtualAddress());
    const unsigned selection[2]={0,0};list->SetGraphicsRoot32BitConstants(8,2,selection,0);
}
bool prepareMissionLighting(ID3D12Device*device,ID3D12GraphicsCommandList*list,size_t count,
    D3D12_CPU_DESCRIPTOR_HANDLE color,D3D12_CPU_DESCRIPTOR_HANDLE depth) {
    auto&upload=r.missionLightingUploads[r.activeSlot];
    if(!ensurePersistentVertexUpload(device,sizeof(MissionLightingFrame),upload))return false;
    auto&indices=r.missionIndexUploads[r.activeSlot];
    if(!ensurePersistentVertexUpload(device,missionIndices.size()*sizeof(unsigned),indices))return false;
    std::memcpy(indices.mapped,missionIndices.data(),missionIndices.size()*sizeof(unsigned));
    const bool shadow=missionShadowPrepared&&ensureMissionShadow(device);
    if(!shadow)missionLighting.bias[2]=0;
    std::memcpy(upload.mapped,&missionLighting,missionLightingEnabled?sizeof(missionLighting):offsetof(MissionLightingFrame,lights));
    // A valid descriptor is always bound, even when this shader branch is off.
    if(!shadow){bindMissionLighting(list);return true;}
    auto&instances=r.missionInstanceUploads[r.activeSlot];
    if(!ensurePersistentVertexUpload(device,missionShadowInstances.size()*sizeof(MissionShadowInstance),instances))return false;
    std::memcpy(instances.mapped,missionShadowInstances.data(),missionShadowInstances.size()*sizeof(MissionShadowInstance));
    bindMissionLighting(list);
    resourceBarrier(list,r.missionShadow.Get(),r.missionShadowState,D3D12_RESOURCE_STATE_DEPTH_WRITE);r.missionShadowState=D3D12_RESOURCE_STATE_DEPTH_WRITE;
    const auto target=r.missionShadowDsv->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(0,nullptr,FALSE,&target);
    list->ClearDepthStencilView(target,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
    D3D12_VIEWPORT vp={0,0,float(missionShadowSize),float(missionShadowSize),0,1};D3D12_RECT rect={0,0,missionShadowSize,missionShadowSize};
    list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&rect);
    for(const auto&batch:missionShadowBatches) {
        const auto&c=gCommands[batch.command];
        auto*pipeline=pipelineFor(device,c,false,true);if(!pipeline)return false;
        list->SetPipelineState(pipeline);
        if(c.state.alphaTest && c.state.texture2D){
            auto&tex=texture(c.state.texture);if(!ensureTextureGpu(device,list,tex))return false;
            auto base=r.srvHeap->GetGPUDescriptorHandleForHeapStart();base.ptr+=static_cast<UINT64>(tex.descriptorSlot)*r.srvStride;
            auto sampler=r.samplerHeap->GetGPUDescriptorHandleForHeapStart();sampler.ptr+=static_cast<UINT64>(samplerIndex(tex))*r.samplerStride;
            list->SetGraphicsRootDescriptorTable(0,base);list->SetGraphicsRootDescriptorTable(4,base);list->SetGraphicsRootDescriptorTable(1,sampler);
        }
        DrawConstants constants={};constants.textureEnabled=c.state.texture2D;constants.alphaTestEnabled=c.state.alphaTest;
        constants.alphaFunction=c.state.alphaFunc;constants.alphaReference=c.state.alphaRef;constants.engineBlend=-1;
        constants.textureEnvMode=batch.first;
        list->SetGraphicsRoot32BitConstants(2,16,&constants,0);
        if(!missionGeometry(device,list,c,batch.count))return false;
    }
    resourceBarrier(list,r.missionShadow.Get(),r.missionShadowState,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);r.missionShadowState=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    list->OMSetRenderTargets(1,&color,FALSE,&depth);return true;
}
}
void beginScene(const float view[16]) {if(view)std::memcpy(missionView.m,view,64);g.draw.worldScene=true;}
void endScene() {g.draw.worldScene=false;}
const float*sceneView(){return missionView.m;}
void setMissionLighting(bool enabled,const std::vector<hwmodern::ResolvedSceneLight>&lights,
    float exposure,float strength,float receiver,float normal) {
    missionLightingEnabled=enabled;missionLighting.parameters[0]=enabled?float(std::min<size_t>(lights.size(),hwmodern::resolvedSceneLightCapacity)):0;
    missionLighting.parameters[1]=float(hwmodern::resolvedSceneLightCapacity);missionLighting.parameters[2]=exposure;missionLighting.parameters[3]=strength;
    missionReceiverBias=receiver;missionNormalBias=normal;
    if(enabled)for(unsigned i=0;i<unsigned(missionLighting.parameters[0]);++i){missionLighting.lights[i]=lights[i];if(lights[i].type==1 && missionLighting.parameters[1]==float(hwmodern::resolvedSceneLightCapacity))missionLighting.parameters[1]=float(i);}
}
