"""OBIG v1 writer/validator. Stored, sorted, 64-bit-offset resource package."""
from pathlib import Path
import argparse
import struct
import zlib
from extract_remastered_big import safe_path

HEADER = struct.Struct('<8sIIQ')
ENTRY = struct.Struct('<256sQII')
MAGIC = b'OBIGPK01'


def key(name):
    result = safe_path(str(name).replace('\\', '/')).as_posix().lower()
    encoded = result.encode('ascii')
    if len(encoded) > 255:
        raise ValueError('OBIG path exceeds 255 bytes: '+result)
    return result


def pack(root, output):
    root, output = Path(root).resolve(), Path(output).resolve()
    files = {}
    for path in root.rglob('*'):
        if not path.is_file():
            continue
        if path.is_symlink() or not path.resolve().is_relative_to(root):
            raise ValueError('OBIG source escapes root')
        name = key(path.relative_to(root))
        if name in files:
            raise ValueError('Duplicate OBIG path: '+name)
        if path.stat().st_size > 0x7fffffff:
            raise ValueError('Individual resource exceeds engine file-size limit')
        files[name] = path
    if not files or len(files) > 1000000:
        raise ValueError('Invalid resource count')
    temp = output.with_suffix('.obig.pending')
    output.parent.mkdir(parents=True, exist_ok=True)
    offset = HEADER.size + ENTRY.size*len(files)
    with temp.open('w+b') as stream:
        stream.write(HEADER.pack(MAGIC, 1, len(files), offset))
        stream.seek(offset)
        index = []
        for name, source in sorted(files.items()):
            start, crc = stream.tell(), 0
            with source.open('rb') as incoming:
                while chunk := incoming.read(8*1024*1024):
                    stream.write(chunk)
                    crc = zlib.crc32(chunk, crc)
            index.append((name.encode('ascii'), start, stream.tell()-start, crc))
        stream.seek(HEADER.size)
        for row in index:
            stream.write(ENTRY.pack(*row))
    validate(temp, deep=True)
    temp.replace(output)
    return len(files)


def validate(path, deep=False):
    path = Path(path)
    with path.open('rb') as stream:
        magic, version, count, data = HEADER.unpack(stream.read(HEADER.size))
        total = path.stat().st_size
        if magic != MAGIC or version != 1 or not 0 < count <= 1000000 or data != HEADER.size+ENTRY.size*count or data > total:
            raise ValueError('Invalid OBIG header')
        rows = [ENTRY.unpack(stream.read(ENTRY.size)) for _ in range(count)]
        previous, end = '', data
        for raw, offset, size, crc in rows:
            if b'\0' not in raw:
                raise ValueError('Unterminated OBIG name')
            name = raw.split(b'\0', 1)[0].decode('ascii')
            if name != key(name) or name <= previous or size > 0x7fffffff or offset < end or offset > total or size > total-offset:
                raise ValueError('Invalid OBIG resource entry')
            if deep:
                stream.seek(offset)
                remaining, actual = size, 0
                while remaining:
                    chunk = stream.read(min(remaining, 8*1024*1024))
                    if not chunk:
                        raise ValueError('Truncated OBIG resource')
                    actual = zlib.crc32(chunk, actual)
                    remaining -= len(chunk)
                if actual != crc:
                    raise ValueError('OBIG checksum mismatch: '+name)
            previous, end = name, offset+size
        return {raw.split(b'\0', 1)[0].decode('ascii'): (offset, size, crc) for raw, offset, size, crc in rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    print('Packed', pack(args.root, args.output), 'resources')
