#!/usr/bin/env python3
"""List/extract the classic Homeworld RBF1.23 BIG archive format."""
import argparse, fnmatch, pathlib, struct

ENTRY = struct.Struct("<IIH2xIIIIB3x")

def decrypt_name(data: bytes) -> str:
    decoded = bytearray(data)
    mask = 213
    for i, value in enumerate(decoded):
        decoded[i] = value ^ mask
        mask = decoded[i]
    return decoded.decode("latin-1").replace("\\", "/")

class BitReader:
    def __init__(self, data): self.data, self.bit = data, 0
    def read(self, count):
        value = 0
        for _ in range(count):
            if self.bit >= len(self.data) * 8: raise ValueError("truncated LZSS stream")
            value = (value << 1) | ((self.data[self.bit >> 3] >> (7 - (self.bit & 7))) & 1)
            self.bit += 1
        return value

def expand_lzss(data: bytes, expected: int) -> bytes:
    reader, window, current, output = BitReader(data), bytearray(4096), 1, bytearray()
    while True:
        if reader.read(1):
            value = reader.read(8)
            output.append(value); window[current] = value; current = (current + 1) & 4095
        else:
            position = reader.read(12)
            if position == 0: break
            length = reader.read(4) + 1
            for i in range(length + 1):
                value = window[(position + i) & 4095]
                output.append(value); window[current] = value; current = (current + 1) & 4095
        if len(output) > expected: raise ValueError("LZSS output exceeds archive length")
    if len(output) != expected: raise ValueError(f"LZSS size {len(output)} != {expected}")
    return bytes(output)

def entries(path):
    with path.open("rb") as stream:
        if stream.read(7) != b"RBF1.23": raise ValueError("not a classic RBF1.23 archive")
        count, flags = struct.unpack("<II", stream.read(8))
        records = [ENTRY.unpack(stream.read(ENTRY.size)) for _ in range(count)]
        for crc1, crc2, name_length, stored, real, offset, stamp, compression in records:
            stream.seek(offset)
            name = decrypt_name(stream.read(name_length))
            if stream.read(1) != b"\0": raise ValueError(f"bad name terminator: {name}")
            yield name, stream.read(stored), real, compression

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=pathlib.Path)
    parser.add_argument("patterns", nargs="*", default=["*"])
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    matched = 0
    for name, stored, real, compression in entries(args.archive):
        if not any(fnmatch.fnmatch(name.lower(), p.lower()) for p in args.patterns): continue
        matched += 1
        print(f"{name}\t{real}")
        if args.output:
            destination = args.output.joinpath(*pathlib.PurePosixPath(name).parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(expand_lzss(stored, real) if compression else stored)
    if not matched: raise SystemExit("no archive entries matched")

if __name__ == "__main__": main()
