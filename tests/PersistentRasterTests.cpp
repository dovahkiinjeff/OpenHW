// Actual production recorder + D3D12 replay, offscreen. No window/game/settings.
#include "../src/Modern/LegacyD3D12GL.cpp"
#include <iostream>
#include <stdexcept>
#include <d3d12sdklayers.h>
#include "../src/Game/ModernText.h"
#undef min
#undef max
extern "C" void hwTestSetClassicShade(const HWLegacyShadeState *);
extern "C" void hwTestEmitClassicShade(const float[3]);
extern "C" int hwTestCaptureClassicShade(HWLegacyShadeState *);
static void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
static void hr(HRESULT value,const char *message){check(SUCCEEDED(value),message);}
struct V {float xyz[3];int normal;};
struct N {float xyz[3],padding;};
struct P {int normal;unsigned short indices[3],material;float uv[3][2];};
struct Fixture {
    V v[4]={{{-.9f,-.9f,0},2},{{.9f,-.9f,0},3},{{.9f,.9f,0},4},{{-.9f,.9f,0},5}};
    N n[6]={{{0,0,1},0},{{.1f,.2f,.7f},0},{{0,0,1},0},{{.2f,0,.8f},0},{{0,.3f,.4f},0},{{-.5f,0,.5f},0}};
    P p[2]={{0,{0,1,2},0,{{0,0},{1,0},{1,1}}},{1,{0,2,3},0,{{0,0},{1,1},{0,1}}}};
    HWPersistentMeshSource source(){HWPersistentMeshSource s={};s.identity=p;s.vertices=v;s.normals=n;s.polygons=p;
        s.vertexCount=4;s.normalCount=6;s.polygonCount=2;s.vertexStride=sizeof(V);s.normalStride=sizeof(N);s.polygonStride=sizeof(P);
        s.normalIndexOffset=offsetof(V,normal);s.faceNormalOffset=offsetof(P,normal);s.materialOffset=offsetof(P,material);
        for(unsigned c=0;c<3;++c){s.indexOffset[c]=unsigned(offsetof(P,indices)+c*2);s.uvOffset[c]=unsigned(offsetof(P,uv)+c*8);}return s;}
};
static void checkAccounting(ID3D12Device *device) {
    size_t entries=0,staging=0;
    std::vector<const ImmutableRasterGeometry *> seen;
    auto visit=[&](const std::shared_ptr<ImmutableRasterGeometry> &entry){
        if(std::find(seen.begin(),seen.end(),entry.get())!=seen.end())return;
        seen.push_back(entry.get());entries+=entry->charge;
        if(entry->vertices)check(entry->vertexAllocationBytes==immutableAllocationBytes(device,entry->vertices.Get()),"vertex allocation charge differs from real allocation");
        if(entry->indices)check(entry->indexAllocationBytes==immutableAllocationBytes(device,entry->indices.Get()),"index allocation charge differs from real allocation");
        if(entry->vertices&&entry->indices&&entry->data.vertices.empty()&&entry->data.indices.empty())
            check(entry->charge==entry->vertexAllocationBytes+entry->indexAllocationBytes,"warm cache still charges discarded CPU/staging reservation");
    };
    for(const auto &mesh:gImmutableMeshes)for(const auto &entry:mesh.second.geometry)visit(entry.second);
    for(const auto &command:gCommands)if(command.immutableGeometry)visit(command.immutableGeometry);
    for(unsigned slot=0;slot<kFrameSlots;++slot) {
        for(const auto &entry:r.geometryFlight[slot])visit(entry);
        for(const auto &ticket:r.geometryStagingFlight[slot]){
            check(ticket->charge==immutableAllocationBytes(device,ticket->resource.Get()),"staging ticket charge differs from real allocation");
            staging+=ticket->charge;
        }
    }
    check(staging==gPersistentGeometryStagingCharge,"staging ownership charge mismatch");
    check(entries+staging==gPersistentGeometryCharge,"live/reserved geometry accounting mismatch");
    check(gPersistentGeometryCharge<=kPersistentGeometryBudget,"geometry cache exceeded its bound");
}
static std::vector<unsigned char> draw(ID3D12Device *device,ID3D12CommandQueue *queue,
    const HWPersistentMeshSource &source,Fixture &f,const HWLegacyShadeState &shade,
    bool persistent,bool smooth,unsigned scenario,unsigned long long &uploaded) {
    // The test waits for each submission before reusing this slot, matching
    // the production presenter's existing slot-fence ownership contract.
    {
        // Isolate staging retirement from independently valid retirement of
        // unregistered geometry whose final owner was this completed slot.
        const auto retainedFlight=r.geometryFlight[0];
        const auto beforeRetirement=gPersistentGeometryCharge,stagingBefore=gPersistentGeometryStagingCharge;
        hwlegacyd3d12::beginFrame(0);
        check(gPersistentGeometryStagingCharge==0 && gPersistentGeometryCharge==beforeRetirement-stagingBefore,
            "slot retirement did not release exactly its fenced staging charge");
    }
    hwglBeginFrame(32,32,0);
    hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglMatrixMode(GL_MODELVIEW);hwglLoadIdentity();
    hwglTranslatef(.03f,-.02f,0);hwglScalef(.9f,.8f,1);
    hwglDisable(GL_LIGHTING);hwglDisable(GL_CULL_FACE);hwglDisable(GL_FOG);hwglDisable(GL_CLIP_PLANE0);hwglDisable(GL_BLEND);
    hwglDisable(GL_TEXTURE_2D);g.draw.engineBlend=-1;
    if(scenario==1){const double plane[]={1,0,0,0};hwglClipPlane(GL_CLIP_PLANE0,plane);hwglEnable(GL_CLIP_PLANE0);}
    if(scenario==2){hwglEnable(GL_BLEND);hwglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
    if(scenario==4){
        const unsigned idle[]={0xff5030d0u};
        hwglBindTexture(GL_TEXTURE_2D,77);hwglTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,idle);
        hwglCaptureEngineIdle(.35f);
    }
    if(scenario==4||scenario==5){
        const unsigned pixels[]={0xff604020u,0xff20a040u,0xffe03090u,0xffd09050u};
        hwglBindTexture(GL_TEXTURE_2D,78);hwglTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        hwglEnable(GL_TEXTURE_2D);
    }
    if(scenario>=10) {
        hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglMatrixMode(GL_MODELVIEW);hwglLoadIdentity();
        hwglDisable(GL_DEPTH_TEST);hwglDisable(GL_SCISSOR_TEST);
        if(scenario>10) {
            const unsigned hull[]={0xff5030d0u};
            hwglBindTexture(GL_TEXTURE_2D,991);hwglTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,hull);
            hwglCaptureEngineIdle(scenario==11?0.0f:0.6f);
        }
        check(hwModernTextDraw("regular",16,2,2,0xffd0e0ffu,"AB",2)!=0,"production UI text draw failed");
    } else {
    hwTestSetClassicShade(&shade);HWLegacyShadeState captured{};check(hwTestCaptureClassicShade(&captured)!=0,"shade getter");
    hwglBegin(GL_TRIANGLES);
    if(persistent){
        check(hwglDrawImmutableMesh(source.identity,0,smooth?3u:1u,&captured)==2,"persistent draw admission");
        check(gFrameVertices.empty()&&gFrameIndices.empty(),"persistent draw decoded/copied transient vertices");
    }else for(unsigned pi=0;pi<2;++pi)for(unsigned c=0;c<3;++c){
        const auto vi=f.p[pi].indices[c];const auto ni=smooth?f.v[vi].normal:f.p[pi].normal;
        hwTestEmitClassicShade(f.n[ni].xyz);hwglTexCoord2f(f.p[pi].uv[c][0],f.p[pi].uv[c][1]);
        hwglVertex3fv(f.v[vi].xyz);
    }
    hwglEnd();
    }
    ComPtr<ID3D12CommandAllocator> allocator;hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
    ComPtr<ID3D12GraphicsCommandList> list;hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=32;desc.Height=32;desc.DepthOrArraySize=1;desc.MipLevels=1;
    desc.Format=kRasterTargetFormat;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> target;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&target)),"target");
    check(hwlegacyd3d12::renderFrameSnapshots(device,list.Get(),target.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE,32,32,gCommands.size(),nullptr,0),"production replay");
    checkAccounting(device);
    uploaded=gPersistentUploadBytes;
    if(persistent && uploaded && r.geometryAllocator.active()) {
        const auto &entry=gCommands.back().immutableGeometry;
        check(entry->charge==128u*1024u && gPersistentGeometryStagingCharge==128u*1024u,
            "tiny entry must charge 128 KiB live GPU plus 128 KiB pending staging, not 384 KiB forever");
    }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;UINT64 bytes=0;
    device->GetCopyableFootprints(&desc,0,1,0,&layout,nullptr,nullptr,&bytes);
    heap.Type=D3D12_HEAP_TYPE_READBACK;desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> output;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&output)),"readback");
    D3D12_TEXTURE_COPY_LOCATION dst={},src={};dst.pResource=output.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=layout;
    src.pResource=target.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);hr(list->Close(),"close");ID3D12CommandList *lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);hr(queue->Signal(fence.Get(),1),"signal");hr(fence->SetEventOnCompletion(1,event),"event");check(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"headless completion");CloseHandle(event);
    void *mapped=nullptr;hr(output->Map(0,nullptr,&mapped),"read map");
    std::vector<unsigned char> pixels(32*32*8);
    for(unsigned y=0;y<32;++y)std::memcpy(pixels.data()+y*32*8,static_cast<unsigned char *>(mapped)+layout.Offset+y*layout.Footprint.RowPitch,32*8);
    output->Unmap(0,nullptr);hwglEndFrame();return pixels;
}
struct EffectGuideResult { std::vector<unsigned char> color; std::vector<float> depth; };
static EffectGuideResult drawEffectGuides(ID3D12Device *device, ID3D12CommandQueue *queue,
                                         bool guides, unsigned size=64, bool cores=false, float background=.1f) {
    hwlegacyd3d12::setFrameGenerationGuidesEnabled(guides);
    hwlegacyd3d12::beginFrame(0);hwglBeginFrame(size,size,0);
    check(!hwlegacyd3d12::frameGenerationEffectDepthResource(),"stale FX guide before current replay");
    auto perspective=[](){hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglFrustum(-1,1,-1,1,1,1000);hwglMatrixMode(GL_MODELVIEW);hwglLoadIdentity();};
    perspective();hwglViewport(0,0,size,size);hwglDisable(GL_CULL_FACE);hwglDisable(GL_FOG);hwglDisable(GL_LIGHTING);
    hwglDisable(GL_TEXTURE_2D);hwglDisable(GL_ALPHA_TEST);hwglDisable(GL_CLIP_PLANE0);hwglDisable(GL_SCISSOR_TEST);
    hwglEngineGlow(0,0,0,0,0);g.draw.engineBlend=-1;
    hwglEnable(GL_DEPTH_TEST);hwglDepthFunc(GL_LESS);hwglDepthMask(GL_TRUE);hwglDisable(GL_BLEND);
    float red=.6f, green=.4f, blue=.2f;
    auto rect=[&](float u,float v,float z,float alpha,bool isPerspective=true){
        const float scale=isPerspective?z:1;hwglColor4f(red,green,blue,alpha);hwglBegin(GL_QUADS);
        for(const auto xy:std::array<std::array<float,2>,4>{{{{u-.09f,v-.09f}},{{u+.09f,v-.09f}},{{u+.09f,v+.09f}},{{u-.09f,v+.09f}}}})
            hwglVertex3f((xy[0]*2-1)*scale,(xy[1]*2-1)*scale,-z);
        hwglEnd();
    };
    if (cores) {
        red=background; green=1-background; blue=.8f;
        for(float u:{.125f,.375f,.625f}) rect(u,.25f,8,1);
        rect(.875f,.25f,2,1);
        rect(.125f,.75f,8,1);rect(.625f,.75f,8,1);rect(.875f,.75f,8,1);
        red=.6f; green=.4f; blue=.2f;
        hwglEnable(GL_BLEND);hwglDepthMask(GL_FALSE);
        hwglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        hwglEngineTrail(TRUE);
        rect(.125f,.25f,4,1);rect(.375f,.25f,4,.96f);
        rect(.625f,.25f,6,1);rect(.625f,.25f,4,1);rect(.625f,.25f,7,1);
        rect(.875f,.25f,5,1); // Dense core still obeys real scene depth.
        rect(.125f,.75f,4,.5f); // Tail must still reveal background smoothly.
        // Glows keep the original additive/angular appearance; only trails
        // use the covering-core tag and source-over blend.
        hwglEngineTrail(FALSE);hwglBlendFunc(GL_SRC_ALPHA,GL_ONE);
        hwglNormal3f(0,0,1);hwglEngineGlow(.7f,.3f,.1f,160.f/255.f,1);
        rect(.875f,.75f,4,1); // Real production capital glow fragment shader.
        hwglEngineGlow(0,0,0,0,0);
        hwglEngineTrail(TRUE);hwglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        // Missiles use the native line PSO, rather than the ship-ribbon triangle
        // PSO. Check its sampled coverage, color and signed view-depth as well.
        auto missileLine=[&](float row,float alpha){
            hwglColor4f(.6f,.4f,.2f,alpha);hwglBegin(GL_LINES);
            hwglVertex3f((.54f*2-1)*4,(row*2-1)*4,-4);
            hwglVertex3f((.71f*2-1)*4,(row*2-1)*4,-4);hwglEnd();
        };
        missileLine(48.5f/64,1);missileLine(51.5f/64,.5f);
        missileLine(15.5f/64,1); // In front of the opaque hull at z=8.
        hwglColor4f(.6f,.4f,.2f,1);hwglBegin(GL_LINES);
        hwglVertex3f((.80f*2-1)*5,(15.5f/64*2-1)*5,-5);
        hwglVertex3f((.94f*2-1)*5,(15.5f/64*2-1)*5,-5);hwglEnd(); // Behind z=2.
        hwglEngineTrail(FALSE);
        rect(.375f,.75f,4,1); // Scope ends: generic FX never gets core metadata.
        hwglMarkSnapshot(HW_LEGACY_RASTER_WORLD);
    } else {
    rect(.875f,.25f,2,1); // An opaque occluder must reject the hidden FX at z=5.
    hwglEnable(GL_BLEND);hwglDepthMask(GL_FALSE);hwglBlendFunc(GL_SRC_ALPHA,GL_ONE);
    rect(.125f,.25f,5,.5f);rect(.375f,.25f,4,0);
    rect(.625f,.25f,3,.5f);rect(.625f,.25f,8,.5f);rect(.875f,.25f,5,.5f);
    hwglBlendFunc(GL_ONE,GL_ONE);rect(.125f,.75f,6,0); // Emission ignores source alpha.
    hwglBlendFunc(GL_SRC_ALPHA,GL_ONE);
    hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglOrtho(-1,1,-1,1,1,1000);hwglMatrixMode(GL_MODELVIEW);
    rect(.625f,.75f,4,.5f,false); // Ortho is not finite 3D FX, even before world end.
    perspective();hwglMarkSnapshot(HW_LEGACY_RASTER_WORLD);
    rect(.375f,.75f,1,.8f); // Deliberately perspective UI: only the world boundary excludes it.
    }
    hwglMarkSnapshot(HW_LEGACY_RASTER_FINAL);
    ComPtr<ID3D12CommandAllocator> allocator;hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"FX allocator");
    ComPtr<ID3D12GraphicsCommandList> list;hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"FX list");
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=size;desc.Height=size;
    desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=kRasterTargetFormat;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> target;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&target)),"FX target");
    check(hwlegacyd3d12::renderFrameSnapshots(device,list.Get(),target.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE,size,size,gCommands.size(),nullptr,0),"FX replay");
    auto *effect=hwlegacyd3d12::frameGenerationEffectDepthResource();check(bool(effect)==guides,"FX guide presence");
    struct Readback { ComPtr<ID3D12Resource> buffer; D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout; unsigned bytesPerPixel; };
    auto copy=[&](ID3D12Resource *input,unsigned bpp){
        Readback output;output.bytesPerPixel=bpp;const auto d=input->GetDesc();UINT64 bytes=0;
        device->GetCopyableFootprints(&d,0,1,0,&output.layout,nullptr,nullptr,&bytes);
        D3D12_HEAP_PROPERTIES readHeap={};readHeap.Type=D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC read={};read.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;read.Width=bytes;read.Height=1;
        read.DepthOrArraySize=read.MipLevels=1;read.SampleDesc.Count=1;read.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr(device->CreateCommittedResource(&readHeap,D3D12_HEAP_FLAG_NONE,&read,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&output.buffer)),"FX readback");
        D3D12_TEXTURE_COPY_LOCATION src={},dst={};src.pResource=input;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource=output.buffer.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=output.layout;
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);return output;
    };
    auto color=copy(target.Get(),8);Readback depth;
    if(effect){resourceBarrier(list.Get(),effect,r.effectDepthState,D3D12_RESOURCE_STATE_COPY_SOURCE);
        depth=copy(effect,4);resourceBarrier(list.Get(),effect,D3D12_RESOURCE_STATE_COPY_SOURCE,r.effectDepthState);}
    hr(list->Close(),"FX close");ID3D12CommandList *lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"FX fence");
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);hr(queue->Signal(fence.Get(),1),"FX signal");hr(fence->SetEventOnCompletion(1,event),"FX event");
    check(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"FX completion");CloseHandle(event);
    auto unpack=[&](Readback &input){std::vector<unsigned char> bytes(size*size*input.bytesPerPixel);void *mapped=nullptr;
        hr(input.buffer->Map(0,nullptr,&mapped),"FX map");for(unsigned y=0;y<size;++y)
            std::memcpy(bytes.data()+size*y*input.bytesPerPixel,static_cast<unsigned char*>(mapped)+input.layout.Offset+y*input.layout.Footprint.RowPitch,size*input.bytesPerPixel);
        input.buffer->Unmap(0,nullptr);return bytes;};
    EffectGuideResult result;result.color=unpack(color);
    if(effect){const auto bytes=unpack(depth);result.depth.resize(size*size);std::memcpy(result.depth.data(),bytes.data(),bytes.size());}
    hwglEndFrame();return result;
}
static void checkEffectGuides(ID3D12Device *device,ID3D12CommandQueue *queue) {
    const auto off=drawEffectGuides(device,queue,false),on=drawEffectGuides(device,queue,true);
    check(!hwlegacyd3d12::frameGenerationEffectDepthFailed(),"healthy FX guide latched failure");
    check(off.color==on.color,"FX guidance changed original color pixels");
    auto depth=[&](float u,float v){return on.depth[unsigned(v*64)*64+unsigned(u*64)];};
    if (std::fabs(depth(.125f,.25f)-5)>=.0001f) {
        for(float v:{.25f,.75f}){for(float u:{.125f,.375f,.625f,.875f})std::cerr<<"FX depth("<<u<<","<<v<<")="<<depth(u,v)<<" ";std::cerr<<'\n';}
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)if(on.depth[y*64+x]<FLT_MAX){std::cerr<<"first finite FX="<<x<<","<<y<<":"<<on.depth[y*64+x]<<'\n';y=64;break;}
    }
    check(std::fabs(depth(.125f,.25f)-5)<.0001f,"FX linear depth/orientation mismatch");
    check(depth(.375f,.25f)==FLT_MAX,"zero-alpha billboard wrote rectangular guide");
    check(std::fabs(depth(.625f,.25f)-3)<.0001f,"FX MIN blend did not keep nearest visible fragment");
    check(depth(.875f,.25f)==FLT_MAX,"opaque depth test did not occlude FX guide");
    check(std::fabs(depth(.125f,.75f)-6)<.0001f,"GL_ONE emission without alpha lost its guide");
    check(depth(.375f,.75f)==FLT_MAX,"UI after world boundary contaminated FX guide");
    check(depth(.625f,.75f)==FLT_MAX,"orthographic content contaminated FX guide");
    const auto coreA=drawEffectGuides(device,queue,true,64,true,.1f);
    const auto coreB=drawEffectGuides(device,queue,true,64,true,.9f);
    const auto coreNoGuide=drawEffectGuides(device,queue,false,64,true,.1f);
    check(coreA.color==coreNoGuide.color,"core color depends on optional guide capture");
    auto index=[](float u,float v){return unsigned(v*64)*64+unsigned(u*64);};
    auto sameColor=[&](float u,float v){unsigned i=index(u,v)*8;
        return std::memcmp(coreA.color.data()+i,coreB.color.data()+i,8)==0;};
    check(sameColor(.125f,.25f),"dense ribbon core inherited background hull color");
    check(!sameColor(.875f,.75f),"original additive engine glow was changed into a covering core");
    check(!sameColor(.125f,.75f),"transparent tail no longer blends with background");
    check(std::fabs(coreA.depth[index(.125f,.25f)]+250000.f)<.1f,
        "covering ribbon core not encoded at actual view depth");
    check(std::fabs(coreA.depth[index(.375f,.25f)]+250000.f)<.1f,
        "95-percent coverage threshold rejected dense core");
    check(std::fabs(coreA.depth[index(.625f,.25f)]+250000.f)<.1f,
        "signed inverse MIN blending did not retain nearest core");
    check(coreA.depth[index(.875f,.25f)]==FLT_MAX,"covering core rendered through foreground hull");
    check(std::fabs(coreA.depth[index(.125f,.75f)]-4)<.0001f,
        "transparent tail incorrectly owns foreground depth");
    check(std::fabs(coreA.depth[index(.375f,.75f)]-4)<.0001f,
        "engine tag leaked into generic transparent effects");
    check(std::fabs(coreA.depth[index(.875f,.75f)]-4)<.0001f,
        "original engine glow incorrectly marked as a covering trail core");
    check(sameColor(.625f,48.5f/64),"missile line core inherited background hull color");
    check(!sameColor(.625f,51.5f/64),"missile line tail lost its transparent fade");
    check(std::fabs(coreA.depth[index(.625f,48.5f/64)]+250000.f)<.1f,
        "native missile line core missing finite foreground guide");
    check(coreA.depth[index(.875f,15.5f/64)]==FLT_MAX,"missile line drew through nearer hull");
    check(std::fabs(coreA.depth[index(.625f,51.5f/64)]-4)<.0001f,
        "missile line tail incorrectly marked as covering foreground");
    std::cout<<"PASS: native missile line core color/guide, transparent tail and foreground occlusion.\n";
    std::cout<<"PASS: engine core background-color independence, tail fade, foreground occlusion, nearest core encoding, original additive glow and scoped tag.\n";
    const auto resized=drawEffectGuides(device,queue,true,32);check(resized.depth.size()==32*32,"FX resize failed");
    drawEffectGuides(device,queue,false);check(!r.effectDepth&&!hwlegacyd3d12::frameGenerationEffectDepthResource(),"FX disable did not retire optional storage");
    for(unsigned slot=0;slot<kFrameSlots;++slot)hwlegacyd3d12::releaseFrameResources(slot);
    r.effectDepthFailed=true;hwlegacyd3d12::setFrameGenerationGuidesEnabled(true);
    check(!hwlegacyd3d12::frameGenerationEffectDepthFailed(),"explicit enable did not clear FX failure latch");
    hwlegacyd3d12::setFrameGenerationGuidesEnabled(false);
    std::cout<<"PASS: frame-generation FX MRT byte-identical base color, finite depth/orientation, alpha rejection, MIN selection, occlusion, GL_ONE emission, UI/ortho exclusion, stale validity and fenced resize/disable.\n";
}
int main()try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    hwglResetAll();Fixture f;auto source=f.source();hwglRegisterImmutableMesh(&source);
    check(gImmutableMeshes.count(source.identity)==1,"immutable registration");
    const auto compact=hwmodern::buildPersistentRaster(source,0,2,true);
    check(compact.vertices.size()==4&&compact.indices.size()==6,"binary remap shared vertices");
    const unsigned expected[]={0,1,2,0,2,3};
    for(unsigned i=0;i<6;++i)check(compact.indices[i]==expected[i],"triangle/corner order changed");
    ComPtr<IDXGIFactory6> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    for(unsigned i=0;!device;++i){
        adapter.Reset();
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 description={};hr(adapter->GetDesc1(&description),"adapter description");
        if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)))){
            std::wcout<<L"GPU: "<<description.Description<<L" (vendor "<<std::hex<<description.VendorId<<L", device "<<description.DeviceId<<std::dec<<L")\n";
        }
    }
    check(device!=nullptr,"D3D12 high-performance hardware device");
    D3D12_COMMAND_QUEUE_DESC qd={};ComPtr<ID3D12CommandQueue> queue;hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
    check(hwlegacyd3d12::initialize(device.Get(),32,32),"renderer initialize");
    unsigned long long firstUpload=0;
    for(unsigned smooth=0;smooth<2;++smooth)for(unsigned scenario=0;scenario<10;++scenario){
        HWLegacyShadeState shade={};shade.base[0]=.03f;shade.base[1]=.12f;shade.base[2]=.24f;shade.base[3]=scenario==2?.27f:.83f;
        shade.direction[0][0]=.25f;shade.direction[0][2]=.93f;shade.direction[1][1]=-.4f;shade.direction[1][2]=.73f;
        shade.diffuse[0][0]=.57f;shade.diffuse[0][1]=.21f;shade.diffuse[0][2]=.39f;
        shade.diffuse[1][0]=.18f;shade.diffuse[1][1]=.62f;shade.diffuse[1][2]=.31f;
        shade.parameters[0]=1;shade.parameters[1]=scenario==0?1.0f:2.0f;
        if(scenario==3){shade.dock[0]=.8f;shade.dock[1]=.2f;shade.dock[2]=.1f;shade.dock[3]=.75f;}
        if(scenario==6)shade.parameters[1]=0;
        if(scenario==7){shade.base[0]=-.1f;shade.base[1]=.99f;shade.diffuse[1][1]=2.0f;}
        if(scenario==8){shade.direction[0][2]=-.93f;shade.direction[1][2]=-.73f;}
        if(scenario==9){shade.dock[0]=.3f;shade.dock[1]=.7f;shade.dock[2]=1.0f;shade.dock[3]=1.0f;}
        unsigned long long ignored=0,uploaded=0;
        const auto oldPixels=draw(device.Get(),queue.Get(),source,f,shade,false,smooth!=0,scenario,ignored);
        const auto newPixels=draw(device.Get(),queue.Get(),source,f,shade,true,smooth!=0,scenario,uploaded);
        check(oldPixels==newPixels,"GPU rendered pixels differ from actual unchanged classic CPU shading");
        check(std::any_of(newPixels.begin(),newPixels.end(),[](unsigned char x){return x!=0;}),"test rendered nothing");
        if(scenario==0){check(uploaded>0,"first GPU geometry was not uploaded");firstUpload+=uploaded;}
        else check(uploaded==0,"repeat frame uploaded immutable vertex/index geometry");
    }
    {
        HWLegacyShadeState textShade={};unsigned long long uploaded=0;
        const auto clean=draw(device.Get(),queue.Get(),source,f,textShade,false,false,10,uploaded);
        check(std::any_of(clean.begin(),clean.end(),[](unsigned char x){return x!=0;}),"GDI text test drew nothing");
        const auto idleHull=draw(device.Get(),queue.Get(),source,f,textShade,false,false,11,uploaded);
        check(clean==idleHull,"sensor text inherited the last visible wreck's idle hull texture");
        const auto fadingHull=draw(device.Get(),queue.Get(),source,f,textShade,false,false,12,uploaded);
        check(clean==fadingHull,"sensor text inherited a fading engine material");
        check(g.draw.engineBlend<0,"text left ship engine blending active");
        hwModernTextReset();
        std::cout<<"PASS: actual Windows text rasterization and D3D12 pixels independent of inherited idle/fading hull material.\n";
    }
    check(r.pipelines.size()>1 && r.legacyShaderCompilations==2,
        "invariant shader pair was recompiled for different fixed-state PSOs");
    const auto warmCharge=gPersistentGeometryCharge;
    check(gPersistentGeometryStagingCharge==0,"completed old staging was not retired with its slot");
    // Force the existing exact UPLOAD-heap fallback without exhausting memory
    // or requesting an invalid GPU allocation. Resource leases keep old MA
    // allocations alive while this wrapper is temporarily unavailable.
    Fixture fallbackFixture;auto fallbackSource=fallbackFixture.source();hwglRegisterImmutableMesh(&fallbackSource);
    r.geometryAllocator.reset();
    HWLegacyShadeState fallbackShade={};fallbackShade.parameters[0]=1;
    fallbackShade.base[0]=fallbackShade.base[1]=fallbackShade.base[2]=fallbackShade.base[3]=1;
    unsigned long long ignored=0,fallbackUpload=0;
    const auto fallbackCpu=draw(device.Get(),queue.Get(),fallbackSource,fallbackFixture,fallbackShade,false,true,0,ignored);
    const auto fallbackGpu=draw(device.Get(),queue.Get(),fallbackSource,fallbackFixture,fallbackShade,true,true,0,fallbackUpload);
    check(fallbackCpu==fallbackGpu && fallbackUpload>0,"persistent upload-heap fallback pixels changed");
    const auto &fallbackEntry=gImmutableMeshes[fallbackSource.identity].geometry.begin()->second;
    D3D12_HEAP_PROPERTIES fallbackHeap={};D3D12_HEAP_FLAGS fallbackFlags={};
    hr(fallbackEntry->vertices->GetHeapProperties(&fallbackHeap,&fallbackFlags),"fallback heap query");
    check(fallbackHeap.Type==D3D12_HEAP_TYPE_UPLOAD && gPersistentGeometryStagingCharge==0,
        "upload fallback was double charged as separate staging");
    hwglForgetImmutableMesh(fallbackSource.identity);
    hr(r.geometryAllocator.initialize(device.Get()),"restore allocator after fallback test");
    draw(device.Get(),queue.Get(),source,f,fallbackShade,true,true,0,ignored);
    check(gPersistentGeometryCharge==warmCharge,"fallback generation retained released charges");
    auto oldEntry=gImmutableMeshes.at(source.identity).geometry.at(3u);
    const auto evictionsBefore=gPersistentGeometryEvictions;
    check(reserveImmutableGeometry(kPersistentGeometryBudget-oldEntry->charge),"safe inactive cache eviction failed");
    check(gPersistentGeometryCharge==oldEntry->charge && gPersistentGeometryEvictions>evictionsBefore,
        "cache did not release only inactive resource charges");
    check(!reserveImmutableGeometry(kPersistentGeometryBudget),"cache evicted GPU/command-pinned geometry");
    check(hwlegacyd3d12::cacheStats().cacheChargeBytes==gPersistentGeometryCharge,
        "diagnostic getter does not report live charge");
    // Unregister/reuse the same address: new source generation must rebuild.
    hwglForgetImmutableMesh(source.identity);f.v[0].xyz[0]=-.4f;hwglRegisterImmutableMesh(&source);
    check(gImmutableMeshes[source.identity].geometry.empty(),"pointer reuse retained old geometry");
    check(oldEntry->vertices&&oldEntry->data.vertices.empty(),"GPU lifetime lost or duplicate CPU vertices retained");
    auto invalidSource=source;invalidSource.vertices=nullptr;
    hwglRegisterImmutableMesh(&invalidSource);
    check(gImmutableMeshes.count(source.identity)==0,"invalid re-registration retained old generation");
    HWLegacyShadeState shade={};shade.parameters[0]=1;check(hwglDrawImmutableMesh(source.identity,0,3,&shade)==0,"mutable/unregistered fallback");
    hwglResetAll();
    hwlegacyd3d12::releaseFrameResources(0); // draw() completed its real fence.
    check(gPersistentGeometryCharge==oldEntry->charge,"mutation/reset prematurely released live geometry ownership");
    oldEntry.reset();
    check(gPersistentGeometryCharge==0 && gPersistentGeometryStagingCharge==0,"geometry charge leaked or underflowed after all owners retired");
    checkEffectGuides(device.Get(),queue.Get());
    ComPtr<ID3D12InfoQueue> info;
    if(SUCCEEDED(device.As(&info)))for(UINT64 index=0;index<info->GetNumStoredMessages();++index){
        SIZE_T bytes=0;info->GetMessage(index,nullptr,&bytes);std::vector<unsigned char> storage(bytes);
        auto *message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());info->GetMessage(index,message,&bytes);
        if(message->Severity==D3D12_MESSAGE_SEVERITY_ERROR||message->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION){std::cerr<<message->pDescription<<'\n';check(false,"D3D12 debug validation error");}
    }
    hwlegacyd3d12::shutdown();
    std::cout<<"PASS actual D3D12 offscreen production shader parity: 20 cases (smooth/flat, zero/one/two lights, dock tint, upper clamp, negative color/light, alpha, actual two-texture engine blend, UV, clip); first geometry upload="<<firstUpload<<" bytes, repeated uploads=0; pointer invalidation, fence ownership and CPU-copy retirement checked.\n";
    std::cout<<"PASS: real MA/committed allocation accounting, fenced staging retirement, upload-heap fallback pixel parity, pinned eviction/reset lifetime and shader pair compiled once across multiple PSOs; warm charge="<<warmCharge<<" bytes.\n";
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
