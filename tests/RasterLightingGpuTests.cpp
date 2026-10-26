// Exercise the production native renderer on an offscreen target; no game,
// window, player settings or installed game assets are used.
#ifdef HW_RASTER_BASELINE_SOURCE
#include HW_RASTER_BASELINE_SOURCE
#else
#include "../src/Modern/LegacyD3D12GL.cpp"
#endif
#include <d3d12sdklayers.h>
#include "ResolvedSceneLighting.h"
#include <iostream>
#include <stdexcept>
static void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
static float halfFloat(unsigned short v){return std::ldexp(float((v&1023)+((v&0x7c00)?1024:0)),(v&0x7c00)?int((v>>10)&31)-25:-24)*(v&0x8000?-1:1);}
namespace emissionFixture {
std::unordered_map<const void*,hwmodern::RasterEmissionSource> materials;
std::unordered_map<std::uint64_t,std::vector<unsigned>> pixels;
unsigned reads=0;
bool describe(const void*id,hwmodern::RasterEmissionSource&out){const auto found=materials.find(id);if(found==materials.end())return false;out=found->second;return true;}
bool read(std::uint64_t token,std::vector<unsigned char>&out){const auto found=pixels.find(token);if(found==pixels.end())return false;++reads;out.resize(found->second.size()*4);std::memcpy(out.data(),found->second.data(),out.size());return true;}
}
int main(int argc,char**argv)try {
    const bool parity=argc>1 && !std::strcmp(argv[1],"parity");
    const bool dense=argc>1 && !std::strcmp(argv[1],"dense");
    const bool regression=argc>1 && !std::strcmp(argv[1],"shadow-regression");
    const bool filter=argc>1 && !std::strcmp(argv[1],"shadow-filter");
    const bool uploadFleet=argc>1 && !std::strcmp(argv[1],"fleet-upload");
    const bool fleet=regression || uploadFleet || (argc>1 && !std::strcmp(argv[1],"fleet"));
    const bool overdraw=argc>1 && !std::strcmp(argv[1],"overdraw");
    const bool noShadows=argc>2 && !std::strcmp(argv[2],"no-shadows");
    const bool stress=filter || dense || fleet || overdraw || (argc>1 && !std::strcmp(argv[1],"stress"));
    const bool performance=stress && std::getenv("HW_GPU_PERFORMANCE_TEST");
    ComPtr<ID3D12Debug> debug;if(!performance && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;require(SUCCEEDED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))),"factory");
    ComPtr<IDXGIAdapter1> adapter;require(SUCCEEDED(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))),"adapter");
    ComPtr<ID3D12Device> device;require(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"device");
    ComPtr<ID3D12InfoQueue> info;device.As(&info);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q={};require(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"queue");
    ComPtr<ID3D12CommandAllocator> allocator;require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))),"allocator");
    ComPtr<ID3D12GraphicsCommandList> list;require(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))),"commands");list->Close();
    ComPtr<ID3D12Fence> fence;device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    const unsigned width=stress?1280:128,height=stress?720:128;require(hwlegacyd3d12::initialize(device.Get(),width,height),"native initialize");
    ComPtr<ID3D12QueryHeap> timestamps;D3D12_QUERY_HEAP_DESC queryDesc={};queryDesc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;queryDesc.Count=2;
    require(SUCCEEDED(device->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&timestamps))),"timestamps");
    UINT64 frequency=0;queue->GetTimestampFrequency(&frequency);double gpuMilliseconds=0,cpuMilliseconds=0;
    D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;d.DepthOrArraySize=d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> color;
    require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_SOURCE,nullptr,IID_PPV_ARGS(&color))),"color target");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};UINT64 bytes=0;device->GetCopyableFootprints(&d,0,1,0,&footprint,nullptr,nullptr,&bytes);
    heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC buffer={};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> read;require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&read))),"readback");
    buffer.Width=16;ComPtr<ID3D12Resource> times;require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&times))),"timestamp readback");
    ComPtr<ID3D12Resource> shadowRead;D3D12_PLACED_SUBRESOURCE_FOOTPRINT shadowFootprint={};UINT64 shadowBytes=0;unsigned long long shadowHash=1469598103934665603ull;
    if(regression && !noShadows){D3D12_RESOURCE_DESC shadowDesc=d;shadowDesc.Width=shadowDesc.Height=2048;shadowDesc.Format=DXGI_FORMAT_R32_TYPELESS;shadowDesc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        device->GetCopyableFootprints(&shadowDesc,0,1,0,&shadowFootprint,nullptr,nullptr,&shadowBytes);buffer.Width=shadowBytes;
        require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&shadowRead))),"shadow readback");}
    std::vector<hwmodern::ResolvedSceneLight> lights(2);
    lights[0].type=1;lights[0].direction[0]=0.5f;lights[0].direction[2]=1;lights[0].color[0]=1;lights[0].intensity=1.35f;
    lights[1].type=0;lights[1].color[0]=lights[1].color[1]=lights[1].color[2]=0.03f;lights[1].intensity=1;
    if(stress){lights.resize(512);for(unsigned i=2;i<512;++i){auto&l=lights[i];l.type=2;l.position[0]=100+float(i);l.radius=1;l.intensity=1;l.color[0]=l.color[1]=l.color[2]=1;}}
    if(dense)for(unsigned i=2;i<512;++i){auto&l=lights[i];l.position[0]=float(i%16)/8-1;l.position[1]=float(i/16)/16-1;l.position[2]=.5f;l.radius=2;l.intensity=.001f;}
    if(overdraw){lights.resize(24);for(unsigned i=2;i<24;++i){auto&l=lights[i];l.type=2;l.position[0]=float(i%6)/3-1;l.position[1]=float(i/6)/2-1;l.position[2]=1;l.radius=3;l.intensity=.04f;l.color[0]=l.color[1]=l.color[2]=1;}}
    struct Vertex {float p[3];int normal;};struct Polygon {int face;unsigned short a,b,c,material;float uv[6];};
    const Vertex vertices[]={{{-.05f,-.4f,.25f},0},{{.05f,-.4f,.25f},0},{{.05f,.4f,.25f},0},{{-.05f,.4f,.25f},0}};
    const float normals[]={0,0,1};const Polygon polygons[]={{0,0,1,2,0,{0,0,1,0,1,1}},{0,0,2,3,0,{0,0,1,1,0,1}}};
    static int meshIdentity;HWPersistentMeshSource source={};source.identity=&meshIdentity;source.vertices=vertices;source.normals=normals;source.polygons=polygons;
    source.vertexCount=4;source.normalCount=1;source.polygonCount=2;source.vertexStride=sizeof(Vertex);source.normalStride=12;source.polygonStride=sizeof(Polygon);
    source.normalIndexOffset=offsetof(Vertex,normal);source.faceNormalOffset=offsetof(Polygon,face);source.materialOffset=offsetof(Polygon,material);
    source.indexOffset[0]=offsetof(Polygon,a);source.indexOffset[1]=offsetof(Polygon,b);source.indexOffset[2]=offsetof(Polygon,c);
    for(unsigned i=0;i<3;++i)source.uvOffset[i]=offsetof(Polygon,uv)+i*8;hwglRegisterImmutableMesh(&source);
    HWLegacyShadeState shade={};shade.base[0]=shade.base[1]=shade.base[2]=.2f;shade.base[3]=1;
    shade.diffuse[0][0]=shade.diffuse[0][1]=shade.diffuse[0][2]=.8f;shade.direction[0][2]=1;shade.parameters[0]=shade.parameters[1]=1;
    GLuint cutoutTexture=0;
    if(regression){const unsigned char pixels[]={255,255,255,255,255,255,255,0,255,255,255,0,255,255,255,255};hwglGenTextures(1,&cutoutTexture);hwglBindTexture(GL_TEXTURE_2D,cutoutTexture);hwglTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);}
    const void*floorMaterial=nullptr,*idleMaterial=nullptr;float emissionGain=0,idleGain=0,engineMix=-1;
    bool lightingEnabled=!parity;
    auto quad=[](float left,float right,float bottom,float top,float z){hwglBegin(GL_QUADS);hwglTexCoord2f(0,0);hwglVertex3f(left,bottom,z);hwglTexCoord2f(1,0);hwglVertex3f(right,bottom,z);hwglTexCoord2f(1,1);hwglVertex3f(right,top,z);hwglTexCoord2f(0,1);hwglVertex3f(left,top,z);hwglEnd();};
    auto render=[&](unsigned frame,float strength) {
        allocator->Reset();list->Reset(allocator.Get(),nullptr);hwglBeginFrame(width,height,frame%3);hwlegacyd3d12::beginFrame(frame%3);
        hwglViewport(0,0,width,height);hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglOrtho(-1,1,-1,1,-1,1);hwglMatrixMode(GL_MODELVIEW);hwglLoadIdentity();
        hwglDisable(GL_TEXTURE_2D);hwglDisable(GL_ALPHA_TEST);hwglDisable(GL_CLIP_PLANE0);
        hwglClearColor(0,0,0,1);hwglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);hwglEnable(GL_DEPTH_TEST);hwglDepthFunc(GL_LEQUAL);hwglDepthMask(GL_TRUE);hwglDisable(GL_BLEND);hwglDisable(GL_CULL_FACE);
        g.draw.worldScene=true;hwglEnable(GL_LIGHTING);hwglEnable(GL_LIGHT0);hwglEnable(GL_COLOR_MATERIAL);hwglColor4f(1,1,1,1);hwglNormal3f(0,0,1);
        float white[]={1,1,1,1},sun[]={0,0,1,0},ambient[]={0.2f,0.2f,0.2f,1};hwglLightfv(GL_LIGHT0,GL_POSITION,sun);hwglLightfv(GL_LIGHT0,GL_DIFFUSE,white);hwglLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
#ifndef HW_RASTER_BASELINE_SOURCE
        hwglCaptureEngineIdle(-1);
        if(idleMaterial){hwglSurfaceMaterial(idleMaterial);hwglSurfaceEmission(idleGain);hwglCaptureEngineIdle(engineMix);}
        hwglSurfaceMaterial(floorMaterial);hwglSurfaceEmission(emissionGain);
#endif
        quad(-.95f,.95f,-.9f,.9f,0);hwglDisable(GL_LIGHTING);
#ifndef HW_RASTER_BASELINE_SOURCE
        hwglCaptureEngineIdle(-1);hwglSurfaceMaterial(nullptr);
#endif
        require(hwglDrawImmutableMesh(&meshIdentity,0,3,&shade)==2,"persistent classic-shade caster");
        if(filter){hwglPushMatrix();hwglTranslatef(40,0,0);require(hwglDrawImmutableMesh(&meshIdentity,0,3,&shade)==2,"distant caster for coarse shadow texels");hwglPopMatrix();}
        if(fleet)for(unsigned i=0;i<512;++i){hwglPushMatrix();hwglTranslatef(float(i%32)/17-.91f,float(i/32)/9-.86f,float(i%4)*.005f);hwglScalef(.25f,.07f,1);
            if(regression){if(i%4==0){hwglBindTexture(GL_TEXTURE_2D,cutoutTexture);hwglEnable(GL_TEXTURE_2D);hwglEnable(GL_ALPHA_TEST);hwglAlphaFunc(GL_GREATER,.5f);}else{hwglDisable(GL_TEXTURE_2D);hwglDisable(GL_ALPHA_TEST);}
                if(i%3==0){const GLdouble plane[]={1,0,0,0};hwglClipPlane(GL_CLIP_PLANE0,plane);hwglEnable(GL_CLIP_PLANE0);}else hwglDisable(GL_CLIP_PLANE0);}
            require(hwglDrawImmutableMesh(&meshIdentity,0,3,&shade)==2,"fleet immutable draw");hwglPopMatrix();}
        hwglDisable(GL_TEXTURE_2D);hwglDisable(GL_ALPHA_TEST);hwglDisable(GL_CLIP_PLANE0);
        if(overdraw){hwglEnable(GL_LIGHTING);for(unsigned i=0;i<32;++i)quad(-.95f,.95f,-.9f,.9f,-.01f*float(i+1));hwglDisable(GL_LIGHTING);}
        g.draw.worldScene=false;hwglDisable(GL_LIGHTING);hwglDisable(GL_DEPTH_TEST);hwglColor4f(0,1,0,1);quad(.7f,.9f,.7f,.9f,0);
        // Model a large legacy upload arena while keeping the tested image
        // identical, so serial/threaded preparation can be compared directly.
        if(uploadFleet)gFrameVertices.resize(gFrameVertices.size()+65536);
        hwglMarkSnapshot(HW_LEGACY_RASTER_WORLD);
#if !defined(HW_RASTER_BASELINE_SOURCE) || defined(HW_RASTER_LIGHTING_REFERENCE)
        hwlegacyd3d12::setMissionLighting(lightingEnabled,lights,1,noShadows?0:strength,0,0);
#endif
        list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
        const auto cpuStart=std::chrono::steady_clock::now();
        require(hwlegacyd3d12::renderFrameSnapshots(device.Get(),list.Get(),color.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE,width,height,gCommands.size(),nullptr,0),"native frame replay");
        cpuMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cpuStart).count();
        list->EndQuery(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);list->ResolveQueryData(timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,times.Get(),0);
        if(frame==0 && shadowRead){resourceBarrier(list.Get(),r.missionShadow.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION fromShadow={},toShadow={};fromShadow.pResource=r.missionShadow.Get();fromShadow.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            toShadow.pResource=shadowRead.Get();toShadow.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;toShadow.PlacedFootprint=shadowFootprint;list->CopyTextureRegion(&toShadow,0,0,0,&fromShadow,nullptr);
            resourceBarrier(list.Get(),r.missionShadow.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
        D3D12_TEXTURE_COPY_LOCATION from={},to={};from.pResource=color.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=read.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
        list->CopyTextureRegion(&to,0,0,0,&from,nullptr);require(SUCCEEDED(list->Close()),"close replay");ID3D12CommandList*commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);queue->Signal(fence.Get(),frame+1);fence->SetEventOnCompletion(frame+1,event);WaitForSingleObject(event,30000);
        void*mapped=nullptr;D3D12_RANGE timeRange={0,16};times->Map(0,&timeRange,&mapped);auto*t=static_cast<UINT64*>(mapped);gpuMilliseconds=double(t[1]-t[0])*1000/double(frequency);D3D12_RANGE noTimeWrite={0,0};times->Unmap(0,&noTimeWrite);
        if(frame==0 && shadowRead){D3D12_RANGE shadowRange={0,static_cast<SIZE_T>(shadowBytes)};shadowRead->Map(0,&shadowRange,&mapped);
            for(unsigned y=0;y<2048;++y)for(unsigned x=0;x<2048*4;++x)shadowHash=(shadowHash^static_cast<unsigned char*>(mapped)[y*shadowFootprint.Footprint.RowPitch+x])*1099511628211ull;
            shadowRead->Unmap(0,&noTimeWrite);}
        D3D12_RANGE range={0,static_cast<SIZE_T>(bytes)};read->Map(0,&range,&mapped);std::vector<unsigned short> pixels(width*height*4);
        for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+y*width*4,static_cast<unsigned char*>(mapped)+y*footprint.Footprint.RowPitch,width*8);
        D3D12_RANGE noWrite={0,0};read->Unmap(0,&noWrite);hwglEndFrame();return pixels;
    };
    auto first=render(0,1);
    auto channel=[&](const std::vector<unsigned short>&p,float x,float y,unsigned c){unsigned px=unsigned((x*.5f+.5f)*width),py=unsigned((y*.5f+.5f)*height);return halfFloat(p[(py*width+px)*4+c]);};
    unsigned long long hash=1469598103934665603ull;for(auto v:first){hash=(hash^(v&255))*1099511628211ull;hash=(hash^(v>>8))*1099511628211ull;}
    if(stress){double total=0,cpu=0;const unsigned samples=performance?32:8;for(unsigned i=1;i<=samples;++i){render(i,1);total+=gpuMilliseconds;cpu+=cpuMilliseconds;}std::cout<<"STRESS GPU "<<total/samples<<" ms CPU "<<cpu/samples<<" ms IMAGE "<<std::hex<<hash<<std::dec<<"\n";}
    else if(parity)std::cout<<"PARITY "<<std::hex<<hash<<std::dec<<"\n";
    else {
        float lit=channel(first,.6f,0,0),shadow=channel(first,-.13f,0,0);
        std::cout<<"lighting red="<<lit<<" shadow="<<shadow<<" green="<<channel(first,.6f,0,1)<<"\n";
        require(lit>channel(first,.6f,0,1)*2,"authored sun color");require(shadow<lit*.7f,"sun casts offset shadow");
        auto noShadow=render(1,0);require(channel(noShadow,-.13f,0,0)>shadow*1.5f,"shadow-strength control");
        require(channel(first,.8f,.8f,1)>.9f&&channel(first,.8f,.8f,0)<.01f,"UI is not mission lit");
        lights[0].direction[2]=-1;auto reverse=render(2,1);require(channel(reverse,.6f,0,0)<lit*.5f,"sun direction controls surface hemisphere");
        lights[0].intensity=0;lights[1].color[1]=.5f;auto ambientFrame=render(3,1);require(channel(ambientFrame,.6f,0,1)>channel(ambientFrame,.6f,0,0)*2,"authored ambient color");
        lights.assign(1,{});lights[0].type=2;lights[0].position[0]=.6f;lights[0].position[2]=.5f;
        lights[0].radius=.75f;lights[0].color[2]=1;lights[0].intensity=2;
        auto point=render(4,1);require(channel(point,.6f,0,2)>.4f && channel(point,-.8f,0,2)<.01f,"authored point color/radius");
        lights[0].type=3;lights[0].direction[2]=-1;lights[0].coneCos=.98f;lights[0].edgeCos=.9f;
        auto spot=render(5,1);require(channel(spot,.6f,0,2)>.4f && channel(spot,.25f,0,2)<.01f,"authored spot cone");
        lights[0].type=4;lights[0].endPosition[0]=.6f;lights[0].endPosition[1]=.6f;lights[0].endPosition[2]=.5f;
        auto line=render(6,1);require(channel(line,.6f,.4f,2)>.4f,"dynamic line-light extent");
#ifndef HW_RASTER_BASELINE_SOURCE
        lights.resize(512);
        for(unsigned i=1;i<512;++i){auto&l=lights[i];l.type=2;l.position[0]=100+float(i);l.radius=1;l.intensity=1;l.color[0]=l.color[1]=l.color[2]=1;}
        auto culled=render(7,1);require(culled==line,"spatial light culling changes no pixels");
        require(hwlegacyd3d12::missionIndices.size()<1024,"unreachable lights were not removed from draw lists");
        static int bayId,onId,idleId;
        emissionFixture::materials[&bayId]={1,4,1};emissionFixture::pixels[1]={0xff800000u,0xff800000u,0xff000000u,0xff000000u};
        emissionFixture::materials[&onId]={2,1,1};emissionFixture::pixels[2]={0xff0000ffu};
        emissionFixture::materials[&idleId]={3,1,1};emissionFixture::pixels[3]={0xffff0000u};
        hwlegacyd3d12::setEmissionSource(emissionFixture::describe,emissionFixture::read);
        lights.assign(1,{});lights[0].type=0;lights[0].intensity=1;
        floorMaterial=&bayId;emissionGain=3;auto bay=render(8,0);
        const float bayBlue=channel(bay,-.65f,0,2);
        require(bayBlue>1,"hangar emission produces HDR radiance in darkness");
        require(channel(bay,.65f,0,2)<.01f,"black mask must not make the hull glow");
        lights[0].color[1]=.8f;auto greenAmbient=render(9,0);
        require(std::fabs(channel(greenAmbient,-.65f,0,2)-bayBlue)<.002f,"ambient tint changes authored emission color");
        require(channel(greenAmbient,.65f,0,1)>.5f,"ordinary hull still receives ambient light");
        require(channel(bay,.8f,.8f,1)>.9f && channel(bay,.8f,.8f,2)<.01f,"glow state leaks into UI");
        lights[0].color[1]=0;floorMaterial=&onId;idleMaterial=&idleId;emissionGain=1;idleGain=.5f;
        engineMix=0;auto idle=render(10,0);require(channel(idle,.6f,0,2)>.7f && channel(idle,.6f,0,0)<.01f,"idle engine emission missing");
        engineMix=1;auto running=render(11,0);require(channel(running,.6f,0,0)>.99f && channel(running,.6f,0,2)<.01f,"active engine emission missing");
        engineMix=.5f;auto blend=render(12,0);
        require(std::fabs(std::pow(channel(blend,.6f,0,0),2.2f)-.5f)<.003f && std::fabs(std::pow(channel(blend,.6f,0,2),2.2f)-.25f)<.003f,"engine emission blend must interpolate radiance");
        hwlegacyd3d12::setEmissionBrightness(.25f);auto dim=render(13,0);
        require(std::fabs(std::pow(channel(dim,.6f,0,0),2.2f)-.125f)<.003f &&
            std::fabs(std::pow(channel(dim,.6f,0,2),2.2f)-.0625f)<.003f,"glow brightness must scale idle and active linear radiance");
        hwlegacyd3d12::setEmissionBrightness(0);auto dark=render(14,0);
        require(channel(dark,.6f,0,0)<.01f && channel(dark,.6f,0,2)<.01f,"zero glow brightness leaves emission visible");
        require(channel(dark,.8f,.8f,1)>.9f,"glow brightness changes UI");
        lightingEnabled=false;auto legacyDark=render(15,0);
        hwlegacyd3d12::setEmissionBrightness(2);auto legacyBright=render(16,0);
        require(legacyDark==legacyBright,"non-PT brightness changes the disabled/path-traced compatibility pass");
        lightingEnabled=true;hwlegacyd3d12::setEmissionBrightness(1);
        std::cout<<"PASS: non-PT glow brightness scales both engine states, preserves UI and leaves path-traced compatibility pixels identical\n";
        require(emissionFixture::reads==3,"warm frames duplicate emission decode/upload");
        for(const auto&entry:hwlegacyd3d12::missionEmissionTextures)require(entry.second.texture.pixels.empty(),"extra expanded CPU emission copy retained");
        emissionFixture::materials.erase(&onId);idleMaterial=nullptr;engineMix=-1;auto removed=render(17,0);
        require(channel(removed,.6f,0,0)<.01f && channel(removed,.6f,0,2)<.01f,"removed material retains a stale glow binding");
        // Model 512 MiB of distinct authored maps with small GPU fixtures.
        // Scaled allocation accounting exercises production eviction without
        // requiring half a GiB of textures on every regression-test adapter.
        hwlegacyd3d12::clearMissionEmission();
        hwlegacyd3d12::missionEmissionAdapterChecked=true;
        hwlegacyd3d12::missionEmissionAdapterBudget=2ull<<30;
        hwlegacyd3d12::missionEmissionLimit=1ull<<30;
        int sweepIds[16]={};
        for(unsigned i=0;i<16;++i){emissionFixture::materials[&sweepIds[i]]={100+i,1,1};emissionFixture::pixels[100+i]={0xff804020u};}
        const unsigned beforeSweep=emissionFixture::reads;
        std::vector<unsigned short> sweepReference;
        for(unsigned i=0;i<80;++i){
            floorMaterial=&sweepIds[i%16];emissionGain=1;
            auto image=render(18+i,0);
            if(i==0)sweepReference=image;else require(image==sweepReference,"camera sweep changes warm texture pixels");
            auto&entry=hwlegacyd3d12::missionEmissionTextures.at(100+i%16);
            hwlegacyd3d12::missionEmissionBytes-=entry.bytes;entry.bytes=32ull<<20;
            hwlegacyd3d12::missionEmissionBytes+=entry.bytes;
        }
        require(emissionFixture::reads-beforeSweep==16,"camera sweeps re-decode/re-upload warm authored glow maps");
        require(hwlegacyd3d12::missionEmissionTextures.size()==16,"camera sweep discarded a warm working set despite spare budget");
        hwlegacyd3d12::missionEmissionLimit=96ull<<20;
        for(unsigned i=0;i<8;++i)require(render(98+i,0)==sweepReference,"memory pressure changes current glow pixels");
        require(hwlegacyd3d12::missionEmissionBytes<=96ull<<20,"inactive glow cache ignores real memory pressure");
        require(hwlegacyd3d12::missionEmissionTextures.count(115)==1,"memory pressure evicted the visible engine map");
        require(emissionFixture::reads-beforeSweep==16,"memory pressure re-uploaded the visible glow map");
        std::cout<<"PASS: 80-frame camera sweep retains simulated 512 MiB working set after 16 uploads; pressure trims to 96 MiB with visible glow pinned and exact pixels\n";
        hwlegacyd3d12::setEmissionSource(nullptr,nullptr);floorMaterial=nullptr;emissionGain=0;
        std::cout<<"PASS: masked HDR hangar emission, ambient independence, idle/running radiance blend, cache reuse and UI isolation\n";
        const float low[3]={-1,-1,-1},high[3]={1,1,1};
        hwmodern::ResolvedSceneLight local={};local.type=2;local.radius=1;local.position[0]=2;
        require(hwlegacyd3d12::missionLightTouches(local,low,high),"tangent light culled");
        local.position[0]=2.01f;require(!hwlegacyd3d12::missionLightTouches(local,low,high),"disjoint light retained");
        local.type=4;local.position[0]=-10;local.endPosition[0]=10;local.radius=.1f;
        require(hwlegacyd3d12::missionLightTouches(local,low,high),"crossing line light culled");
        local.type=1;local.position[0]=1e10f;require(hwlegacyd3d12::missionLightTouches(local,low,high),"directional light culled");
        // Keep all candidates for non-finite support, allowing the shader's
        // existing policy to decide rather than creating culling holes.
        local.type=2;local.radius=std::numeric_limits<float>::infinity();
        require(hwlegacyd3d12::missionLightTouches(local,low,high),"non-finite radius culled");
        unsigned seed=5917;
        auto random=[&]{seed=seed*1664525u+1013904223u;return float((seed>>8)&65535)/1024-32;};
        for(unsigned i=0;i<512;++i){auto&l=hwlegacyd3d12::missionLighting.lights[i];l={};l.type=i%5;l.radius=std::fabs(random())*.3f;
            for(unsigned a=0;a<3;++a){l.position[a]=random();l.endPosition[a]=random();}}
        hwlegacyd3d12::buildMissionLightTree(512);
        for(unsigned sample=0;sample<512;++sample){float lo[3],hi[3];for(unsigned a=0;a<3;++a){lo[a]=random();hi[a]=lo[a]+std::fabs(random())*.15f;}
            std::vector<unsigned> expected;for(unsigned i=0;i<512;++i)if(hwlegacyd3d12::missionLightTouches(hwlegacyd3d12::missionLighting.lights[i],lo,hi))expected.push_back(i);
            hwlegacyd3d12::missionIndices=hwlegacyd3d12::missionInfiniteLights;
            hwlegacyd3d12::queryMissionLights(0,lo,hi);std::sort(hwlegacyd3d12::missionIndices.begin(),hwlegacyd3d12::missionIndices.end());
            require(expected==hwlegacyd3d12::missionIndices,"spatial tree changed the brute-force light list");}
        std::cout<<"PASS: 512 spatial queries exactly match all 512 brute-force point/spot/line/global lights\n";
        std::cout<<"PASS: 511 unreachable lights removed with exact pixel parity; tangent, line and infinite support retained\n";
#endif
        std::cout<<"PASS: production raster sun color/direction, actual shadow, strength, ambient and UI isolation\n";
    }
    if(regression && shadowRead)std::cout<<"SHADOW_DEPTH "<<std::hex<<shadowHash<<std::dec<<"\n";
    if(filter && !noShadows){std::vector<unsigned short> levels;const unsigned y=height/2;
        for(unsigned x=unsigned(.325f*width);x<unsigned(.4725f*width);++x)levels.push_back(first[(y*width+x)*4]);
        std::sort(levels.begin(),levels.end());levels.erase(std::unique(levels.begin(),levels.end()),levels.end());
        std::cout<<"SHADOW_LEVELS "<<levels.size()<<"\n";
#ifndef HW_RASTER_BASELINE_SOURCE
        require(levels.size()>12,"shadow filter still quantizes into hard nine-tap bands");
#endif
    }
#ifndef HW_RASTER_BASELINE_SOURCE
    if(fleet && !noShadows)require(hwlegacyd3d12::missionShadowBatches.size()<5,"repeated hulls did not batch shadow draws");
#endif
    if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n=0;info->GetMessage(i,nullptr,&n);std::vector<unsigned char> storage(n);auto*m=reinterpret_cast<D3D12_MESSAGE*>(storage.data());info->GetMessage(i,m,&n);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<m->pDescription<<"\n";throw std::runtime_error("D3D12 validation error");}}
    hwlegacyd3d12::shutdown();CloseHandle(event);return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
