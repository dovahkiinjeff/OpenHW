namespace {
extern bool missionLightingEnabled;
bool missionCaster(const RasterCommand&command);
hwmodern::RasterEmissionDescribe missionEmissionDescribe=nullptr;
hwmodern::RasterEmissionRead missionEmissionRead=nullptr;
struct MissionEmissionTexture { TextureRecord texture;UINT64 bytes=0,lastUse=0; };
std::unordered_map<std::uint64_t,MissionEmissionTexture> missionEmissionTextures;
UINT64 missionEmissionBytes=0;
// Retain camera-visible working sets while measured GPU headroom exists.
// The old 256 MiB / three-frame cache thrashed on normal RM fleet sweeps.
UINT64 missionEmissionBudget=512ull*1024*1024;
UINT64 missionEmissionAdapterBudget=1024ull*1024*1024;
UINT64 missionEmissionLimit=512ull*1024*1024;
UINT64 missionEmissionNextBudgetQuery=0;
ComPtr<IDXGIAdapter3> missionEmissionAdapter;
bool missionEmissionAdapterChecked=false;
UINT64 missionEmissionStagingLastBusy[kFrameSlots]={};
float missionEmissionBrightness=1.0f;
bool missionEmissionBindingsValid=false;
unsigned missionEmissionBoundSlots[2]={};
struct MissionEmissionParameters {unsigned flags;float gain,idleGain,padding;};
MissionEmissionParameters missionEmissionBoundParameters={};
void trimMissionEmissionStaging(unsigned slot) {
    // The caller has waited this flight slot's fence. A cold fleet upload
    // must not leave a huge, mostly idle mapped staging arena resident.
    auto&upload=r.textureUploads[slot];
    if(r.textureUploadOffsets[slot]>=upload.capacity/4)
        missionEmissionStagingLastBusy[slot]=gPersistentSequence;
    if(upload.resource && upload.capacity>64ull*1024*1024 &&
       gPersistentSequence-missionEmissionStagingLastBusy[slot]>180){
        if(upload.mapped)upload.resource->Unmap(0,nullptr);
        upload.mapped=nullptr;upload.resource.Reset();upload.capacity=0;
    }
}
void releaseMissionEmission(MissionEmissionTexture&entry) {
    retainAcrossFlight(entry.texture.gpu);gTextureDescriptorSlots.retire(entry.texture.descriptorSlot);missionEmissionBytes-=entry.bytes;
}
void trimMissionEmission() {
    if(missionEmissionBytes<=missionEmissionBudget)return;
    std::vector<std::pair<UINT64,std::uint64_t>> cold;
    for(const auto&entry:missionEmissionTextures)
        if(entry.second.lastUse+3<gPersistentSequence)
            cold.emplace_back(entry.second.lastUse,entry.first);
    std::sort(cold.begin(),cold.end());
    for(const auto&candidate:cold){
        if(missionEmissionBytes<=missionEmissionBudget)break;
        const auto found=missionEmissionTextures.find(candidate.second);
        releaseMissionEmission(found->second);missionEmissionTextures.erase(found);
    }
}
void clearMissionEmission() {
    for(auto&entry:missionEmissionTextures)releaseMissionEmission(entry.second);
    missionEmissionTextures.clear();missionEmissionBytes=0;
    missionEmissionAdapter.Reset();missionEmissionAdapterChecked=false;
    missionEmissionNextBudgetQuery=0;
    missionEmissionBudget=missionEmissionLimit=512ull*1024*1024;
    missionEmissionAdapterBudget=1024ull*1024*1024;
    std::fill(std::begin(missionEmissionStagingLastBusy),std::end(missionEmissionStagingLastBusy),0);
}
TextureRecord*missionEmissionTexture(ID3D12Device*device,ID3D12GraphicsCommandList*list,const void*material) {
    if(!material || !missionEmissionDescribe || !missionEmissionRead)return nullptr;
    hwmodern::RasterEmissionSource source;
    if(!missionEmissionDescribe(material,source) || !source.token || !source.width || !source.height)return nullptr;
    auto found=missionEmissionTextures.find(source.token);
    if(found!=missionEmissionTextures.end()){found->second.lastUse=gPersistentSequence;return &found->second.texture;}
    MissionEmissionTexture entry;
    auto&tex=entry.texture;tex.width=source.width;tex.height=source.height;
    tex.minFilter=GL_LINEAR_MIPMAP_LINEAR;tex.generateMips=true;
    if(!missionEmissionRead(source.token,tex.pixels) || tex.pixels.size()!=size_t(source.width)*source.height*4)return nullptr;
    tex.descriptorSlot=gTextureDescriptorSlots.allocate();
    if(!ensureTextureGpu(device,list,tex)){retainAcrossFlight(tex.gpu);gTextureDescriptorSlots.retire(tex.descriptorSlot);return nullptr;}
    // Authoritative texels remain in the shared lossless material store.
    // Discard the temporary expanded copy immediately after its GPU upload.
    std::vector<unsigned char>().swap(tex.pixels);
    const auto description=tex.gpu->GetDesc();entry.bytes=device->GetResourceAllocationInfo(0,1,&description).SizeInBytes;
    entry.lastUse=gPersistentSequence;missionEmissionBytes+=entry.bytes;
    return &missionEmissionTextures.emplace(source.token,std::move(entry)).first->second.texture;
}
void bindMissionEmission(ID3D12Device*device,ID3D12GraphicsCommandList*list,const RasterCommand&command,bool enabled) {
    MissionEmissionParameters p={};
    TextureRecord*active=nullptr,*idle=nullptr;
    if(enabled && command.state.emissionGain>0)active=missionEmissionTexture(device,list,command.state.materialIdentity);
    if(enabled && command.state.engineBlend>=0 && command.state.engineIdleEmissionGain>0)idle=missionEmissionTexture(device,list,command.state.engineIdleMaterial);
    p.flags=(active?1u:0u)|(idle?2u:0u);p.gain=command.state.emissionGain*missionEmissionBrightness;p.idleGain=command.state.engineIdleEmissionGain*missionEmissionBrightness;
    if(!active)p.gain=0;if(!idle)p.idleGain=0;
    for(unsigned i=0;i<2;++i){const auto*tex=i?idle:active;
        const unsigned slot=tex?tex->descriptorSlot:texture(0).descriptorSlot;
        if(!missionEmissionBindingsValid || missionEmissionBoundSlots[i]!=slot){auto srv=r.srvHeap->GetGPUDescriptorHandleForHeapStart();srv.ptr+=UINT64(slot)*r.srvStride;
            list->SetGraphicsRootDescriptorTable(10+i,srv);missionEmissionBoundSlots[i]=slot;}}
    if(!missionEmissionBindingsValid || std::memcmp(&p,&missionEmissionBoundParameters,sizeof(p))){list->SetGraphicsRoot32BitConstants(12,4,&p,0);missionEmissionBoundParameters=p;}
    missionEmissionBindingsValid=true;
}
}
void setEmissionBrightness(float strength) {
    missionEmissionBrightness=std::isfinite(strength)?std::max(0.0f,std::min(strength,2.0f)):1.0f;
}
// Gather every currently needed identity BEFORE eviction, including idle
// engine maps. Camera returns must never discard a texture just before use.
void prepareMissionEmissionResidency(ID3D12Device*device,size_t count) {
    if(!missionLightingEnabled || !missionEmissionDescribe)return;
    if(!missionEmissionAdapterChecked){
        missionEmissionAdapterChecked=true;ComPtr<IDXGIFactory4> factory;
        if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            factory->EnumAdapterByLuid(device->GetAdapterLuid(),IID_PPV_ARGS(&missionEmissionAdapter));
    }
    if(missionEmissionAdapter && gPersistentSequence>=missionEmissionNextBudgetQuery){
        DXGI_QUERY_VIDEO_MEMORY_INFO memory={};
        if(SUCCEEDED(missionEmissionAdapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory))){
            missionEmissionAdapterBudget=memory.Budget;
            const UINT64 reserve=std::max<UINT64>(512ull*1024*1024,memory.Budget/16);
            const UINT64 allowed=memory.Budget>reserve?memory.Budget-reserve:0;
            const UINT64 otherUsage=memory.CurrentUsage>missionEmissionBytes?memory.CurrentUsage-missionEmissionBytes:0;
            missionEmissionLimit=allowed>otherUsage?allowed-otherUsage:0;
        }
        missionEmissionNextBudgetQuery=gPersistentSequence+60;
    }
    UINT64 requiredBytes=0;
    for(size_t i=0;i<count;++i){
        const auto&c=gCommands[i];if(!missionCaster(c))continue;
        const void*ids[]={c.state.emissionGain>0?c.state.materialIdentity:nullptr,
            c.state.engineBlend>=0 && c.state.engineIdleEmissionGain>0?c.state.engineIdleMaterial:nullptr};
        for(const void*id:ids){hwmodern::RasterEmissionSource source;
            if(!id || !missionEmissionDescribe(id,source))continue;
            auto found=missionEmissionTextures.find(source.token);
            if(found!=missionEmissionTextures.end()){
                if(found->second.lastUse!=gPersistentSequence)requiredBytes+=found->second.bytes;
                found->second.lastUse=gPersistentSequence;
            }
        }
    }
    missionEmissionBudget=hwmodern::materialRetentionTarget(requiredBytes,
        missionEmissionLimit,missionEmissionAdapterBudget,true);
    trimMissionEmission();
}
void setEmissionSource(hwmodern::RasterEmissionDescribe describe,hwmodern::RasterEmissionRead read) {
    missionEmissionDescribe=describe;missionEmissionRead=read;
}
