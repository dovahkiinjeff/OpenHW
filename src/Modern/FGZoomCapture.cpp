#include "DiagnosticPolicy.h"
#include "FGZoomCapture.h"
#if HW_ENABLE_DIAGNOSTICS
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace hwmodern { namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned sampleLimit=12;
constexpr auto combinedRead=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
struct Layer {
    ComPtr<ID3D12Resource> readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes=0, rowBytes=0;
    const char* name=nullptr;
};
struct Sample {
    FGEffectGuideCamera camera;
    float distance=0,jitterX=0,jitterY=0,delta=0;
    bool reset=false;
    bool generationRecorded=false, generationReset=false;
    unsigned generatedFrames=0;
    unsigned long long fence=0;
    int antiAliasingMode=0,frameGenerationMode=0;
    bool reconstructionRecorded=false;
    bool cloudProbeRecorded=false;
    float nativeProjection[2]={};
    std::string motionState;
    std::vector<Layer> layers;
};
struct State {
    bool triggered=false, finished=false, haveDistance=false, zoomRequested=false;
    float previousDistance=0;
    unsigned long long previousFrame=0;
    FGEffectGuideCamera previousCamera;
    std::vector<Sample> samples;
    std::string request;
} state;
std::string completedRequest;
bool temporalMode() {
    static const bool active=[] {const char* value=std::getenv("HW_TEMPORAL_CAPTURE");return value&&std::strcmp(value,"1")==0;}();
    return active;
}
bool enabled() {
    static const bool active=[] {
        const char* value=std::getenv("HW_FG_ZOOM_CAPTURE");
        const bool active=temporalMode() || (value && std::strcmp(value,"1")==0);
        if(active)HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Recorder enabled; waiting for arming and zoom.\n");
        return active;
    }();
    return active;
}
bool armed() {
    // An optional launch-specific marker lets the user choose the affected
    // scene. Loading/cutscene camera motion must not consume the one capture.
    static const std::wstring marker=[] {
        wchar_t path[32768]{};
        const DWORD size=GetEnvironmentVariableW(L"HW_FG_ZOOM_CAPTURE_ARM_FILE",path,32768);
        return size && size<32768 ? std::wstring(path) : std::wstring();
    }();
    if(temporalMode()) {
        if(state.triggered)return true;
        if(marker.empty())return false;
        std::ifstream request(marker);
        std::string label;std::getline(request,label);
        if(label.empty()||label.size()>80||label==completedRequest)return false;
        if(!std::all_of(label.begin(),label.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';}))return false;
        state.request=label;return true;
    }
    if(marker.empty())return true;
    static bool ready=false;
    if(!ready) {
        ready=GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES;
        if(ready)HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Armed; waiting for an applied gameplay zoom.\n");
    }
    return ready;
}
void barrier(ID3D12GraphicsCommandList* commands,ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};commands->ResourceBarrier(1,&b);
}
bool allocate(ID3D12Device* device,ID3D12Resource* source,const char* name,Layer& layer) {
    if(!source)return false;
    const auto desc=source->GetDesc();
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count!=1 || desc.DepthOrArraySize!=1 || desc.MipLevels!=1)return false;
    layer.name=name;
    device->GetCopyableFootprints(&desc,0,1,0,&layer.footprint,nullptr,&layer.rowBytes,&layer.bytes);
    if(layer.bytes>128ull*1024*1024 || layer.rowBytes==0)return false;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=layer.bytes;
    buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&layer.readback)));
}
void copy(ID3D12GraphicsCommandList* commands,ID3D12Resource* source,D3D12_RESOURCE_STATES read,const Layer& layer) {
    barrier(commands,source,read,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=source;from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=layer.readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=layer.footprint;
    commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    barrier(commands,source,D3D12_RESOURCE_STATE_COPY_SOURCE,read);
}
std::filesystem::path outputDirectory() {
    wchar_t custom[32768]{};
    const DWORD customLength=GetEnvironmentVariableW(L"HW_FG_ZOOM_CAPTURE_DIR",custom,32768);
    if(customLength && customLength<32768)return temporalMode()?std::filesystem::path(custom)/state.request:std::filesystem::path(custom);
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    SYSTEMTIME now{};GetLocalTime(&now);wchar_t folder[128]{};
    swprintf_s(folder,L"FGZoom-%04u%02u%02u-%02u%02u%02u-%lu",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,GetCurrentProcessId());
    wchar_t development[32768]{};
    const DWORD length=GetEnvironmentVariableW(L"HW_DEVELOPMENT_ROOT",development,32768);
    const auto base=length && length<32768?std::filesystem::path(development):std::filesystem::path(executable).parent_path()/L"Development";
    return base/L"Diagnostics"/folder;
}
void array(std::ostream& out,const float* values) {
    out<<'['<<values[0]<<','<<values[1]<<','<<values[2]<<']';
}
void writeSamples() {
    try {
        const auto directory=outputDirectory();std::filesystem::create_directories(directory);
        std::ofstream manifest(directory/"capture.json");manifest.exceptions(std::ios::badbit|std::ios::failbit);
        manifest<<std::setprecision(9)<<"{\"version\":"<<(temporalMode()?4:3)<<",\"trigger\":\""<<(temporalMode()?"explicit reconstruction burst":"gameplay camera zoom input")<<"\",\"projection\":\"normal finite\",\"near\":0.05,\"far\":500000,\"samples\":[";
        for(size_t i=0;i<state.samples.size();++i) {
            auto& sample=state.samples[i];if(i)manifest<<',';
            manifest<<"{\"frame\":"<<sample.camera.frameId<<",\"distance\":"<<sample.distance<<",\"dt\":"<<sample.delta
                <<",\"reset\":"<<(sample.reset?"true":"false")<<",\"fov\":"<<sample.camera.verticalFovRadians<<",\"aspect\":"<<sample.camera.aspect
                <<",\"generationRecorded\":"<<(sample.generationRecorded?"true":"false")
                <<",\"generationReset\":"<<(sample.generationReset?"true":"false")
                <<",\"generatedFrameCount\":"<<sample.generatedFrames
                <<",\"antiAliasingMode\":"<<sample.antiAliasingMode<<",\"frameGenerationMode\":"<<sample.frameGenerationMode
                <<",\"reconstructionRecorded\":"<<(sample.reconstructionRecorded?"true":"false")
                <<",\"cloudProbeRecorded\":"<<(sample.cloudProbeRecorded?"true":"false")
                <<",\"nativeProjection\":["<<sample.nativeProjection[0]<<','<<sample.nativeProjection[1]<<']'
                <<",\"jitter\":["<<sample.jitterX<<','<<sample.jitterY<<"],\"position\":";
            array(manifest,sample.camera.position);manifest<<",\"right\":";array(manifest,sample.camera.right);
            manifest<<",\"up\":";array(manifest,sample.camera.up);manifest<<",\"forward\":";array(manifest,sample.camera.forward);
            manifest<<",\"layers\":[";
            for(size_t j=0;j<sample.layers.size();++j) {
                auto& layer=sample.layers[j];const auto& fp=layer.footprint.Footprint;
                const std::string name=std::to_string(i)+"-"+layer.name+".bin";
                void* memory=nullptr;D3D12_RANGE read{0,static_cast<SIZE_T>(layer.bytes)};
                if(FAILED(layer.readback->Map(0,&read,&memory)))throw std::runtime_error("readback map failed");
                try {
                    std::ofstream pixels(directory/name,std::ios::binary);pixels.exceptions(std::ios::badbit|std::ios::failbit);
                    for(unsigned y=0;y<fp.Height;++y)pixels.write(static_cast<char*>(memory)+layer.footprint.Offset+static_cast<size_t>(y)*fp.RowPitch,static_cast<std::streamsize>(layer.rowBytes));
                }catch(...){D3D12_RANGE none{};layer.readback->Unmap(0,&none);throw;}
                D3D12_RANGE none{};layer.readback->Unmap(0,&none);
                if(j)manifest<<',';
                manifest<<"{\"file\":\""<<name<<"\",\"width\":"<<fp.Width<<",\"height\":"<<fp.Height<<",\"format\":"<<static_cast<unsigned>(fp.Format)<<",\"rowBytes\":"<<layer.rowBytes
                    <<",\"orientation\":\""<<(std::strcmp(layer.name,"native-depth")==0?"bottom-up":"top-down")<<"\"}";
            }
            manifest<<']';
            if(!sample.motionState.empty()) {
                const auto name=std::to_string(i)+"-motion-state.json";
                std::ofstream trace(directory/name);trace.exceptions(std::ios::badbit|std::ios::failbit);
                trace<<sample.motionState;trace.close();
                manifest<<",\"motionStateFile\":\""<<name<<'"';
            }
            manifest<<'}';
        }
        manifest<<"]}\n";manifest.close();
        HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Saved %zu consecutive frame inputs to %ls\n",state.samples.size(),directory.c_str());
    }catch(const std::exception& error){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Capture save failed: %s\n",error.what());}
    if(temporalMode()){completedRequest=state.request;state=State{};}
    else {state.samples.clear();state.finished=true;}
}
Sample* beginSample(const FGEffectGuideCamera& camera,float distance,float jx,float jy,float delta,bool reset,unsigned long long fence) {
    if(!enabled()||state.finished||!armed())return nullptr;
    if(!state.samples.empty()&&state.samples.back().camera.frameId==camera.frameId)return &state.samples.back();
    if(state.samples.size()>=sampleLimit)return nullptr;
    if(!std::isfinite(distance)||(temporalMode()?distance<0:distance<=0))return nullptr;
    if(!state.triggered) {
        const bool consecutive=state.haveDistance&&camera.frameId==state.previousFrame+1;
        const bool zoom=state.zoomRequested&&consecutive&&std::fabs(distance-state.previousDistance)>std::max(.01f,state.previousDistance*.0001f);
        bool movement=false;
        if(consecutive)for(unsigned axis=0;axis<3;++axis)
            movement=movement||std::fabs(camera.position[axis]-state.previousCamera.position[axis])>.001f||
                std::fabs(camera.forward[axis]-state.previousCamera.forward[axis])>.00001f||
                std::fabs(camera.up[axis]-state.previousCamera.up[axis])>.00001f;
        if(temporalMode()) {
            const bool waitZoom=state.request.rfind("zoom-",0)==0,waitMove=state.request.rfind("move-",0)==0;
            state.triggered=waitZoom?zoom:waitMove?movement:true;
        }else state.triggered=zoom;
        state.previousDistance=distance;state.previousFrame=camera.frameId;state.haveDistance=true;
        state.previousCamera=camera;
        if(!state.triggered)return nullptr;
        HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Collecting twelve consecutive frame inputs (%s).\n",temporalMode()?state.request.c_str():"gameplay zoom");
    }
    Sample sample;sample.camera=camera;sample.distance=distance;sample.jitterX=jx;sample.jitterY=jy;
    sample.delta=delta;sample.reset=reset;sample.fence=fence;
    state.samples.push_back(std::move(sample));return &state.samples.back();
}
bool appendLayers(ID3D12GraphicsCommandList* commands,ID3D12Device* device,Sample& sample,
    unsigned count,ID3D12Resource* const* resources,const char* const* names,const D3D12_RESOURCE_STATES* reads) {
    std::vector<Layer> layers(count);
    std::uint64_t retained=0;for(const auto&s:state.samples)for(const auto&l:s.layers)retained+=l.bytes;
    for(unsigned i=0;i<count;++i){
        if(!allocate(device,resources[i],names[i],layers[i]))return false;
        retained+=layers[i].bytes;
        if(temporalMode()&&retained>(2ull<<30))return false;
    }
    const auto first=sample.layers.size();sample.layers.reserve(first+count);
    for(auto& layer:layers)sample.layers.push_back(std::move(layer));
    for(unsigned i=0;i<count;++i)copy(commands,resources[i],reads[i],sample.layers[first+i]);
    return true;
}
} // namespace
bool temporalCaptureEnabled(){return temporalMode();}
void temporalCaptureRecordMotionState(unsigned long long frameId,const char* json) {
    if(!temporalMode() || !json || state.finished)return;
    const auto sample=std::find_if(state.samples.begin(),state.samples.end(),[frameId](const Sample&s){return s.camera.frameId==frameId;});
    if(sample==state.samples.end() || !sample->motionState.empty())return;
    // CPU-only trace is bounded independently of the GPU readback budget.
    const size_t length=std::strlen(json);
    if(length<=2u*1024u*1024u)sample->motionState.assign(json,length);
}
bool temporalCaptureNeedsCloudProbe(unsigned long long frameId) {
    if(!temporalMode() || state.finished)return false;
    const auto sample=std::find_if(state.samples.begin(),state.samples.end(),[frameId](const Sample&s){return s.camera.frameId==frameId;});
    return sample!=state.samples.end() && sample->reconstructionRecorded && !sample->cloudProbeRecorded;
}
void temporalCaptureRecordCloudProbe(ID3D12GraphicsCommandList* commands,unsigned long long frameId,
    ID3D12Resource* cloudlessInput,ID3D12Resource* nativeDepth,ID3D12Resource* cloudField,float p10,float p14) {
    if(!commands || !cloudlessInput || !nativeDepth || !cloudField || !temporalCaptureNeedsCloudProbe(frameId))return;
    const auto sample=std::find_if(state.samples.begin(),state.samples.end(),[frameId](const Sample&s){return s.camera.frameId==frameId;});
    ComPtr<ID3D12Device> device;if(FAILED(cloudlessInput->GetDevice(IID_PPV_ARGS(&device))))return;
    ID3D12Resource* resources[]={cloudlessInput,nativeDepth,cloudField};
    const char* names[]={"sr-cloudless","native-raster-depth","cloud-scattering-transmission"};
    const D3D12_RESOURCE_STATES reads[]={D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    try {
        if(!appendLayers(commands,device.Get(),*sample,3,resources,names,reads)){state.finished=true;return;}
        sample->nativeProjection[0]=p10;sample->nativeProjection[1]=p14;sample->cloudProbeRecorded=true;
    }catch(const std::exception&e){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Cloud comparison failed: %s\n",e.what());state.finished=true;}
}
void temporalCaptureRecordInputs(ID3D12GraphicsCommandList* commands,ID3D12Device* device,unsigned long long fence,
    const FGEffectGuideCamera& camera,float distance,float jx,float jy,float delta,bool reset,int aa,int fg,
    ID3D12Resource* input,ID3D12Resource* depth,ID3D12Resource* motion) {
    if(!temporalMode()||!commands||!device)return;
    try {
        auto* sample=beginSample(camera,distance,jx,jy,delta,reset,fence);if(!sample||sample->reconstructionRecorded)return;
        ID3D12Resource* resources[]={input,depth,motion};const char* names[]={"sr-input","opaque-depth","opaque-motion"};
        D3D12_RESOURCE_STATES reads[]={D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        sample->antiAliasingMode=aa;sample->frameGenerationMode=fg;
        if(!appendLayers(commands,device,*sample,3,resources,names,reads)){state.finished=true;return;}
        sample->reconstructionRecorded=true;
    }catch(const std::exception& e){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Reconstruction capture failed: %s\n",e.what());state.finished=true;}
}
void temporalCaptureRecordResolved(ID3D12GraphicsCommandList* commands,unsigned long long frameId,ID3D12Resource* resolved) {
    if(!temporalMode()||!commands||!resolved)return;
    const auto found=std::find_if(state.samples.begin(),state.samples.end(),[frameId](const Sample&s){return s.camera.frameId==frameId;});
    if(found==state.samples.end())return;
    ComPtr<ID3D12Device> device;if(FAILED(resolved->GetDevice(IID_PPV_ARGS(&device))))return;
    ID3D12Resource* resources[]={resolved};const char* names[]={"sr-output"};D3D12_RESOURCE_STATES reads[]={combinedRead};
    try{if(!appendLayers(commands,device.Get(),*found,1,resources,names,reads))state.finished=true;}
    catch(const std::exception& e){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Resolved capture failed: %s\n",e.what());state.finished=true;}
}
void fgZoomCaptureNotifyZoom() {
    if(!enabled() || state.finished || state.triggered || !armed())return;
    if(!state.zoomRequested)
        HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Gameplay zoom input received; waiting for camera movement.\n");
    state.zoomRequested=true;
}
void fgZoomCaptureRecord(ID3D12GraphicsCommandList* commands,ID3D12Device* device,unsigned long long fence,
    const FGEffectGuideCamera& camera,float distance,float jx,float jy,float delta,bool reset,
    ID3D12Resource* color,ID3D12Resource* mergedDepth,ID3D12Resource* mergedMotion,ID3D12Resource* opaqueDepth,ID3D12Resource* opaqueMotion,
    ID3D12Resource* nativeDepth,ID3D12Resource* cloudGuide) {
    if(!enabled()||!commands||!device)return;
    try {
        auto* sample=beginSample(camera,distance,jx,jy,delta,reset,fence);if(!sample)return;
        if(std::any_of(sample->layers.begin(),sample->layers.end(),[](const Layer& layer){return std::strcmp(layer.name,"color")==0;}))return;
        ID3D12Resource* resources[7]={color,mergedDepth,mergedMotion,opaqueDepth,opaqueMotion};
        const char* names[7]={"color","fg-depth","fg-motion","opaque-depth","opaque-motion"};
        D3D12_RESOURCE_STATES reads[7]={combinedRead,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            combinedRead,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,combinedRead};
        unsigned layers=sample->reconstructionRecorded?3:5;
        if(nativeDepth){resources[layers]=nativeDepth;names[layers]="native-depth";reads[layers++]=combinedRead;}
        if(cloudGuide){resources[layers]=cloudGuide;names[layers]="cloud-guide";reads[layers++]=combinedRead;}
        if(!appendLayers(commands,device,*sample,layers,resources,names,reads)) {
            HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Allocation/budget limit; saving the completed portion.\n");state.finished=true;
        }
    }catch(const std::exception& error){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Capture stopped: %s\n",error.what());state.finished=true;}

}
void fgZoomCapturePoll(ID3D12Fence* fence) {
    if(!enabled()||state.samples.empty()||!fence)return;
    const auto completed=fence->GetCompletedValue();
    if(completed==~0ull)return;
    const auto needed=std::max_element(state.samples.begin(),state.samples.end(),[](const Sample& a,const Sample& b){return a.fence<b.fence;})->fence;
    if(completed>=needed && (state.samples.size()==sampleLimit||state.finished))writeSamples();
}
void fgZoomCaptureRecordGenerated(ID3D12GraphicsCommandList* commands,unsigned long long frameId,
    ID3D12Resource* present,ID3D12Resource* generated,bool reset,unsigned count) {
    if(!enabled() || !commands || !present || !generated)return;
    const auto found=std::find_if(state.samples.begin(),state.samples.end(),
        [frameId](const Sample& sample){return sample.camera.frameId==frameId;});
    if(found==state.samples.end() || found->generationRecorded)return;
    ComPtr<ID3D12Device> device;
    if(FAILED(present->GetDevice(IID_PPV_ARGS(&device))))return;
    try {
        Layer presented,interpolated;
        if(!allocate(device.Get(),present,"presented-color",presented) ||
            !allocate(device.Get(),generated,"generated-color",interpolated))return;
        // Reserve and retain both destinations before recording references.
        const auto first=found->layers.size();found->layers.reserve(first+2);
        found->layers.push_back(std::move(presented));
        found->layers.push_back(std::move(interpolated));
        copy(commands,present,D3D12_RESOURCE_STATE_PRESENT,found->layers[first]);
        copy(commands,generated,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,found->layers[first+1]);
        found->generationReset=reset;found->generatedFrames=count;found->generationRecorded=true;
    }catch(const std::exception& error){HW_DIAG_FPRINTF(stderr,"[FGZoomCapture] Generation capture failed: %s\n",error.what());}
}
void fgZoomCaptureFinish(ID3D12Fence* fence) {
    // Presenter initialization calls shutdown before creating its first device.
    // Empty cleanup must not consume this process's later user-armed capture.
    if(!enabled() || state.samples.empty())return;
    state.finished=true;
    fgZoomCapturePoll(fence);
}
} // namespace hwmodern

#endif // HW_ENABLE_DIAGNOSTICS
