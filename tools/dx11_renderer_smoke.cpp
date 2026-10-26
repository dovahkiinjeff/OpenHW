// Hidden synthetic test window only: never launches the game or reads its saves.
#include "../src/Modern/LegacyD3D12GL.cpp"
#include <d3d11sdklayers.h>
#include <iostream>
int main() {
 SetEnvironmentVariableW(L"HW_DX11_DEBUG",L"1");
 HWND window=CreateWindowExW(0,L"STATIC",L"OpenHW DX11 validation",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!window || !hwlegacyd3d11::initialize(window,64,64))return 1;
 hwlegacyd3d11::setVSync(0);
 const float sun[]={0,0,1},white[]={1,1,1},ambient[]={0.05f,0.05f,0.05f};
 hwlegacyd3d11::setSun(true,sun,white,ambient,1,1);
 const unsigned normals[]={0xffff8080u,0xff008080u};
 static int identity;
 const unsigned controls[]={0xff0080ffu,0xff0080ffu};
 hwlegacyd3d11::registerMaterial(&identity,"synthetic normal map|rm-material",2,1,nullptr,nullptr,normals,controls);
 if(!hwlegacyd3d11::materials[&identity].rmMaterial)return 9;
 for(int frame=0;frame<3;++frame) {
  hwlegacyd3d11::beginFrame(64,64);
  hwglClearColor(0,0,0,1);hwglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  hwglMatrixMode(GL_PROJECTION);hwglLoadIdentity();hwglMatrixMode(GL_MODELVIEW);hwglLoadIdentity();
  float scene[16];hwglGetFloatv(GL_MODELVIEW_MATRIX,scene);hwlegacyd3d11::beginScene(scene);
  hwglTranslatef(0,0,-0.25f);
  hwglEnable(GL_LIGHTING);hwglEnable(GL_LIGHT0);hwglDisable(GL_CULL_FACE);
  const float light[]={0,0,1,0};hwglLightfv(GL_LIGHT0,GL_POSITION,light);
  hwglColor4f(1,1,1,1);hwglNormal3f(0,0,1);
  hwglSurfaceMaterial(&identity);
  hwglBegin(GL_QUADS);
  hwglTexCoord2f(0,0);hwglVertex3f(-0.9f,-0.9f,0);
  hwglTexCoord2f(1,0);hwglVertex3f(0.9f,-0.9f,0);
  hwglTexCoord2f(1,1);hwglVertex3f(0.9f,0.9f,0);
  hwglTexCoord2f(0,1);hwglVertex3f(-0.9f,0.9f,0);hwglEnd();
  hwlegacyd3d11::endScene();
  // Asymmetric UI markers catch the vertical inversion that a symmetric
  // lighting test cannot. Also exercise GL culling after the clip-Y change.
  hwglLoadIdentity();hwglDisable(GL_LIGHTING);hwglDisable(GL_TEXTURE_2D);
  hwglEnable(GL_CULL_FACE);hwglCullFace(GL_BACK);
  auto uiQuad=[](float bottom,float top){
   hwglBegin(GL_QUADS);hwglVertex2f(-0.8f,bottom);hwglVertex2f(-0.4f,bottom);
   hwglVertex2f(-0.4f,top);hwglVertex2f(-0.8f,top);hwglEnd();
  };
  hwglColor4f(1,0,0,1);uiQuad(0.75f,0.95f);
  hwglColor4f(0,0,1,1);uiQuad(-0.95f,-0.75f);
  // DrawPixels has already been transformed by the shared DX12 recorder.
  // Its separate branch must get the same orientation correction exactly once.
  unsigned pixelMarker[64];for(auto&pixel:pixelMarker)pixel=0xff00ff00u;
  hwglRasterPos2f(0.5f,0.5f);hwglDrawPixels(8,8,GL_RGBA,GL_UNSIGNED_BYTE,pixelMarker);
  hwlegacyd3d11::present(false);
  if(!hwlegacyd3d11::shadowData.enabled){std::cerr<<"FAIL: shadow pass not rendered\n";return 3;}
  ComPtr<ID3D11Texture2D> back,read;
  hwlegacyd3d11::swap->GetBuffer(0,IID_PPV_ARGS(&back));D3D11_TEXTURE2D_DESC desc;back->GetDesc(&desc);
  desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  if(FAILED(hwlegacyd3d11::device->CreateTexture2D(&desc,nullptr,&read)))return 4;
  hwlegacyd3d11::context->CopyResource(read.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE pixels={};
  if(FAILED(hwlegacyd3d11::context->Map(read.Get(),0,D3D11_MAP_READ,0,&pixels)))return 5;
  const unsigned char*row=static_cast<const unsigned char*>(pixels.pData)+32*pixels.RowPitch;
  unsigned lit=row[16*4],unlit=row[48*4];
  const auto*top=static_cast<const unsigned char*>(pixels.pData)+5*pixels.RowPitch+12*4;
  const auto*bottom=static_cast<const unsigned char*>(pixels.pData)+59*pixels.RowPitch+12*4;
  const auto*pixelMarkerRead=static_cast<const unsigned char*>(pixels.pData)+12*pixels.RowPitch+52*4;
  bool orientation=top[0]>240&&top[2]<10&&bottom[2]>240&&bottom[0]<10&&pixelMarkerRead[1]>240;
  if(!orientation)std::cerr<<"Orientation pixels: top="<<unsigned(top[0])<<','<<unsigned(top[1])<<','<<unsigned(top[2])
   <<" bottom="<<unsigned(bottom[0])<<','<<unsigned(bottom[1])<<','<<unsigned(bottom[2])
   <<" DrawPixels="<<unsigned(pixelMarkerRead[0])<<','<<unsigned(pixelMarkerRead[1])<<','<<unsigned(pixelMarkerRead[2])<<'\n';
  hwlegacyd3d11::context->Unmap(read.Get(),0);
  if(!orientation){std::cerr<<"FAIL: UI orientation, back-face winding or pretransformed pixels\n";return 8;}
  if(lit<unlit+40){std::cerr<<"FAIL: normal-map lighting contrast: "<<lit<<" / "<<unlit<<'\n';return 6;}
  std::cout<<"Normal-map pixels: lit="<<lit<<", opposite-normal="<<unlit<<'\n';
  hwlegacyd3d11::swap->Present(0,0);
 }
 if(FAILED(hwlegacyd3d11::device->GetDeviceRemovedReason()))return 2;
 ComPtr<ID3D11InfoQueue> queue;hwlegacyd3d11::device.As(&queue);
 if(queue)for(UINT64 i=0;i<queue->GetNumStoredMessages();i++) {
  SIZE_T size=0;queue->GetMessage(i,nullptr,&size);std::vector<char> storage(size);
  auto *message=reinterpret_cast<D3D11_MESSAGE*>(storage.data());queue->GetMessage(i,message,&size);
  if(message->Severity<=D3D11_MESSAGE_SEVERITY_ERROR){std::cerr<<"FAIL: "<<message->pDescription<<'\n';return 7;}
 }
 hwlegacyd3d11::shutdown();DestroyWindow(window);
 std::cout<<"PASS: DX11 device, SM5 shaders, shadows, normal maps, upright UI, culling, DrawPixels and repeated presentation\n";
 return 0;
}
