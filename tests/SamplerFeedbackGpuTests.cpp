#include "SamplerFeedbackTexture.h"
#include <windows.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
using namespace hwmodern;
static void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static void check(HRESULT hr,const char* s){if(FAILED(hr)){std::cerr<<s<<" HRESULT="<<std::hex<<hr<<std::dec<<'\n';throw std::runtime_error(s);}}
static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v={};v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;v.Transition.pResource=r;v.Transition.StateBefore=a;v.Transition.StateAfter=b;v.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;return v;}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE t){D3D12_HEAP_PROPERTIES h={};h.Type=t;h.CreationNodeMask=h.VisibleNodeMask=1;return h;}
int main(int argc,char** argv){try{
 require(argc==2,"shader path required");
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"factory");
 ComPtr<IDXGIAdapter1> adapter;check(factory->EnumAdapterByGpuPreference(0,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)),"adapter");
 ComPtr<ID3D12Device> device;check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)),"device");
 D3D12_COMMAND_QUEUE_DESC qd={};ComPtr<ID3D12CommandQueue> queue;check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
 SamplerFeedbackContext context;if(!context.initialize(device.Get(),queue.Get(),2)){std::cout<<"SKIP: sampler feedback + SM6.5 + tiled tier2 unavailable\n";return 0;}
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.NumDescriptors=5;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> descriptors;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors)),"descriptors");
 UINT stride=device->GetDescriptorHandleIncrementSize(hd.Type);
 auto cpu=[&](unsigned i){auto h=descriptors->GetCPUDescriptorHandleForHeapStart();h.ptr+=i*stride;return h;};
 auto gpu=[&](unsigned i){auto h=descriptors->GetGPUDescriptorHandleForHeapStart();h.ptr+=i*stride;return h;};
 context.createNullViews(cpu(2),stride,2);
 D3D12_RESOURCE_DESC textureDesc={};textureDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;textureDesc.Width=textureDesc.Height=1024;textureDesc.DepthOrArraySize=4;textureDesc.MipLevels=11;textureDesc.SampleDesc.Count=1;textureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 SamplerFeedbackTexture texture;if(!texture.create(context,textureDesc,0,cpu(2))){ComPtr<ID3D12InfoQueue> info;device.As(&info);if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T bytes=0;info->GetMessage(i,nullptr,&bytes);std::vector<char> memory(bytes);auto message=(D3D12_MESSAGE*)memory.data();info->GetMessage(i,message,&bytes);std::cerr<<message->pDescription<<'\n';}require(false,"reserved feedback texture create");}
 auto smallDesc=textureDesc;smallDesc.Width=4;smallDesc.Height=4;smallDesc.MipLevels=3;
 SamplerFeedbackTexture fallbackTexture;require(!fallbackTexture.create(context,smallDesc,1,cpu(3)),"small texture should use exact committed fallback");
 ComPtr<ID3D12Resource> committed;auto defaultHeap=heap(D3D12_HEAP_TYPE_DEFAULT);
 check(device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&textureDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&committed)),"committed texture");
 D3D12_SHADER_RESOURCE_VIEW_DESC srv={};srv.Format=textureDesc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2DArray.ArraySize=4;srv.Texture2DArray.MipLevels=11;
 device->CreateShaderResourceView(texture.resource(),&srv,cpu(0));device->CreateShaderResourceView(committed.Get(),&srv,cpu(1));
 constexpr UINT samples=262144;
 auto buffer=[&](UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags){D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=d.DepthOrArraySize=d.MipLevels=d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;auto h=heap(type);ComPtr<ID3D12Resource> r;check(device->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"buffer");return r;};
 auto output=buffer(samples*16,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 auto readback=buffer(samples*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
 D3D12_UNORDERED_ACCESS_VIEW_DESC uav={};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=samples;uav.Buffer.StructureByteStride=16;device->CreateUnorderedAccessView(output.Get(),nullptr,&uav,cpu(4));
 D3D12_DESCRIPTOR_RANGE ranges[3]={};ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;ranges[0].NumDescriptors=2;
 ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;ranges[1].NumDescriptors=2;ranges[1].RegisterSpace=2;
 ranges[2].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;ranges[2].NumDescriptors=1;
 D3D12_ROOT_PARAMETER params[4]={};for(UINT i=0;i<3;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable.NumDescriptorRanges=1;params[i].DescriptorTable.pDescriptorRanges=ranges+i;}
 params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[3].Constants.Num32BitValues=2;
 D3D12_STATIC_SAMPLER_DESC sampler={};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
 D3D12_ROOT_SIGNATURE_DESC rootDesc={};rootDesc.NumParameters=4;rootDesc.pParameters=params;rootDesc.NumStaticSamplers=1;rootDesc.pStaticSamplers=&sampler;
 ComPtr<ID3DBlob> rootBytes,errors;check(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&rootBytes,&errors),"root serialize");
 ComPtr<ID3D12RootSignature> root;check(device->CreateRootSignature(0,rootBytes->GetBufferPointer(),rootBytes->GetBufferSize(),IID_PPV_ARGS(&root)),"root create");
 std::ifstream shaderFile(argv[1],std::ios::binary);std::vector<char> shader((std::istreambuf_iterator<char>(shaderFile)),{});require(!shader.empty(),"shader bytes");
 D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc={};psoDesc.pRootSignature=root.Get();psoDesc.CS={shader.data(),shader.size()};ComPtr<ID3D12PipelineState> pso;check(device->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&pso)),"pso");
 ComPtr<ID3D12Fence> fence;check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");UINT64 serial=0;
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");
 auto wait=[&](){const auto value=++serial;check(queue->Signal(fence.Get(),value),"signal");check(fence->SetEventOnCompletion(value,event),"fence event");require(WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"GPU timeout");return value;};
 ComPtr<ID3D12CommandAllocator> allocator;check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
 ComPtr<ID3D12GraphicsCommandList1> command;check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),pso.Get(),IID_PPV_ARGS(&command)),"commands");
 auto reset=[&](){check(allocator->Reset(),"allocator reset");check(command->Reset(allocator.Get(),pso.Get()),"commands reset");};
 auto submit=[&](){check(command->Close(),"close");ID3D12CommandList* lists[]={command.Get()};queue->ExecuteCommandLists(1,lists);return wait();};
 auto bind=[&](UINT mode){ID3D12DescriptorHeap* heaps[]={descriptors.Get()};command->SetDescriptorHeaps(1,heaps);command->SetComputeRootSignature(root.Get());command->SetComputeRootDescriptorTable(0,gpu(0));command->SetComputeRootDescriptorTable(1,gpu(2));command->SetComputeRootDescriptorTable(2,gpu(4));UINT constants[]={mode,samples};command->SetComputeRoot32BitConstants(3,2,constants,0);};
 UINT64 uploadBytes=0;std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(44);device->GetCopyableFootprints(&textureDesc,0,44,0,layouts.data(),nullptr,nullptr,&uploadBytes);
 auto upload=buffer(uploadBytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
 void* mapped=nullptr;check(upload->Map(0,nullptr,&mapped),"upload map");
 for(UINT channel=0;channel<4;++channel)for(UINT mip=0;mip<11;++mip){auto& l=layouts[channel*11+mip];for(UINT y=0;y<l.Footprint.Height;++y)for(UINT x=0;x<l.Footprint.Width;++x){unsigned char rgba[4]={(unsigned char)((x*13+y*7+channel*53+mip*17)%256),(unsigned char)((x*3+y*19+channel*37+mip*23)%256),(unsigned char)((x*5+y*31+channel*17+mip*11)%256),(unsigned char)((x*23+y*3+channel*43+mip*29)%256)};std::memcpy((char*)mapped+l.Offset+y*l.Footprint.RowPitch+x*4,rgba,4);}}
 upload->Unmap(0,nullptr);
 auto copy=[&](ID3D12Resource* target,const std::vector<SamplerFeedbackMipRange>& ranges){for(const auto& range:ranges)for(UINT slice=0;slice<4;++slice)for(UINT mip=range.firstMip;mip<range.firstMip+range.mipCount;++mip){UINT index=slice*11+mip;D3D12_TEXTURE_COPY_LOCATION src={},dst={};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=layouts[index];dst.pResource=target;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=index;command->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}};
 auto verify=[&](){auto b=transition(output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);command->ResourceBarrier(1,&b);command->CopyBufferRegion(readback.Get(),0,output.Get(),0,samples*16);b=transition(output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);command->ResourceBarrier(1,&b);};
 auto assertExact=[&](){void* p=nullptr;check(readback->Map(0,nullptr,&p),"readback map");const auto* values=(const UINT*)p;for(UINT i=0;i<samples*4;++i)require(values[i]==0,"reserved texture changed sampled output bits");D3D12_RANGE noWrite={};readback->Unmap(0,&noWrite);};
 std::vector<SamplerFeedbackMipRange> missing;require(texture.mapAll(context,missing)&&missing.size()==5,"initial whole mip mapping");
 D3D12_RESOURCE_BARRIER alias={};alias.Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;alias.Aliasing.pResourceAfter=texture.resource();command->ResourceBarrier(1,&alias);
 require(!texture.readyForSampling(),"uninitialized mapped texture accepted for sampling");std::vector<SamplerFeedbackMipRange> pendingAgain;require(texture.mapAll(context,pendingAgain)&&pendingAgain.size()==missing.size(),"failed upload retry lost mapped but uninitialized ranges");
 copy(texture.resource(),missing);texture.uploadsRecorded(missing);copy(committed.Get(),{{0,11}});
 D3D12_RESOURCE_BARRIER ready[]={transition(texture.resource(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),transition(committed.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)};command->ResourceBarrier(2,ready);
 bind(0);command->Dispatch(samples/64,1,1);verify();submit();assertExact();
 const auto fullBytes=texture.mappedBytes();require(fullBytes==344ull*65536,"driver tiling byte accounting");
 reset();bind(2);require(texture.prepareFeedback(context,0),"feedback readback prepare");require(texture.beginFeedback(command.Get(),gpu(2),0),"feedback begin");command->Dispatch(samples/64,1,1);require(texture.endFeedback(command.Get(),0),"feedback decode/readback");verify();auto token=submit();texture.feedbackSubmitted(0,token);
 require(!texture.consumeFeedback(0,token-1),"feedback consumed before completion");require(!texture.prepareFeedback(context,0),"readback overwritten while pending");require(texture.consumeFeedback(0,token),"feedback consume after completion");assertExact();
 require(texture.finestWarmMip()<=3&&texture.finestWarmMip()>=1,"hardware min feedback unexpected");
 require(texture.trimInactive(context,false)==0&&texture.fullyMapped(),"active/inflight texture changed residency");
 const auto trimmed=texture.trimInactive(context,true);require(trimmed>0&&!texture.fullyMapped(),"inactive feedback did not unmap fine mips");require(texture.mappedBytes()+trimmed==fullBytes,"trimmed physical byte accounting");
 require(context.pendingRetiredBytes()>=trimmed,"tile heap freed before queue retirement fence");wait();context.collectRetiredHeaps();require(context.pendingRetiredBytes()==0,"physical heaps retained after safe retirement");
 reset();require(texture.mapAll(context,missing)&&!missing.empty(),"reactivation did not restore missing full quality mips");command->ResourceBarrier(1,&alias);auto b=transition(texture.resource(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);command->ResourceBarrier(1,&b);copy(texture.resource(),missing);texture.uploadsRecorded(missing);b=transition(texture.resource(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);command->ResourceBarrier(1,&b);bind(0);command->Dispatch(samples/64,1,1);verify();submit();assertExact();require(texture.readyForSampling()&&texture.mappedBytes()==fullBytes,"reactivation not fully resident");

 // Real GPU timestamps compare capture cost, not CPU submission latency. A
 // production capture is duty-cycled; non-capture frames use the original
 // SM6.3 library and therefore contain no extra feedback branch or writes.
 D3D12_QUERY_HEAP_DESC queryDesc={};queryDesc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;queryDesc.Count=4;ComPtr<ID3D12QueryHeap> queries;check(device->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&queries)),"queries");auto timings=buffer(32,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);UINT64 frequency=0;check(queue->GetTimestampFrequency(&frequency),"timestamp frequency");
 reset();bind(1);command->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);for(unsigned i=0;i<20;++i)command->Dispatch(samples/64,1,1);command->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
 require(texture.prepareFeedback(context,1),"timing feedback prepare");bind(2);command->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2);require(texture.beginFeedback(command.Get(),gpu(2),1),"timing feedback begin");for(unsigned i=0;i<20;++i)command->Dispatch(samples/64,1,1);require(texture.endFeedback(command.Get(),1),"timing feedback end");command->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,3);command->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,4,timings.Get(),0);token=submit();texture.feedbackSubmitted(1,token);require(texture.consumeFeedback(1,token),"timing consume");check(timings->Map(0,nullptr,&mapped),"timestamp map");auto ticks=(const UINT64*)mapped;double baseline=1000.0*(ticks[1]-ticks[0])/frequency,withFeedback=1000.0*(ticks[3]-ticks[2])/frequency;timings->Unmap(0,nullptr);
 reset();bind(4);require(texture.prepareFeedback(context,2),"channel-specific feedback prepare");require(texture.beginFeedback(command.Get(),gpu(2),2),"channel-specific feedback begin");command->Dispatch(samples/64,1,1);require(texture.endFeedback(command.Get(),2),"channel-specific feedback end");verify();token=submit();texture.feedbackSubmitted(2,token);require(texture.consumeFeedback(2,token),"channel-specific feedback consume");assertExact();require(texture.finestWarmMip()<=1,"channel3 finer mip request ignored");
 reset();bind(1);require(texture.prepareFeedback(context,0),"empty feedback prepare");require(texture.beginFeedback(command.Get(),gpu(2),0),"empty feedback begin");require(texture.endFeedback(command.Get(),0),"empty feedback end");token=submit();texture.feedbackSubmitted(0,token);require(texture.consumeFeedback(0,token),"empty feedback consume");require(texture.finestWarmMip()==texture.packedFirstMip(),"unsampled material did not retain packed tail policy");
 // A NULL paired feedback view is a defined no-op, used by small/fallback
 // committed resources sharing the indexed material table.
 reset();bind(3);command->Dispatch(samples/64,1,1);verify();submit();assertExact();
 texture.releaseInactive(context);wait();context.collectRetiredHeaps();require(context.pendingRetiredBytes()==0,"abandoned mapping heaps leaked");
 ComPtr<ID3D12InfoQueue> info;device.As(&info);if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T bytes=0;info->GetMessage(i,nullptr,&bytes);std::vector<char> memory(bytes);auto message=(D3D12_MESSAGE*)memory.data();info->GetMessage(i,message,&bytes);if(message->Severity<=D3D12_MESSAGE_SEVERITY_WARNING)std::cerr<<message->pDescription<<'\n';require(message->Severity>D3D12_MESSAGE_SEVERITY_ERROR,"D3D12 debug-layer error");}
 CloseHandle(event);
 std::cout<<"PASS: hardware sampler feedback tier "<<context.tier()<<"; 4-layer mip chains, wrap/fractional LOD samples bit-identical before/after tile retirement+rehydration; active/fence/readback guards and NULL fallback verified.\n";
 std::cout<<"Full mapped bytes="<<fullBytes<<", feedback-trimmed bytes="<<trimmed<<", remaining warm bytes="<<fullBytes-trimmed<<".\n";
 std::cout<<"GPU timestamp test (20x"<<samples<<" samples): baseline="<<baseline<<"ms, feedback+clear+decode+copy="<<withFeedback<<"ms, burst excess amortized over 60 frames="<<std::max(0.0,withFeedback-baseline)/60<<"ms per frame (synthetic, not an in-game speedup).\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
