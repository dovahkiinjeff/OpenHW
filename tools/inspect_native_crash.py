"""Resolve minidump exception and stack candidates with a matching local PDB."""
import ctypes as c
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
count, directory = struct.unpack_from('<II', data, 8)
streams = {}
for i in range(count):
    kind, size, offset = struct.unpack_from('<III', data, directory + i * 12)
    streams[kind] = (offset, size)
modules = []
off, _ = streams[4]
for i in range(struct.unpack_from('<I', data, off)[0]):
    p = off + 4 + i * 108
    base, size = struct.unpack_from('<QI', data, p)
    nameoff = struct.unpack_from('<I', data, p + 20)[0]
    length = struct.unpack_from('<I', data, nameoff)[0]
    name = data[nameoff + 4:nameoff + 4 + length].decode('utf-16-le')
    modules.append((base, size, name))
exe = next(m for m in modules if Path(m[2]).name.lower() in ('homeworldmodern.exe','openhw.exe'))
off, _ = streams[6]
tid = struct.unpack_from('<I', data, off)[0]
address = struct.unpack_from('<Q', data, off + 24)[0]
print('Exception', hex(address), 'RVA', hex(address-exe[0]), 'thread', tid)
print('Exception parameters', struct.unpack_from('<QQ', data, off + 40))

class Symbol(c.Structure):
    _fields_ = [('SizeOfStruct', c.c_uint32), ('TypeIndex', c.c_uint32),
                ('Reserved', c.c_uint64 * 2), ('Index', c.c_uint32),
                ('Size', c.c_uint32), ('ModBase', c.c_uint64),
                ('Flags', c.c_uint32), ('Value', c.c_uint64),
                ('Address', c.c_uint64), ('Register', c.c_uint32),
                ('Scope', c.c_uint32), ('Tag', c.c_uint32),
                ('NameLen', c.c_uint32), ('MaxNameLen', c.c_uint32),
                ('Name', c.c_char * 1024)]

class Line(c.Structure):
    _fields_ = [('SizeOfStruct', c.c_uint32), ('Key', c.c_void_p),
                ('LineNumber', c.c_uint32), ('FileName', c.c_char_p),
                ('Address', c.c_uint64)]

dbg = c.WinDLL('dbghelp', use_last_error=True)
handle = c.c_void_p(1)
dbg.SymInitializeW.argtypes = [c.c_void_p, c.c_wchar_p, c.c_int]
dbg.SymLoadModuleExW.argtypes = [c.c_void_p, c.c_void_p, c.c_wchar_p,
                                c.c_wchar_p, c.c_uint64, c.c_uint32, c.c_void_p, c.c_uint32]
dbg.SymLoadModuleExW.restype = c.c_uint64
dbg.SymFromAddr.argtypes = [c.c_void_p, c.c_uint64, c.POINTER(c.c_uint64), c.POINTER(Symbol)]
dbg.SymGetLineFromAddr64.argtypes = [c.c_void_p, c.c_uint64, c.POINTER(c.c_uint32), c.POINTER(Line)]
dbg.SymSetOptions(0x10 | 0x200)
assert dbg.SymInitializeW(handle, str(Path(sys.argv[2]).resolve().parent), 0)
assert dbg.SymLoadModuleExW(handle, None, str(Path(sys.argv[2]).resolve()), None, exe[0], exe[1], None, 0)

def resolve(addr):
    symbol = Symbol()
    symbol.SizeOfStruct = 88
    symbol.MaxNameLen = 1024
    displacement = c.c_uint64()
    if not dbg.SymFromAddr(handle, addr, c.byref(displacement), c.byref(symbol)):
        return hex(addr) + ' unresolved ' + str(c.get_last_error())
    line = Line()
    line.SizeOfStruct = c.sizeof(Line)
    delta = c.c_uint32()
    location = ''
    if dbg.SymGetLineFromAddr64(handle, addr, c.byref(delta), c.byref(line)):
        location = f' {line.FileName.decode()}:{line.LineNumber}'
    return f'{hex(addr)} {symbol.Name.decode()}+{displacement.value:x}{location}'

print(resolve(address))
exceptionoff = streams[6][0]
contextsize, contextoff = struct.unpack_from('<II', data, exceptionoff + 160)
exceptionrsp = struct.unpack_from('<Q', data, contextoff + 152)[0]
print('Exception RSP', hex(exceptionrsp))
if 5 in streams:
    memoryoff = streams[5][0]
    for i in range(struct.unpack_from('<I', data, memoryoff)[0]):
        base, size, offset = struct.unpack_from('<QII', data, memoryoff+4+i*16)
        if base <= exceptionrsp < base+size:
            begin = exceptionrsp-base
            for j in range(begin,min(size,begin+4096)-7,8):
                candidate = struct.unpack_from('<Q', data, offset+j)[0]
                if exe[0] <= candidate < exe[0]+exe[1]:
                    print(f'ExceptionStack+{j-begin:x}',resolve(candidate))
for register, offset in [('Rax',120), ('Rcx',128), ('Rdx',136), ('Rbx',144), ('Rsi',168), ('Rdi',176), ('R8',184), ('R9',192)]:
    print(register, hex(struct.unpack_from('<Q',data,contextoff+offset)[0]))
off, _ = streams[3]
for i in range(struct.unpack_from('<I', data, off)[0]):
    p = off + 4 + i * 48
    if struct.unpack_from('<I', data, p)[0] != tid:
        continue
    stackbase, stacksize, stackoff = struct.unpack_from('<QII', data, p + 24)
    contextsize, contextoff = struct.unpack_from('<II', data, p + 40)
    rsp = exceptionrsp
    start = max(0, rsp-stackbase)
    print('RSP', hex(rsp), 'stack offset', hex(start))
    for j in range(start, min(stacksize, start+8192)-7, 8):
        candidate = struct.unpack_from('<Q', data, stackoff+j)[0]
        if exe[0] <= candidate < exe[0]+exe[1]:
            print(f'Stack+{j:x}', resolve(candidate))
