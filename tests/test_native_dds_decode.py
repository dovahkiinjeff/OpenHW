"""Execute the production DDS decoder with the real DirectXTex dependency."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeDdsDecodeTests(unittest.TestCase):
    def test_native_rgba_compressed_and_rejected_input(self):
        source = (ROOT / 'src/Modern/ModernGraphics.cpp').read_text()
        decoder = 'extern "C" int hwModernGraphicsDecodeDds(' + source.split(
            'extern "C" int hwModernGraphicsDecodeDds(', 1)[1].split('\nnamespace', 1)[0]
        harness = r'''
#define NOMINMAX
#include <windows.h>
#include <DirectXTex.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
struct { double decodeMilliseconds; int decodeCalls; unsigned long long decodeBytes; } textureLoadMetrics;
struct TextureMetricTimer { TextureMetricTimer(double*) {} };
'''
        cases = r'''
void check(const DirectX::Image &source, bool exact) {
    DirectX::Blob blob;
    assert(SUCCEEDED(DirectX::SaveToDDSMemory(source,DirectX::DDS_FLAGS_NONE,blob)));
    unsigned int w=0,h=0; unsigned char *pixels=nullptr;
    assert(hwModernGraphicsDecodeDds(blob.GetBufferPointer(),(unsigned int)blob.GetBufferSize(),&w,&h,&pixels));
    assert(w==4 && h==4 && pixels);
    if(exact) assert(pixels[0]==80 && pixels[1]==90 && pixels[2]==100 && pixels[3]==255);
    std::free(pixels);
}
int main(int argc, char **argv) {
    DirectX::ScratchImage image;
    assert(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,4,4,1,1)));
    const DirectX::Image *base=image.GetImage(0,0,0);
    for(int i=0;i<16;i++) { base->pixels[i*4]=80; base->pixels[i*4+1]=90; base->pixels[i*4+2]=100; base->pixels[i*4+3]=255; }
    check(*base,true);
    DirectX::ScratchImage compressed;
    assert(SUCCEEDED(DirectX::Compress(*base,DXGI_FORMAT_BC1_UNORM,DirectX::TEX_COMPRESS_DEFAULT,1.0f,compressed)));
    check(*compressed.GetImage(0,0,0),false);
    unsigned int w=0,h=0; unsigned char *pixels=nullptr;
    assert(!hwModernGraphicsDecodeDds(nullptr,0,&w,&h,&pixels));
    unsigned char bad[128]={}; assert(!hwModernGraphicsDecodeDds(bad,128,&w,&h,&pixels));
    for(int i=1;i<argc;i++) {
        std::ifstream file(argv[i],std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
        assert(!data.empty());
        assert(hwModernGraphicsDecodeDds(data.data(),(unsigned int)data.size(),&w,&h,&pixels));
        assert(w && h && pixels); std::free(pixels);
    }
    return 0;
}
'''
        cache = (ROOT / 'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup = Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)', cache)[1]) / 'Common7/Tools/VsDevCmd.bat'
        dep = Path(re.search(r'VCPKG_INSTALLED_DIR:PATH=(.+)', cache)[1]) / 'x64-windows'
        with tempfile.TemporaryDirectory(prefix='hw-native-dds-') as tmp:
            folder = Path(tmp)
            (folder / 'test.cpp').write_text(harness + decoder + cases)
            result = subprocess.run(
                f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++17 /MD /O2 /I"{dep / "include"}" test.cpp /Fe:test.exe /link /LIBPATH:"{dep / "lib"}" DirectXTex.lib ole32.lib windowscodecs.lib"',
                cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            env = dict(os.environ)
            env['PATH'] = str(dep/'bin') + os.pathsep + env['PATH']
            # If locally generated experiment DDSs exist, test those exact bytes
            # too. The synthetic cases remain portable without any game assets.
            textures = list((ROOT/'out/hwrm-interceptor-test/runtime').rglob('*.dds'))
            subprocess.run([str(folder/'test.exe'), *map(str,textures)], cwd=folder, env=env, check=True)


if __name__ == '__main__':
    unittest.main()
