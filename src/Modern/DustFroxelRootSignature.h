#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "DustScreenBindings.h"

namespace hwmodern {
// Shared by the production pipeline and its offscreen PSO validation. The
// packed grid is t16, seven descriptors after the t9 volume-buffer binding.
inline HRESULT createDustFroxelRootSignature(ID3D12Device *device,ID3D12RootSignature **output)
{
    D3D12_DESCRIPTOR_RANGE dust={};dust.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    dust.NumDescriptors=particleDensityGridSlot+1;dust.BaseShaderRegister=9;
    dust.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE uav={};uav.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;uav.NumDescriptors=1;
    uav.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE noise={};noise.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;noise.NumDescriptors=1;
    noise.BaseShaderRegister=6;noise.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[6]={};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges=1;parameters[0].DescriptorTable.pDescriptorRanges=&dust;
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable.NumDescriptorRanges=1;parameters[2].DescriptorTable.pDescriptorRanges=&uav;
    parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[3].Descriptor.ShaderRegister=18;
    parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[4].Descriptor.ShaderRegister=1;
    parameters[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[5].DescriptorTable.NumDescriptorRanges=1;parameters[5].DescriptorTable.pDescriptorRanges=&noise;
    D3D12_STATIC_SAMPLER_DESC sampler={};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderRegister=1;
    D3D12_ROOT_SIGNATURE_DESC desc={};desc.NumParameters=6;desc.pParameters=parameters;
    desc.NumStaticSamplers=1;desc.pStaticSamplers=&sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> serialized,errors;
    HRESULT result=D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors);
    if(FAILED(result))return result;
    return device->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(output));
}
}
