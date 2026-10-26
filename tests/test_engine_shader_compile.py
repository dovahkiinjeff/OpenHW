"""Compile the actual embedded raster shaders without launching the game."""
import ctypes
import os
from pathlib import Path
import re
import unittest


@unittest.skipUnless(os.name == 'nt', 'Windows shader compiler required')
class EngineShaders(unittest.TestCase):
    def test_raster_shaders(self):
        root=Path(__file__).resolve().parents[1]/'src'/'Modern'
        compiler=ctypes.WinDLL('d3dcompiler_47.dll').D3DCompile
        compiler.restype=ctypes.c_long
        compiler.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,
            ctypes.c_void_p,ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p,
            ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_void_p)]
        def method(blob,index,result,*args):
            table=ctypes.cast(blob,ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
            return ctypes.WINFUNCTYPE(result,ctypes.c_void_p,*args)(table[index])
        for filename,name,entries in [
            ('LegacyD3D12GL.cpp','legacyShader',[('LegacyVS','vs_5_0'),('LegacyPS','ps_5_0')]),
            ('LegacyD3D11Renderer.inc','shader',[('VS','vs_5_0'),('PS','ps_5_0')]),
            ('ModernGraphics.cpp','presenterShader',[('PresentVS','vs_5_0'),('PresentPS','ps_5_0')])]:
            source=re.search(r'const char\s+(?:\*\s*)?'+name+r'(?:\[\])?\s*=\s*R"\((.*?)\)";',
                (root/filename).read_text(),re.S).group(1).encode()
            for entry,profile in entries:
                with self.subTest(shader=filename,entry=entry):
                    code=ctypes.c_void_p(); errors=ctypes.c_void_p()
                    status=compiler(source,len(source),filename.encode(),None,None,
                        entry.encode(),profile.encode(),0,0,ctypes.byref(code),ctypes.byref(errors))
                    diagnostic=''
                    if errors:
                        pointer=method(errors,3,ctypes.c_void_p)(errors)
                        length=method(errors,4,ctypes.c_size_t)(errors)
                        diagnostic=ctypes.string_at(pointer,length).decode(errors='replace')
                        method(errors,2,ctypes.c_ulong)(errors)
                    if code: method(code,2,ctypes.c_ulong)(code)
                    self.assertGreaterEqual(status,0,diagnostic)
