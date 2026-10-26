#!/usr/bin/env python3
"""Inspect Homeworld MAD animation metadata from a BIG archive."""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

from extract_vanilla_ship_textures import entry_bytes, read_archive


def cstring(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise ValueError(f"string offset {offset} is outside a {len(data)} byte block")
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"unterminated string at offset {offset}")
    return data[offset:end].decode("latin-1")


def inspect_mad(data: bytes) -> dict:
    if len(data) < 40:
        raise ValueError("MAD file is shorter than its header")
    identifier, version, string_length, string_offset, length, fps, object_count, object_offset, animation_count = struct.unpack_from(
        "<8sfIIffIII", data
    )
    if identifier.rstrip(b"\0") != b"Mellonn":
        raise ValueError(f"unexpected MAD identifier {identifier!r}")
    if string_offset + string_length > len(data):
        raise ValueError("MAD string block extends past end of file")
    strings = data[string_offset:string_offset + string_length]

    animations = []
    animation_offset = 40
    for index in range(animation_count):
        name_offset, start, end, flags = struct.unpack_from("<IffI", data, animation_offset + index * 16)
        animations.append({
            "index": index,
            "name": cstring(strings, name_offset),
            "start": start,
            "end": end,
            "duration": end - start,
            "flags": flags,
        })

    objects = []
    for index in range(object_count):
        base = object_offset + index * 48
        if base + 48 > len(data):
            raise ValueError("MAD object table extends past end of file")
        name_offset, name_crc, _pad, animation_bits, keyframes = struct.unpack_from("<IH2sII", data, base)
        objects.append({
            "index": index,
            "name": cstring(strings, name_offset),
            "name_crc": name_crc,
            "animation_bits": f"0x{animation_bits:08x}",
            "keyframes": keyframes,
        })

    return {
        "identifier": identifier.rstrip(b"\0").decode("ascii"),
        "version": version,
        "length": length,
        "frames_per_second": fps,
        "animations": animations,
        "objects": objects,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--member", required=True)
    args = parser.parse_args()

    blob, entries = read_archive(args.archive)
    matches = [entry for entry in entries if entry.name.lower() == args.member.lower()]
    if len(matches) != 1:
        print(f"expected one archive member, found {len(matches)}", file=sys.stderr)
        return 1
    result = inspect_mad(entry_bytes(blob, matches[0]))
    result["member"] = matches[0].name
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
