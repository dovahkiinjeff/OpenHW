// CPU-only: execute the actual integration against mocked Streamline entry
// points and inert COM vtables. No D3D device, driver or GPU work is created.
#include "../src/Modern/ModernDlaa.cpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace mock {
enum class Stage { None, Token, Constants, SrOptions, RrOptions, Tags, Evaluate, FgOptions, FgState, ReflexOptions, Sleep, Marker };
Stage failure=Stage::None;
sl::Result result=sl::Result::eWarnOutOfVRAM;
unsigned calls=0,tags=0,evaluations=0,barriers=0,releases=0;
unsigned fgOffCalls=0,fgOnCalls=0;
unsigned tokenCalls=0,sleepCalls=0,fgStateCalls=0;
unsigned framesPresented=2;
sl::DLSSGStatus fgStatus=sl::DLSSGStatus::eOk;
std::vector<sl::Feature> freed;
std::vector<sl::ResourceLifecycle> lifecycles;
std::vector<sl::BufferType> tagTypes;
std::vector<sl::Extent> tagExtents;
std::vector<std::pair<sl::PCLMarker,unsigned>> markers;
std::vector<unsigned> constantFrames;
struct Token final:sl::FrameToken {unsigned index=1;operator uint32_t()const override{return index;}} token;
struct Com {void **table;};
void *resourceTable[32]={},*commandsTable[96]={};
Com resource{resourceTable},commands{commandsTable};
ULONG STDMETHODCALLTYPE release(void *){++releases;return 1;}
void STDMETHODCALLTYPE barrier(void *,UINT,const D3D12_RESOURCE_BARRIER *){++barriers;}
sl::Result check(Stage stage){++calls;return stage==failure?result:sl::Result::eOk;}
sl::Result srOptions(const sl::ViewportHandle&,const sl::DLSSOptions&){return check(Stage::SrOptions);}
sl::Result rrOptions(const sl::ViewportHandle&,const sl::DLSSDOptions&){return check(Stage::RrOptions);}
sl::Result srOptimal(const sl::DLSSOptions&,sl::DLSSOptimalSettings&){return sl::Result::eOk;}
sl::Result rrOptimal(const sl::DLSSDOptions&,sl::DLSSDOptimalSettings&){return sl::Result::eOk;}
sl::Result reflexOptions(const sl::ReflexOptions&){return check(Stage::ReflexOptions);}
sl::Result fgOptions(const sl::ViewportHandle&,const sl::DLSSGOptions &options){
    if(options.mode==sl::DLSSGMode::eOff)++fgOffCalls;else ++fgOnCalls;
    return check(Stage::FgOptions);
}
sl::Result fgState(const sl::ViewportHandle&,sl::DLSSGState &state,const sl::DLSSGOptions *options){
    ++fgStateCalls;state.status=fgStatus;state.numFramesActuallyPresented=framesPresented;
    if(options)return sl::Result::eErrorInvalidParameter;
    return check(Stage::FgState);
}
sl::Result sleep(const sl::FrameToken&){++sleepCalls;return check(Stage::Sleep);}
sl::Result marker(sl::PCLMarker marker,const sl::FrameToken&frame){markers.emplace_back(marker,static_cast<unsigned>(frame));return check(Stage::Marker);}
}

extern "C" sl::Result slInit(const sl::Preferences&,uint64_t){return sl::Result::eOk;}
extern "C" sl::Result slSetD3DDevice(void*){return sl::Result::eOk;}
extern "C" sl::Result slIsFeatureSupported(sl::Feature,const sl::AdapterInfo&){return sl::Result::eOk;}
extern "C" sl::Result slGetNewFrameToken(sl::FrameToken *&token,const uint32_t *index) {
    ++mock::tokenCalls;if(index)mock::token.index=*index;
    auto result=mock::check(mock::Stage::Token);token=&mock::token;return result;
}
extern "C" sl::Result slSetConstants(const sl::Constants&,const sl::FrameToken&frame,const sl::ViewportHandle&) {
    mock::constantFrames.push_back(static_cast<unsigned>(frame));
    return mock::check(mock::Stage::Constants);
}
extern "C" sl::Result slSetTagForFrame(const sl::FrameToken&,const sl::ViewportHandle&,
    const sl::ResourceTag *tags,uint32_t count,sl::CommandBuffer*) {
    ++mock::tags;mock::lifecycles.clear();mock::tagTypes.clear();mock::tagExtents.clear();
    for(uint32_t i=0;i<count;++i){mock::lifecycles.push_back(tags[i].lifecycle);mock::tagTypes.push_back(tags[i].type);mock::tagExtents.push_back(tags[i].extent);}
    return mock::check(mock::Stage::Tags);
}
extern "C" sl::Result slEvaluateFeature(sl::Feature,const sl::FrameToken&,
    const sl::BaseStructure**,uint32_t,sl::CommandBuffer*) {
    ++mock::evaluations;return mock::check(mock::Stage::Evaluate);
}
extern "C" sl::Result slFreeResources(sl::Feature feature,const sl::ViewportHandle&) {
    ++mock::calls;mock::freed.push_back(feature);return sl::Result::eOk;
}
extern "C" sl::Result slGetFeatureFunction(sl::Feature,const char *name,void *&function) {
    ++mock::calls;
    if(!std::strcmp(name,"slDLSSSetOptions"))function=reinterpret_cast<void*>(mock::srOptions);
    else if(!std::strcmp(name,"slDLSSDSetOptions"))function=reinterpret_cast<void*>(mock::rrOptions);
    else if(!std::strcmp(name,"slDLSSGetOptimalSettings"))function=reinterpret_cast<void*>(mock::srOptimal);
    else if(!std::strcmp(name,"slDLSSDGetOptimalSettings"))function=reinterpret_cast<void*>(mock::rrOptimal);
    else if(!std::strcmp(name,"slReflexSetOptions"))function=reinterpret_cast<void*>(mock::reflexOptions);
    else if(!std::strcmp(name,"slDLSSGSetOptions"))function=reinterpret_cast<void*>(mock::fgOptions);
    else if(!std::strcmp(name,"slDLSSGGetState"))function=reinterpret_cast<void*>(mock::fgState);
    else if(!std::strcmp(name,"slReflexSleep"))function=reinterpret_cast<void*>(mock::sleep);
    else if(!std::strcmp(name,"slPCLSetMarker"))function=reinterpret_cast<void*>(mock::marker);
    else return sl::Result::eErrorFeatureMissing;
    return sl::Result::eOk;
}

static void require(bool value,const char *message) {
    if(!value)throw std::runtime_error(message);
}
static void prepare() {
    using namespace hwmodern;
    state=DlaaState{};
    state.initialized=state.deviceReady=state.supported=true;
    state.rrRequested=state.rrSupported=state.rrPluginRequested=true;
    state.mode=3;state.width=1280;state.height=720;
    state.output.Attach(reinterpret_cast<ID3D12Resource*>(&mock::resource));
    mock::failure=mock::Stage::None;mock::result=sl::Result::eWarnOutOfVRAM;
    mock::calls=mock::tags=mock::evaluations=mock::barriers=0;
    mock::fgOffCalls=mock::fgOnCalls=0;
    mock::tokenCalls=mock::sleepCalls=mock::fgStateCalls=0;
    mock::framesPresented=2;mock::fgStatus=sl::DLSSGStatus::eOk;
    mock::markers.clear();mock::constantFrames.clear();mock::tagTypes.clear();mock::tagExtents.clear();
    mock::freed.clear();mock::lifecycles.clear();
}
static bool evaluate(bool rr) {
    auto *commands=reinterpret_cast<ID3D12GraphicsCommandList*>(&mock::commands);
    auto *resource=reinterpret_cast<ID3D12Resource*>(&mock::resource);
    if(rr)return hwmodern::dlaaEvaluateRayReconstruction(commands,resource,640,360,
        DXGI_FORMAT_R16G16B16A16_FLOAT,resource,resource,resource,resource,resource,
        45.f,16.f/9,0,0,false);
    return hwmodern::dlaaEvaluate(commands,resource,640,360,
        DXGI_FORMAT_R16G16B16A16_FLOAT,resource,resource,45.f,16.f/9,0,0,false);
}
static bool prepareGeneration() {
    auto *commands=reinterpret_cast<ID3D12GraphicsCommandList*>(&mock::commands);
    auto *resource=reinterpret_cast<ID3D12Resource*>(&mock::resource);
    return hwmodern::dlaaFrameGenerationPrepare(commands,resource,resource,resource,
        640,360,1280,720,45,.25f,-.25f,false,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
int main(){try {
    mock::resourceTable[2]=reinterpret_cast<void*>(mock::release);
    mock::commandsTable[26]=reinterpret_cast<void*>(mock::barrier);
    using namespace hwmodern;
    for(bool rr:{false,true}) {
        prepare();
        require(evaluate(rr),"healthy evaluation failed");
        require(mock::tags==1&&mock::evaluations==1,"healthy call sequence changed");
        require(mock::lifecycles.size()==(rr?8:4),"healthy tag set changed");
        for(size_t tag=0;tag<mock::lifecycles.size();++tag)
            require(mock::lifecycles[tag]==(tag==1?sl::ResourceLifecycle::eValidUntilEvaluate:
                sl::ResourceLifecycle::eValidUntilPresent),
                "resource lifetime does not match persistent inputs/shared fallback output");
        require(evaluate(rr)&&mock::evaluations==2,"healthy second frame stopped evaluating");
        require(state.outputReadable,"healthy output was not readable");
        require(mock::freed.empty(),"feature resources freed on an in-flight evaluation");
        dlaaReleaseOutput();
        require(mock::freed.size()==(rr?2:1),"output lifecycle did not release used feature resources once");
        const auto freeCount=mock::freed.size();dlaaReleaseOutput();
        require(mock::freed.size()==freeCount,"feature resources freed twice");
    }
    for(bool rr:{false,true})for(auto result:{sl::Result::eWarnOutOfVRAM,sl::Result::eErrorNGXFailed})
    for(auto stage:{mock::Stage::Token,mock::Stage::Constants,mock::Stage::SrOptions,
                    mock::Stage::RrOptions,mock::Stage::Tags,mock::Stage::Evaluate}) {
        if(!rr&&stage==mock::Stage::RrOptions)continue;
        prepare();mock::failure=stage;mock::result=result;
        require(!evaluate(rr),"injected Streamline failure was accepted");
        const auto calls=mock::calls;
        for(unsigned frame=0;frame<1000;++frame)require(!evaluate(rr),"failed configuration retried");
        require(mock::calls==calls,"failed configuration kept calling/tagging/allocating through Streamline");
        require(mock::freed.empty(),"failure freed feature resources before GPU drain");
        require(!state.outputReadable,"failed output remained readable");
        if(rr)require(!dlaaRayReconstructionAvailable(),"failed RR remained available for frame evaluation");
        else {
            require(!dlaaActive()&&dlaaAvailable()&&!dlaaRayReconstructionAvailable(),
                "failed SR activity/capability distinction changed");
            require(!evaluate(true)&&mock::calls==calls,
                "exhausted SR fallback left another RR allocation path enabled");
        }
        mock::failure=mock::Stage::None;
        dlaaSetMode(4);
        require(evaluate(rr),"explicit configuration change did not permit recovery");
        dlaaReleaseOutput();
    }
    prepare();mock::failure=mock::Stage::Evaluate;
    require(!evaluate(true),"RR warning did not activate SR fallback");
    mock::failure=mock::Stage::None;
    require(evaluate(false),"RR failure blocked healthy SR fallback");
    require(!dlaaRayReconstructionAvailable(),"SR fallback accidentally reset RR failure");
    dlaaSetRayReconstructionEnabled(false);dlaaSetRayReconstructionEnabled(true);
    require(dlaaRayReconstructionAvailable()&&evaluate(true),"explicit RR toggle did not permit recovery");
    dlaaReleaseOutput();
    prepare();mock::failure=mock::Stage::Evaluate;
    require(!evaluate(false),"SR warning did not select native fallback");
    const auto exhaustedCalls=mock::calls;
    require(!evaluate(true)&&mock::calls==exhaustedCalls,"SR failure retried allocation through RR");
    mock::failure=mock::Stage::None;
    dlaaSetRayReconstructionEnabled(false);dlaaSetRayReconstructionEnabled(true);
    require(evaluate(true)&&dlaaActive(),"explicit RR configuration change did not recover exhausted output");
    dlaaReleaseOutput();
    prepare();state.frameGenerationSupported=true;
    dlaaSetFrameGenerationEnabled(false);
    require(mock::fgOffCalls==1&&state.frameGenerationOptionsKnown,
        "supported device did not initialize explicit Off before its first menu Present");
    const auto initialOffCalls=mock::calls;
    for(unsigned frame=0;frame<1000;++frame) {
        dlaaSetFrameGenerationEnabled(false);dlaaFrameGenerationBeginFrame(frame);
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
    }
    require(mock::calls==initialOffCalls&&mock::fgOffCalls==1,
        "already initialized Off repeated SDK options or markers every frame");
    dlaaReleaseDevice();
    // Accepted options alone do not prove generated presentations. FG uses
    // its own token/guides with native/FSR/XeSS as well as NVIDIA reconstruction.
    for(int mode:{0,2,3}) {
        prepare();state.mode=mode;state.frameGenerationSupported=true;
        if(mode==0)state.supported=false; // SR support is not an FG prerequisite.
        dlaaSetFrameGenerationEnabled(true);
        require(!dlaaFrameGenerationActive()&&mock::fgOnCalls==0,"FG claimed active before any scene/present");
        const auto configured=mock::calls;
        for(unsigned repeat=0;repeat<1000;++repeat)dlaaSetFrameGenerationEnabled(true);
        require(mock::calls==configured,"unchanged FG request repeatedly set SDK options");
        dlaaFrameGenerationBeginFrame(42);
        require(prepareGeneration(),"provider-independent FG preparation failed");
        require(mock::fgOnCalls==1&&!dlaaFrameGenerationActive(),"accepted FG options were mistaken for actual activity");
        require(mock::tagTypes.size()==3&&mock::tagTypes[0]==sl::kBufferTypeDepth&&
            mock::tagTypes[1]==sl::kBufferTypeMotionVectors&&mock::tagTypes[2]==sl::kBufferTypeHUDLessColor,
            "FG did not tag all independent inputs");
        require(mock::tagExtents[0].width==640&&mock::tagExtents[2].width==1280,
            "FG HUDless extent did not match output resolution");
        for(auto lifecycle:mock::lifecycles)require(lifecycle==sl::ResourceLifecycle::eValidUntilPresent,
            "FG requested volatile input clones");
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        require(dlaaFrameGenerationActive()&&dlaaFrameGenerationPresentedFrames()==2&&
            dlaaFrameGenerationApplicationFrames()==1,"runtime presentation evidence was not used");
        require(mock::tokenCalls==1&&mock::sleepCalls==1&&mock::markers.size()==4,
            "FG did not use one token and four render/present markers");
        require(mock::markers[0].first==sl::PCLMarker::eRenderSubmitStart&&
            mock::markers[1].first==sl::PCLMarker::eRenderSubmitEnd&&
            mock::markers[2].first==sl::PCLMarker::ePresentStart&&mock::markers[3].first==sl::PCLMarker::ePresentEnd,
            "FG render/present markers were out of order");
        for(const auto &marker:mock::markers)require(marker.second==42,"Present marker frame did not match constants");
        dlaaFrameGenerationBeginFrame(43); // Menu/loading: no valid scene inputs.
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        require(!dlaaFrameGenerationActive()&&mock::fgOffCalls==1,"menu retained stale active FG guides");
        dlaaFrameGenerationBeginFrame(44);require(prepareGeneration(),"gameplay FG did not resume after menu");
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        require(dlaaFrameGenerationActive()&&mock::fgOnCalls==2,"gameplay FG did not reconfigure exactly once");
        require(mock::freed.empty(),"FG hook freed in-flight feature resources");
        dlaaReleaseDevice();
    }
    prepare();state.frameGenerationSupported=true;dlaaSetFrameGenerationEnabled(true);
    dlaaFrameGenerationBeginFrame(50);
    mock::failure=mock::Stage::Evaluate;
    require(!evaluate(true),"RR failure was not latched on a shared FG frame");
    mock::failure=mock::Stage::None;
    require(evaluate(false)&&prepareGeneration(),"RR fallback blocked SR or independent FG");
    require(mock::tokenCalls==1,"RR/SR fallback and FG created different presented-frame tokens");
    dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
    require(dlaaFrameGenerationActive()&&!dlaaRayReconstructionAvailable(),"FG reset an RR failure latch");
    dlaaReleaseDevice();
    for(auto stage:{mock::Stage::FgOptions,mock::Stage::FgState,mock::Stage::Tags,
                   mock::Stage::Constants,mock::Stage::Sleep,mock::Stage::Marker}) {
        prepare();state.frameGenerationSupported=true;dlaaSetFrameGenerationEnabled(true);
        mock::failure=stage;
        dlaaFrameGenerationBeginFrame(60);prepareGeneration();
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        require(dlaaFrameGenerationFailed()&&!dlaaFrameGenerationActive(),"FG failure was not latched");
        if(stage==mock::Stage::FgOptions)
            require(mock::fgOnCalls==1&&mock::fgOffCalls==1&&dlaaFrameGenerationDisableFailed(),
                "partially failed On did not attempt Off once or failed disable was hidden");
        const auto calls=mock::calls;
        for(unsigned frame=0;frame<1000;++frame) {
            dlaaFrameGenerationBeginFrame(61+frame);require(!prepareGeneration(),"failed FG retried");
            dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        }
        require(mock::calls==calls&&mock::freed.empty(),"FG failure kept allocating/calling or freed in-flight resources");
        mock::failure=mock::Stage::None;
        require(evaluate(false),"FG failure disabled independent healthy SR");
        dlaaSetFrameGenerationEnabled(false);
        require(dlaaFrameGenerationFailed(),"automatic provider disable cleared the runtime failure and would oscillate Auto");
        dlaaSetFrameGenerationEnabled(true);
        require(!dlaaFrameGenerationDisableFailed(),"successful explicit disable retry retained uncertain SDK state");
        dlaaFrameGenerationBeginFrame(2000);require(prepareGeneration(),"explicit FG toggle did not recover");
        dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
        require(dlaaFrameGenerationActive(),"recovered FG lacked actual presentation evidence");
        dlaaReleaseDevice();
    }
    prepare();state.frameGenerationSupported=true;dlaaSetFrameGenerationEnabled(true);
    dlaaFrameGenerationBeginFrame(70);require(prepareGeneration(),"runtime-status fixture failed to prepare");
    mock::fgStatus=sl::DLSSGStatus::eFailCommonConstantsInvalid;
    dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
    require(dlaaFrameGenerationFailed()&&mock::fgOffCalls==1&&!dlaaFrameGenerationActive(),
        "invalid runtime status was reported ACTIVE");
    dlaaReleaseDevice();
    prepare();state.frameGenerationSupported=true;dlaaSetFrameGenerationEnabled(true);
    dlaaFrameGenerationBeginFrame(80);
    mock::failure=mock::Stage::Evaluate;require(!evaluate(false),"SR failure was not latched");
    mock::failure=mock::Stage::None;require(prepareGeneration(),"latched SR incorrectly blocked independent FG");
    dlaaFrameGenerationPresentBegin();dlaaFrameGenerationPresentEnd(true);
    require(dlaaFrameGenerationActive()&&!dlaaActive(),"independent FG reset SR failure or did not activate");
    const auto srCalls=mock::calls;
    for(unsigned frame=0;frame<1000;++frame)require(!evaluate(false)&&!evaluate(true),"FG allowed exhausted SR/RR retries");
    require(mock::calls==srCalls,"FG reset bounded reconstruction-failure behavior");
    dlaaReleaseDevice();
    prepare();
    require(!dlaaEvaluate(nullptr,nullptr,0,0,DXGI_FORMAT_UNKNOWN,nullptr,nullptr,45,1,0,0,false),
        "invalid resources were accepted");
    require(!state.evaluateFailed&&mock::calls==0&&evaluate(false),
        "missing frame resources permanently disabled healthy DLSS");
    dlaaReleaseOutput();
    std::puts("Streamline CPU mock tests passed: persistent tags, bounded failure/warning retries, healthy frames, SR fallback, explicit recovery, drained lifecycle release.");
    return 0;
}catch(const std::exception &e){std::fprintf(stderr,"Streamline CPU mock failure: %s\n",e.what());return 1;}}
