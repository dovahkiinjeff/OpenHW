"""Execute the production shared DDS cache with the real DirectXTex decoder."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def production_source(path):
    source = Path(path).read_text(encoding='utf-8')
    start = source.index('extern "C" int hwModernGraphicsDecodeDds(')
    end = source.index('extern "C" unsigned int hwModernGraphicsModelTextureCap(', start)
    return source[start:end]


HARNESS = r'''
#define NOMINMAX
#include <windows.h>
#include <DirectXTex.h>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>
struct { double decodeMilliseconds=0; unsigned long long decodeCalls=0,decodeBytes=0,sharedHits=0,sharedMisses=0; } textureLoadMetrics;
struct TextureMetricTimer { explicit TextureMetricTimer(double*) {} };
bool hwDiagnosticsDisabled() { return false; }
'''


CASES = r'''
DirectX::Blob image(unsigned char red, unsigned size=4) {
    DirectX::ScratchImage pixels;assert(SUCCEEDED(pixels.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,size,size,1,1)));
    auto* p=pixels.GetImage(0,0,0);for(unsigned i=0;i<size*size;++i){p->pixels[4*i]=red;p->pixels[4*i+1]=90;p->pixels[4*i+2]=100;p->pixels[4*i+3]=255;}
    DirectX::Blob result;assert(SUCCEEDED(DirectX::SaveToDDSMemory(*p,DirectX::DDS_FLAGS_NONE,result)));return result;
}
unsigned char* get(DirectX::Blob& data,const char* key,unsigned char red,unsigned side=4){
    unsigned w=0,h=0;unsigned char* p=nullptr;
    assert(hwModernGraphicsDecodeDdsShared(data.GetBufferPointer(),unsigned(data.GetBufferSize()),key,&w,&h,&p));
    assert(w==side&&h==side&&p&&p[0]==red&&p[1]==90&&p[2]==100&&p[3]==255);return p;
}
int main(){
    // Small budget exercises the same production eviction code without large
    // test allocations. The runtime keeps its existing 128 MiB budget.
    auto first=image(80),changed=image(120);
    for(unsigned i=0;i<50;++i){auto* p=get(first,i%2?"r1/test.dds":"R1\\TEST.DDS",80);p[0]=0;hwModernGraphicsFreeDecodedDds(p);}
    assert(textureLoadMetrics.decodeCalls==1&&textureLoadMetrics.sharedHits==49);
    assert(first.GetBufferSize()==changed.GetBufferSize());
    auto* oldLease=get(first,"r1/test.dds",80);
    auto* newLease=get(changed,"r1/test.dds",120);
    assert(oldLease!=newLease&&oldLease[0]==80);hwModernGraphicsFreeDecodedDds(oldLease);hwModernGraphicsFreeDecodedDds(newLease);
    auto before=textureLoadMetrics.decodeCalls;auto* current=get(changed,"r1/test.dds",120);hwModernGraphicsFreeDecodedDds(current);assert(textureLoadMetrics.decodeCalls==before);
    for(unsigned i=0;i<50;++i){std::string key="entry-"+std::to_string(i);auto* p=get(first,key.c_str(),80);hwModernGraphicsFreeDecodedDds(p);assert(sharedDdsCacheBytes<=sharedDdsCacheLimit);}
    before=textureLoadMetrics.decodeCalls;current=get(first,"entry-49",80);hwModernGraphicsFreeDecodedDds(current);assert(textureLoadMetrics.decodeCalls==before);
    current=get(first,"entry-0",80);hwModernGraphicsFreeDecodedDds(current);assert(textureLoadMetrics.decodeCalls==before+1);
    auto large=image(40,32);before=textureLoadMetrics.decodeCalls;
    for(unsigned i=0;i<2;++i){auto* p=get(large,"oversized",40,32);hwModernGraphicsFreeDecodedDds(p);}
    assert(textureLoadMetrics.decodeCalls==before+2&&sharedDdsCacheBytes<=sharedDdsCacheLimit);
    unsigned w=0,h=0;unsigned char* p=nullptr;assert(!hwModernGraphicsDecodeDdsShared(nullptr,0,"invalid",&w,&h,&p));hwModernGraphicsFreeDecodedDds(nullptr);
    std::printf("PASS: repeated reuse, writable lease isolation, source-edit invalidation, normalized paths, LRU eviction, bounded memory, oversized bypass and invalid input\n");
}
'''


def build_and_run(source, cases, folder):
    cache = (ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
    setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1])/'Common7/Tools/VsDevCmd.bat'
    dep = Path(re.search(r'VCPKG_INSTALLED_DIR:PATH=(.+)', cache)[1])/'x64-windows'
    (folder/'test.cpp').write_text(HARNESS+source+cases, encoding='utf-8')
    command = (f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && '
               f'cl /nologo /EHsc /std:c++17 /MD /O2 /I"{dep / "include"}" '
               f'test.cpp /Fe:test.exe /link /LIBPATH:"{dep / "lib"}" '
               'DirectXTex.lib ole32.lib windowscodecs.lib"')
    built = subprocess.run(command,cwd=folder,capture_output=True,text=True)
    if built.returncode:raise RuntimeError(built.stdout+built.stderr)
    env=dict(os.environ);env['PATH']=str(dep/'bin')+os.pathsep+env['PATH']
    return subprocess.run([str(folder/'test.exe')],cwd=folder,env=env,capture_output=True,text=True,check=True)


class SharedDdsCacheTests(unittest.TestCase):
    def test_real_decodes_and_cache_lifetimes(self):
        source=production_source(ROOT/'src/Modern/ModernGraphics.cpp')
        source=source.replace('constexpr size_t sharedDdsCacheLimit = 128u * 1024u * 1024u;',
                              'constexpr size_t sharedDdsCacheLimit = 1024u;')
        with tempfile.TemporaryDirectory(prefix='openhw-dds-cache-') as tmp:
            result=build_and_run(source,CASES,Path(tmp))
            self.assertIn('PASS:',result.stdout)
            print(result.stdout.strip())


if __name__=='__main__':unittest.main()
