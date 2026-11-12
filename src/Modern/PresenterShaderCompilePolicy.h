#pragma once

#include <cstring>
#include <d3dcompiler.h>

namespace hwmodern {
inline bool presenterShaderNeedsFlowControl(const char *entry)
{
    return std::strcmp(entry, "DustScreenPS") == 0 ||
           std::strcmp(entry, "DustScreenGuidedPS") == 0;
}

inline UINT presenterShaderCompileFlags(const char *entry)
{
    // Keep full optimization, but preserve cloud traversal branches. The
    // flattened variant fails NVIDIA pipeline creation with E_OUTOFMEMORY
    // even on an otherwise idle device. This also applies to the FG MRT.
    return D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3 |
        (presenterShaderNeedsFlowControl(entry) ? D3DCOMPILE_PREFER_FLOW_CONTROL : 0u);
}

inline const char *presenterShaderCompileVariant(const char *entry)
{
    return presenterShaderNeedsFlowControl(entry) ? "cloud-flow-control-v1" : "";
}
} // namespace hwmodern
