#pragma once
#include <d3d12.h>

namespace hwmodern {
constexpr UINT dustScreenFieldSlot=6;
constexpr UINT particleDensityGridSlot=7;
// t15 owns slot 6 of the per-frame t9 table. Slot 7 is the immutable
// StructuredBuffer<float> particle grid, not the retired temporal history.
inline void writeDustScreenFieldSrv(ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE heapBase,UINT descriptorStride,
    UINT perFrameBase,ID3D12Resource* texture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC view={};
    view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels=1;
    heapBase.ptr+=SIZE_T(perFrameBase+dustScreenFieldSlot)*descriptorStride;
    device->CreateShaderResourceView(texture,&view,heapBase);
}
}
