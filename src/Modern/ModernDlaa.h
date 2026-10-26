#ifndef HW_MODERN_DLAA_H
#define HW_MODERN_DLAA_H

#if defined(_WIN32)

#include <d3d12.h>
#include <dxgi1_6.h>

namespace hwmodern
{

void dlaaEarlyInitialize(void);
void dlaaSetMode(int mode);
void dlaaSetRayReconstructionEnabled(bool enabled);
void dlaaSetFrameGenerationEnabled(bool enabled);
bool dlaaFrameGenerationAvailable(void);
bool dlaaFrameGenerationActive(void);
bool dlaaFrameGenerationFailed(void);
bool dlaaFrameGenerationDisableFailed(void);
const char *dlaaFrameGenerationReason(void);
unsigned long long dlaaFrameGenerationPresentedFrames(void);
unsigned long long dlaaFrameGenerationApplicationFrames(void);
/* One SL frame token is shared by reconstruction, FG and its Present markers.
   These hooks do not depend on the selected spatial/temporal upscaler. */
void dlaaFrameGenerationBeginFrame(unsigned long long frameId);
bool dlaaFrameGenerationPrepare(ID3D12GraphicsCommandList *commands,
    ID3D12Resource *hudless, ID3D12Resource *depth, ID3D12Resource *motion,
    unsigned int inputWidth, unsigned int inputHeight,
    unsigned int outputWidth, unsigned int outputHeight,
    float verticalFieldOfViewDegrees, float jitterX, float jitterY,
    bool resetHistory, D3D12_RESOURCE_STATES hudlessState,
    D3D12_RESOURCE_STATES depthState, D3D12_RESOURCE_STATES motionState);
void dlaaFrameGenerationPresentBegin(void);
void dlaaFrameGenerationPresentEnd(bool presentSucceeded);
ID3D12Resource *dlaaOutputResource(void);
bool dlaaSetDevice(ID3D12Device *device, IDXGIAdapter1 *adapter);
void dlaaReleaseDevice(void);
bool dlaaCreateOutput(ID3D12DescriptorHeap *shaderVisibleHeap,
                      unsigned int descriptorSize,
                      unsigned int outputSrvIndex,
                      unsigned int width, unsigned int height);
bool dlaaGetOptimalRenderSize(int mode,
                              unsigned int outputWidth,
                              unsigned int outputHeight,
                              unsigned int *renderWidth,
                              unsigned int *renderHeight);
void dlaaReleaseOutput(void);
bool dlaaEvaluate(ID3D12GraphicsCommandList *commands,
                  ID3D12Resource *inputColor,
                  unsigned int inputWidth,
                  unsigned int inputHeight,
                  DXGI_FORMAT inputFormat,
                  ID3D12Resource *depth,
                  ID3D12Resource *motion,
                  float verticalFieldOfViewDegrees,
                  float aspectRatio,
                  float jitterX,
                  float jitterY,
                  bool resetHistory);
bool dlaaEvaluateRayReconstruction(
                  ID3D12GraphicsCommandList *commands,
                  ID3D12Resource *inputColor,
                  unsigned int inputWidth,
                  unsigned int inputHeight,
                  DXGI_FORMAT inputFormat,
                  ID3D12Resource *depth,
                  ID3D12Resource *motion,
                  ID3D12Resource *diffuseAlbedo,
                  ID3D12Resource *specularAlbedo,
                  ID3D12Resource *normalRoughness,
                  float verticalFieldOfViewDegrees,
                  float aspectRatio,
                  float jitterX,
                  float jitterY,
                  bool resetHistory);
bool dlaaAvailable(void);
bool dlaaActive(void);
bool dlaaRayReconstructionAvailable(void);
}

#endif

#endif
