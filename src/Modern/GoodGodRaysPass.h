#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <algorithm>
#include <utility>

namespace hwmodern {
// Appended to the per-flight t9 table; t19 must never alias a cloud/grid SRV.
inline constexpr UINT godRayFieldSlot = 10u;
inline constexpr DXGI_FORMAT godRayFieldFormat = DXGI_FORMAT_R32G32_FLOAT;

struct GodRayFieldResources {
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv;
    UINT width = 0, height = 0;
    UINT failedWidth = 0, failedHeight = 0;
};

// Call only after this flight slot's fence completed. Changing quality then
// replaces only this slot, without waiting for the whole graphics queue.
inline HRESULT ensureGodRayField(ID3D12Device *device, GodRayFieldResources &field,
                                 UINT sceneWidth, UINT sceneHeight, int quality)
{
    if (!device || !sceneWidth || !sceneHeight) return E_INVALIDARG;
    const UINT divisor = quality < 2 ? 4u : 2u;
    const UINT width = std::max(1u, (sceneWidth + divisor - 1u) / divisor);
    const UINT height = std::max(1u, (sceneHeight + divisor - 1u) / divisor);
    if (field.texture && field.width == width && field.height == height) return S_OK;
    if (field.failedWidth == width && field.failedHeight == height) return E_OUTOFMEMORY;
    D3D12_HEAP_PROPERTIES heap = {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width; desc.Height = height;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = godRayFieldFormat; desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
        IID_PPV_ARGS(&texture));
    if (SUCCEEDED(result) && !field.rtv) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heapDesc.NumDescriptors = 1;
        result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&field.rtv));
    }
    if (FAILED(result)) {
        field.failedWidth = width; field.failedHeight = height;
        return result;
    }
    texture->SetName(L"OpenHW bounded god rays: illumination and linear depth");
    device->CreateRenderTargetView(texture.Get(), nullptr,
        field.rtv->GetCPUDescriptorHandleForHeapStart());
    field.texture = std::move(texture);
    field.width = width; field.height = height;
    field.failedWidth = field.failedHeight = 0;
    return S_OK;
}

inline void writeGodRayFieldSrv(ID3D12Device *device,
    D3D12_CPU_DESCRIPTOR_HANDLE heapBase, UINT stride, UINT perFrameBase,
    ID3D12Resource *texture)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = godRayFieldFormat;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    heapBase.ptr += static_cast<SIZE_T>(perFrameBase + godRayFieldSlot) * stride;
    device->CreateShaderResourceView(texture, &srv, heapBase);
}

inline void recordGodRayField(ID3D12GraphicsCommandList *commands,
    const GodRayFieldResources &field, ID3D12PipelineState *pipeline,
    ID3D12RootSignature *root, ID3D12DescriptorHeap *srvs, UINT stride,
    UINT motionBase, UINT dustBase, const void *constants, UINT constantDwords)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = field.texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commands->ResourceBarrier(1, &barrier);
    const auto rtv = field.rtv->GetCPUDescriptorHandleForHeapStart();
    commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const D3D12_VIEWPORT viewport = {0,0,static_cast<float>(field.width),
        static_cast<float>(field.height),0,1};
    const D3D12_RECT scissor = {0,0,static_cast<LONG>(field.width),static_cast<LONG>(field.height)};
    commands->RSSetViewports(1, &viewport);
    commands->RSSetScissorRects(1, &scissor);
    commands->SetPipelineState(pipeline);
    commands->SetGraphicsRootSignature(root);
    ID3D12DescriptorHeap *heaps[] = {srvs};
    commands->SetDescriptorHeaps(1, heaps);
    auto handle = srvs->GetGPUDescriptorHandleForHeapStart();
    commands->SetGraphicsRootDescriptorTable(0, handle);
    handle.ptr += static_cast<UINT64>(motionBase) * stride;
    commands->SetGraphicsRootDescriptorTable(1, handle);
    handle = srvs->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(dustBase) * stride;
    commands->SetGraphicsRootDescriptorTable(3, handle);
    commands->SetGraphicsRoot32BitConstants(2, constantDwords, constants, 0);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3,1,0,0);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commands->ResourceBarrier(1, &barrier);
}
}
