// Included by the actual production DXR GPU fixture, after a warmed scene.
// Serial and parallel CPU recording submit the same A/B/C dependency chain.
hwmodern::FrameWorker rayWorker(L"Offline DXR replay"),postWorker(L"Offline post replay");
ComPtr<ID3D12CommandAllocator> preAllocator,postAllocator;
ComPtr<ID3D12GraphicsCommandList> preCommands,postCommands;
require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&preAllocator))),"pre allocator");
require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&postAllocator))),"post allocator");
require(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,preAllocator.Get(),nullptr,IID_PPV_ARGS(&preCommands))),"pre commands");
require(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,postAllocator.Get(),nullptr,IID_PPV_ARGS(&postCommands))),"post commands");
preCommands->Close();postCommands->Close();
const auto outputDesc=state.output->GetDesc();
D3D12_PLACED_SUBRESOURCE_FOOTPRINT outputLayout={};UINT outputRows=0;UINT64 outputRowBytes=0,outputBytes=0;
device->GetCopyableFootprints(&outputDesc,0,1,0,&outputLayout,&outputRows,&outputRowBytes,&outputBytes);
ComPtr<ID3D12Resource> outputReadback,dependencyInput,dependencyMid,dependencyReadback;
require(createBuffer(outputBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,outputReadback),"output readback");
const unsigned pattern=0xa17e56c3u;
require(createUpload(&pattern,sizeof(pattern),dependencyInput),"dependency source");
require(createBuffer(4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,dependencyMid),"dependency default buffer");
require(createBuffer(4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,dependencyReadback),"dependency readback");
std::vector<unsigned char> serialPixels;
std::thread::id rayThread,postThread;
for(unsigned replay=0;replay<12;++replay) {
    const bool parallel=(replay&1)!=0;
    raytracingSetFrameSlot(replay%3);raytracingBeginFrame();raytracingBeginScene(60,1,identity);
    raytracingSubmitGeometry(&mesh,vertices,3,12,indices,1,6,0,&surface,sizeof(surface),transform,baseColor,3);
    raytracingEndScene();
    state.frameSeed=0x1230u;state.previousSceneHash=~buildSceneHash();state.accumulationFrame=0;
    state.materialFrameSerial=11;
    require(SUCCEEDED(preAllocator->Reset())&&SUCCEEDED(preCommands->Reset(preAllocator.Get(),nullptr)),"pre reset");
    require(SUCCEEDED(postAllocator->Reset())&&SUCCEEDED(postCommands->Reset(postAllocator.Get(),nullptr)),"post reset");
    auto rayRecord=[&]{
        if(parallel)rayThread=std::this_thread::get_id();
        require(raytracingRecord(commands.Get(),sky.Get(),nullptr,0,nullptr,0),"threaded production ray dispatch");
        require(SUCCEEDED(commands->Close()),"ray close");
    };
    auto preRecord=[&]{
        preCommands->CopyBufferRegion(dependencyMid.Get(),0,dependencyInput.Get(),0,4);
        transitionResource(preCommands.Get(),dependencyMid.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
        require(SUCCEEDED(preCommands->Close()),"pre close");
    };
    if(parallel) {
        hwmodern::FrameJob rayJob(rayWorker,rayRecord);preRecord();require(rayJob.wait(),"DXR worker exception");
    } else {preRecord();rayRecord();}
    auto postRecord=[&]{
        if(parallel)postThread=std::this_thread::get_id();
        transitionResource(postCommands.Get(),state.output.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION source={};source.pResource=state.output.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION target={};target.pResource=outputReadback.Get();target.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;target.PlacedFootprint=outputLayout;
        postCommands->CopyTextureRegion(&target,0,0,0,&source,nullptr);
        transitionResource(postCommands.Get(),state.output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transitionResource(postCommands.Get(),state.motion.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        postCommands->CopyBufferRegion(dependencyReadback.Get(),0,dependencyMid.Get(),0,4);
        transitionResource(postCommands.Get(),dependencyMid.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        require(SUCCEEDED(postCommands->Close()),"post close");
    };
    if(parallel){hwmodern::FrameJob postJob(postWorker,postRecord);require(postJob.wait(),"post worker exception");}
    else postRecord();
    ID3D12CommandList *batch[]={preCommands.Get(),commands.Get(),postCommands.Get()};
    queue->ExecuteCommandLists(3,batch);
    require(SUCCEEDED(queue->Signal(fence.Get(),++serial)),"worker signal");
    require(SUCCEEDED(fence->SetEventOnCompletion(serial,event))&&WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"worker GPU completion");
    require(SUCCEEDED(device->GetDeviceRemovedReason()),"worker GPU device removed");
    void *mapped=nullptr;require(SUCCEEDED(dependencyReadback->Map(0,nullptr,&mapped)),"dependency map");
    require(*static_cast<unsigned *>(mapped)==pattern,"cross-list A-to-C dependency violated");dependencyReadback->Unmap(0,nullptr);
    require(SUCCEEDED(outputReadback->Map(0,nullptr,&mapped)),"worker output map");
    std::vector<unsigned char> pixels;
    for(UINT row=0;row<outputRows;++row){auto begin=static_cast<unsigned char *>(mapped)+outputLayout.Offset+row*outputLayout.Footprint.RowPitch;
        pixels.insert(pixels.end(),begin,begin+outputRowBytes);}
    outputReadback->Unmap(0,nullptr);
    if(parallel)require(pixels==serialPixels,"threading changed path-traced output pixels");else serialPixels=std::move(pixels);
    require(SUCCEEDED(allocator->Reset())&&SUCCEEDED(commands->Reset(allocator.Get(),nullptr)),"ray allocator reset");
}
require(rayThread!=std::this_thread::get_id()&&postThread!=std::this_thread::get_id()&&rayThread!=postThread,"GPU stages did not use distinct CPU threads");
std::cout<<"PASS: 12 serial/threaded production DXR frames, exact pixel parity, distinct ray/post threads and ordered A/B/C GPU dependencies\n";
