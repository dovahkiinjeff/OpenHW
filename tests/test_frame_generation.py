"""Compile production FG control flow against actual SDK types and mocked I/O.

No window, runtime DLL, GPU device or command queue is created. Win32/COM calls
at the external resource boundary are substituted; the actual descriptor,
callback, error-latching and lifetime logic remains unchanged.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FrameGeneration(unittest.TestCase):
    def test_actual_sdk_descriptors_callbacks_failure_and_lifetime(self):
        sdk_candidates = [ROOT / '.deps/fidelityfx',
                          ROOT.parent / 'Homeworld-RTX/.deps/fidelityfx']
        sdk = next((path / 'Kits/FidelityFX' for path in sdk_candidates
                    if (path / 'Kits/FidelityFX/api/include/ffx_api.hpp').exists()
                    and (path / 'Kits/FidelityFX/framegeneration/include/ffx_framegeneration.h').exists()), None)
        self.assertIsNotNone(sdk, 'Installed FidelityFX SDK required')
        source = (ROOT / 'src/Modern/ModernFrameGeneration.cpp').read_text()
        source = '\n'.join(line for line in source.splitlines()
                           if not line.startswith('#include'))
        # Substitute only the COM/OS calls that would otherwise touch hardware.
        for original, replacement in (
            ('hudlessColor->GetDesc()', 'mockDescription(hudlessColor)'),
            ('depth->GetDesc()', 'mockDescription(depth)'),
            ('motion->GetDesc()', 'mockDescription(motion)'),
            ('commands->GetDevice(IID_PPV_ARGS(&device))', 'mockDevice(&device)'),
            ('device->Release()', 'mockReleaseDevice(device)'),
            ('(*swapchain)->Release()', 'mockReleaseProxy(*swapchain)'),
        ):
            self.assertIn(original, source)
            source = source.replace(original, replacement)
        harness = r'''
#include "ModernFrameGeneration.h"
#include "FGZoomCapture.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <windows.h>
#include <ffx_api.hpp>
#include <dx12/ffx_api_dx12.hpp>
#include <ffx_framegeneration.h>
#include <dx12/ffx_api_framegeneration_dx12.h>
static int creates,configures,prepares,generations,destroys,disables,resourceReads;
static uint64_t failType;
static bool enabled;
static bool failDisable,finalDrained;
static ffxConfigureDescFrameGeneration savedConfig{};
static ffxDispatchDescFrameGenerationPrepareV2 savedPrepare{};
static std::vector<int> order;
static ID3D12Resource *color=reinterpret_cast<ID3D12Resource*>(0x1000);
static ID3D12Resource *depthInput=reinterpret_cast<ID3D12Resource*>(0x2000);
static ID3D12Resource *motionInput=reinterpret_cast<ID3D12Resource*>(0x3000);
static UINT imageWidth=320,imageHeight=180;
static DXGI_FORMAT imageFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
static void require(bool value){if(!value)std::abort();}
namespace hwmodern {
// Readback boundary only: this CPU harness never owns a D3D command list.
void fgZoomCaptureRecordGenerated(ID3D12GraphicsCommandList*,unsigned long long,
    ID3D12Resource*,ID3D12Resource*,bool,unsigned) {}
}
static D3D12_RESOURCE_DESC mockDescription(ID3D12Resource *r){
    ++resourceReads;D3D12_RESOURCE_DESC d{};d.Width=imageWidth;d.Height=imageHeight;
    d.Format=r==color?imageFormat:r==depthInput?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32_FLOAT;
    return d;
}
static HRESULT mockDevice(ID3D12Device **d){*d=reinterpret_cast<ID3D12Device*>(0x4000);return S_OK;}
static void mockReleaseDevice(ID3D12Device*){}
static void mockReleaseProxy(IDXGISwapChain4*){}
static HMODULE mockModule(LPCWSTR){return reinterpret_cast<HMODULE>(1);}
static DWORD mockEnvironment(LPCWSTR,LPWSTR,DWORD){return 0;}
static FfxApiResource mockResource(ID3D12Resource *r,uint32_t state){
    FfxApiResource resource{};resource.resource=r;resource.state=state;return resource;
}
#define GetModuleHandleW mockModule
#define LoadLibraryW mockModule
#define GetEnvironmentVariableW mockEnvironment
#define ffxApiGetResourceDX12 mockResource
extern "C" ffxReturnCode_t ffxCreateContext(ffxContext *context,ffxCreateContextDescHeader *h,
                                           const ffxAllocationCallbacks*){
    ++creates;require(h&&h->type);
    if(h->type==failType)return FFX_API_RETURN_ERROR_MEMORY;
    if(h->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12){
        auto c=reinterpret_cast<ffxCreateContextDescFrameGenerationSwapChainForHwndDX12*>(h);
        require(c->gameQueue&&c->hwnd&&c->dxgiFactory&&c->swapchain&&!*c->swapchain);
        require(h->pNext&&h->pNext->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12);
        require(reinterpret_cast<ffxCreateContextDescFrameGenerationSwapChainVersionDX12*>(h->pNext)->version==FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION);
        *c->swapchain=reinterpret_cast<IDXGISwapChain4*>(0x5000);*context=reinterpret_cast<void*>(1);
    }else{
        require(h->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION);
        auto c=reinterpret_cast<ffxCreateContextDescFrameGeneration*>(h);
        require(c->flags==0&&c->backBufferFormat==ffxApiGetSurfaceFormatDX12(DXGI_FORMAT_R8G8B8A8_UNORM));
        bool backend=false,version=false,hudless=false;
        for(auto p=h->pNext;p;p=p->pNext){
            require(p->type);
            if(p->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12)backend=true;
            if(p->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION){
                version=true;require(reinterpret_cast<ffxCreateContextDescFrameGenerationVersion*>(p)->version==FFX_FRAMEGENERATION_VERSION);}
            if(p->type==FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_HUDLESS){
                hudless=true;require(reinterpret_cast<ffxCreateContextDescFrameGenerationHudless*>(p)->hudlessBackBufferFormat==ffxApiGetSurfaceFormatDX12(imageFormat));}
        }
        require(backend&&version&&hudless);*context=reinterpret_cast<void*>(2);order.push_back(1);
    }
    return FFX_API_RETURN_OK;
}
extern "C" ffxReturnCode_t ffxConfigure(ffxContext*,const ffxConfigureDescHeader *h){
    ++configures;require(h&&h->type);
    if(h->type==FFX_API_CONFIGURE_DESC_TYPE_GLOBALDEBUG)return FFX_API_RETURN_OK;
    require(h->type==FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION);
    auto c=reinterpret_cast<const ffxConfigureDescFrameGeneration*>(h);
    // The real provider sets frameGenEnabled before a fallible swapchain
    // configure. Reproduce that partial success rather than an atomic error.
    if(c->frameGenerationEnabled&&h->type==failType){enabled=true;return FFX_API_RETURN_ERROR_MEMORY;}
    require(c->swapChain);
    if(!c->frameGenerationEnabled&&failDisable)return FFX_API_RETURN_ERROR_RUNTIME_ERROR;
    enabled=c->frameGenerationEnabled;
    if(enabled){require(c->frameGenerationCallback&&c->frameGenerationCallbackUserContext);
        require(!c->allowAsyncWorkloads&&!c->presentCallback);
        require(c->HUDLessColor.resource==color&&c->HUDLessColor.state==FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        savedConfig=*c;order.push_back(2);
    }else{require(!c->frameGenerationCallback&&!c->HUDLessColor.resource);++disables;order.push_back(4);}
    return FFX_API_RETURN_OK;
}
extern "C" ffxReturnCode_t ffxDispatch(ffxContext*,const ffxDispatchDescHeader *h){
    require(h&&h->type);
    if(h->type==failType)return FFX_API_RETURN_ERROR_MEMORY;
    if(h->type==FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2){
        ++prepares;savedPrepare=*reinterpret_cast<const ffxDispatchDescFrameGenerationPrepareV2*>(h);
        require(savedPrepare.depth.state==FFX_API_RESOURCE_STATE_COMPUTE_READ);
        require(savedPrepare.motionVectors.state==FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        require(savedPrepare.depth.resource==depthInput&&savedPrepare.motionVectors.resource==motionInput);
    }else if(h->type==FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION){++generations;}
    else {require(h->type==FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12);finalDrained=true;}
    return FFX_API_RETURN_OK;
}
extern "C" ffxReturnCode_t ffxDestroyContext(ffxContext *c,const ffxAllocationCallbacks*){
    ++destroys;if(*c==reinterpret_cast<void*>(2)){require(!enabled||finalDrained);order.push_back(5);}else {order.push_back(6);enabled=false;}
    *c=nullptr;return FFX_API_RETURN_OK;
}
'''
        harness += '\n' + source
        harness += r'''
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace hwmodern;
    // This is the original regression: C descriptors have no automatic type.
    ffxConfigureDescFrameGeneration raw{};CHECK(raw.header.type==0);
    auto factory=reinterpret_cast<IDXGIFactory*>(0x6000);
    auto queue=reinterpret_cast<ID3D12CommandQueue*>(0x7000);
    auto commands=reinterpret_cast<ID3D12GraphicsCommandList*>(0x8000);
    IDXGISwapChain4 *swapchain=nullptr;DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=320;desc.Height=180;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    CHECK(fsrFrameGenerationCreateSwapchain(factory,queue,reinterpret_cast<HWND>(1),&desc,&swapchain));
    CHECK(fsrFrameGenerationAvailable()&&!fsrFrameGenerationActive());
    FsrFrameGenerationCamera camera;camera.nearPlane=.05f;camera.farPlane=500000;
    camera.position[0]=123;camera.up[2]=.25f;
    auto prepare=[&](uint64_t id){return fsrFrameGenerationPrepare(commands,swapchain,color,depthInput,motionInput,
        imageWidth,imageHeight,imageWidth,imageHeight,.25f,-.125f,1.2f,7.5f,false,id,&camera);};
    auto present=[&](){ffxDispatchDescFrameGeneration d{};d.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
        d.frameID=savedConfig.frameID;d.numGeneratedFrames=1;
        return savedConfig.frameGenerationCallback(&d,savedConfig.frameGenerationCallbackUserContext);};
    fsrFrameGenerationSetEnabled(true);CHECK(prepare(0));CHECK(!fsrFrameGenerationActive());
    CHECK(savedPrepare.cameraNear==.05f&&savedPrepare.cameraFar==500000&&savedPrepare.cameraPosition[0]==123);
    CHECK(savedPrepare.cameraUp[2]==.25f&&savedPrepare.frameTimeDelta==7.5f);
    CHECK(savedPrepare.jitterOffset.x==-.25f&&savedPrepare.jitterOffset.y==.125f&&savedPrepare.reset);
    CHECK(present()==FFX_API_RETURN_OK&&fsrFrameGenerationActive());
    CHECK(prepare(1)&&!savedPrepare.reset);CHECK(present()==FFX_API_RETURN_OK);
    auto stats=fsrFrameGenerationStats();CHECK(stats.generationCallbacks==2&&stats.generationDispatches==2&&stats.generatedFrameRequests==2);
    fsrFrameGenerationSuspend();CHECK(!enabled&&!fsrFrameGenerationActive());
    CHECK(fsrFrameGenerationStats().requested);CHECK(prepare(4)&&savedPrepare.reset);CHECK(present()==0);
    // Each heavy runtime failure latches. A thousand frames cannot allocate or retry.
    const uint64_t stages[]={FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION,
        FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2,FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION};
    for(auto stage:stages){
        fsrFrameGenerationSetEnabled(false);fsrFrameGenerationSetEnabled(true);failType=stage;
        if(stage==FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION){CHECK(prepare(10));CHECK(present()!=0);}else CHECK(!prepare(10));
        CHECK(fsrFrameGenerationFailed()&&!fsrFrameGenerationActive());
        int c=creates,p=prepares,g=generations;
        for(int i=0;i<1000;++i){fsrFrameGenerationSetEnabled(true);CHECK(!prepare(11+i));}
        CHECK(creates==c&&prepares==p&&generations==g&&!enabled);
        failType=0;fsrFrameGenerationSetEnabled(false);fsrFrameGenerationSetEnabled(true);
        CHECK(prepare(2000)&&present()==0&&!fsrFrameGenerationFailed());
    }
    // Format changes first disable/flush/destroy the old context, then create.
    int oldDestroys=destroys;imageFormat=DXGI_FORMAT_R16G16B16A16_FLOAT;
    CHECK(prepare(2001)&&destroys==oldDestroys+1&&present()==0);
    // Off failure means the SDK may still be active: latch that uncertainty,
    // block repeated SDK calls and permit an explicit settings edge retry.
    failDisable=true;fsrFrameGenerationSuspend();
    CHECK(enabled&&fsrFrameGenerationDisableFailed()&&fsrFrameGenerationStats().disableFailed);
    int oldConfigures=configures;
    for(int i=0;i<1000;++i){fsrFrameGenerationSetEnabled(true);fsrFrameGenerationSuspend();CHECK(!prepare(3000+i));}
    CHECK(configures==oldConfigures&&enabled);
    fsrFrameGenerationSetEnabled(false);CHECK(configures==oldConfigures+1&&fsrFrameGenerationDisableFailed());
    // An explicit ON cannot clear uncertain ownership until Off succeeds;
    // that failed ON edge is consumed so ordinary frame calls stay bounded.
    oldConfigures=configures;fsrFrameGenerationSetEnabled(true);
    CHECK(configures==oldConfigures+1&&fsrFrameGenerationDisableFailed()&&fsrFrameGenerationFailed());
    oldConfigures=configures;
    for(int i=0;i<1000;++i){fsrFrameGenerationSetEnabled(true);CHECK(!prepare(4500+i));}
    CHECK(configures==oldConfigures);
    failDisable=false;fsrFrameGenerationSetEnabled(false);fsrFrameGenerationSetEnabled(true);fsrFrameGenerationSuspend();
    CHECK(!enabled&&!fsrFrameGenerationDisableFailed());CHECK(prepare(4000)&&present()==0);
    // The final release must not skip its drain merely because Off failed.
    failDisable=true;fsrFrameGenerationSuspend();oldConfigures=configures;
    fsrFrameGenerationRelease();CHECK(configures==oldConfigures+1&&finalDrained&&!enabled);
    failDisable=false;finalDrained=false;
    swapchain=nullptr;CHECK(fsrFrameGenerationCreateSwapchain(factory,queue,reinterpret_cast<HWND>(1),&desc,&swapchain));
    fsrFrameGenerationSetEnabled(false);fsrFrameGenerationSetEnabled(true);CHECK(prepare(5000)&&present()==0);
    // Context allocation failure is also a bounded failure, with no active proxy config.
    fsrFrameGenerationRelease();CHECK(!enabled&&!fsrFrameGenerationAvailable());
    CHECK(order[order.size()-2]==5&&order.back()==6);
    swapchain=nullptr;CHECK(fsrFrameGenerationCreateSwapchain(factory,queue,reinterpret_cast<HWND>(1),&desc,&swapchain));
    fsrFrameGenerationSetEnabled(false);fsrFrameGenerationSetEnabled(true);
    failType=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;CHECK(!prepare(0));
    int oldCreates=creates;for(int i=0;i<1000;++i)CHECK(!prepare(i));CHECK(creates==oldCreates);
    failType=0;fsrFrameGenerationRelease();
    // Enabling the setting cannot repair a failed HWND proxy or erase its
    // original failure stage; only a new successful Create can do that.
    fsrFrameGenerationSetEnabled(false);swapchain=nullptr;
    failType=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12;
    CHECK(!fsrFrameGenerationCreateSwapchain(factory,queue,reinterpret_cast<HWND>(1),&desc,&swapchain));
    CHECK(!swapchain&&fsrFrameGenerationFailed());
    const char *creationStage=fsrFrameGenerationReason();
    fsrFrameGenerationSetEnabled(true);CHECK(fsrFrameGenerationFailed());
    CHECK(!std::strcmp(creationStage,fsrFrameGenerationReason()));
    oldCreates=creates;CHECK(!prepare(0));CHECK(creates==oldCreates);
    failType=0;fsrFrameGenerationRelease();
    std::puts("PASS: actual SDK descriptor types/version chains, Present generation callback, SDR/motion/camera contracts, disable/drain order and1000-frame failure latches");
    return 0;
}
'''
        compilers = list(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob(
            'Microsoft Visual Studio/*/Community/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC required for CPU frame-generation test')
        compiler = sorted(compilers)[-1]
        vcvars = compiler.parents[7] / 'VC/Auxiliary/Build/vcvars64.bat'
        with tempfile.TemporaryDirectory(prefix='hw-frame-generation-') as directory:
            directory = Path(directory)
            (directory / 'framegen.cpp').write_text(harness)
            includes = [ROOT / 'src/Modern', sdk / 'api/include', sdk / 'framegeneration/include']
            include_flags = ' '.join(f'/I"{path}"' for path in includes)
            command = (f'call "{vcvars}" >nul && "{compiler}" /nologo /std:c++17 /EHsc '
                       f'{include_flags} framegen.cpp /Fe:framegen.exe && framegen.exe')
            result = subprocess.run(command, shell=True, cwd=directory,
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout)


if __name__ == '__main__':
    unittest.main()
