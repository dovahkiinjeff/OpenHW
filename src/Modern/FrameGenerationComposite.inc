// Included after the presenter and shader compiler definitions. Each descriptor
// table belongs to a fenced flight slot; rewriting the ordinary presenter table
// here would change the HUD-less draw that was recorded earlier in this list.
bool recordFrameGenerationComposite(UINT backBufferIndex,const PresenterConstants &constants)
{
    auto &frame=presenter.frames[presenter.activeFrameIndex];
    if(!frame.frameGenerationHudless || presenter.frameGenerationCompositeFailed)return false;
    if(!presenter.frameGenerationCompositePipeline) {
        presenter.frameGenerationCompositeFailed=true;
        D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors=3;range.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER roots[2]={};
        roots[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        roots[0].DescriptorTable.NumDescriptorRanges=1;roots[0].DescriptorTable.pDescriptorRanges=&range;
        roots[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        roots[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        roots[1].Constants.Num32BitValues=1;roots[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler={};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rootDesc={};rootDesc.NumParameters=2;rootDesc.pParameters=roots;
        rootDesc.NumStaticSamplers=1;rootDesc.pStaticSamplers=&sampler;
        rootDesc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> serialized,errors,vs,ps;
        if(FAILED(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors)) ||
            FAILED(presenter.device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),
                IID_PPV_ARGS(&presenter.frameGenerationCompositeRoot))))return false;
        if(!compileShader("CompositeVS","vs_5_0",vs,"renderer.shader.fg-composite-vs","Frame generation display vertex shader",nullptr,
                "FrameGenerationComposite.hlsl",hwmodern::frameGenerationCompositeShader) ||
           !compileShader("CompositePS","ps_5_0",ps,"renderer.shader.fg-composite-ps","Frame generation display composite",nullptr,
                "FrameGenerationComposite.hlsl",hwmodern::frameGenerationCompositeShader))return false;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline={};pipeline.pRootSignature=presenter.frameGenerationCompositeRoot.Get();
        pipeline.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pipeline.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        pipeline.SampleMask=UINT_MAX;pipeline.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
        pipeline.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pipeline.RasterizerState.DepthClipEnable=TRUE;
        pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        auto &blend=pipeline.BlendState.RenderTarget[0];
        blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;
        blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;
        blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;
        pipeline.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipeline.NumRenderTargets=1;pipeline.RTVFormats[0]=presenterFormat;pipeline.SampleDesc.Count=1;
        if(FAILED(presenter.device->CreateGraphicsPipelineState(&pipeline,
            IID_PPV_ARGS(&presenter.frameGenerationCompositePipeline))))return false;
        presenter.frameGenerationCompositeFailed=false;
    }
    if(!frame.frameGenerationCompositeSrvs) {
        D3D12_DESCRIPTOR_HEAP_DESC heap={};heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.NumDescriptors=3;heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if(FAILED(presenter.device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&frame.frameGenerationCompositeSrvs))))return false;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE cpu=frame.frameGenerationCompositeSrvs->GetCPUDescriptorHandleForHeapStart();
    for(ID3D12Resource *texture:{presenter.sourceTexture.Get(),presenter.worldTexture.Get(),frame.frameGenerationHudless.Get()}) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv={};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format=texture->GetDesc().Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
        presenter.device->CreateShaderResourceView(texture,&srv,cpu);cpu.ptr+=presenter.srvDescriptorSize;
    }
    auto *commands=presenter.commandList.Get();auto *backBuffer=presenter.backBuffers[backBufferIndex].Get();
    transition(commands,backBuffer,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto rtv=presenter.rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(backBufferIndex)*presenter.rtvDescriptorSize;
    commands->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
    D3D12_VIEWPORT viewport={0,0,float(presenter.width),float(presenter.height),0,1};
    D3D12_RECT scissor={0,0,LONG(presenter.width),LONG(presenter.height)};
    commands->RSSetViewports(1,&viewport);commands->RSSetScissorRects(1,&scissor);
    commands->SetPipelineState(presenter.frameGenerationCompositePipeline.Get());
    commands->SetGraphicsRootSignature(presenter.frameGenerationCompositeRoot.Get());
    ID3D12DescriptorHeap *heaps[]={frame.frameGenerationCompositeSrvs.Get()};commands->SetDescriptorHeaps(1,heaps);
    commands->SetGraphicsRootDescriptorTable(0,frame.frameGenerationCompositeSrvs->GetGPUDescriptorHandleForHeapStart());
    commands->SetGraphicsRoot32BitConstants(1,1,&constants.displayControls,0);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);commands->DrawInstanced(3,1,0,0);
    transition(commands,backBuffer,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
    return true;
}
