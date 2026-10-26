#!/usr/bin/env python3
"""Extract, upscale (256 minimum / 2048 maximum), and BC7-pack HW1 FX."""
from __future__ import annotations
import argparse, hashlib, json, subprocess, sys
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).parent))
from extract_vanilla_ship_textures import (decode_lif, entry_bytes, read_archive,
                                           safe_relative)
from ship_texture_pipeline import run_realesrgan

def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""): h.update(block)
    return h.hexdigest()

def fx_dimensions(width: int, height: int, minimum: int = 256,
                  maximum: int = 2048) -> tuple[int, int]:
    if min(width, height, minimum) <= 0 or maximum < minimum:
        raise ValueError('Invalid FX dimensions')
    scale = min(max(4.0, minimum / min(width, height)), maximum / max(width, height))
    size = (round(width * scale), round(height * scale))
    if min(size) < minimum:
        raise ValueError('FX aspect ratio cannot satisfy both bounds without distortion')
    return size

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--realesrgan", type=Path)
    p.add_argument("--backend", choices=('realesrgan', 'lanczos'), default='realesrgan')
    p.add_argument("--converter", type=Path, required=True)
    p.add_argument("--model", default="realesrgan-x4plus")
    p.add_argument("--tile", type=int, default=256)
    p.add_argument("--min-dimension", type=int, default=256)
    p.add_argument("--max-dimension", type=int, default=2048)
    args = p.parse_args()
    if args.backend == 'realesrgan' and args.realesrgan is None:
        p.error('--realesrgan is required for the realesrgan backend')
    decoded, upscaled, dds = (args.work / name for name in ("decoded", "upscaled", "dds"))
    blob, entries = read_archive(args.archive)
    selected = [e for e in entries if e.name.lower().startswith("etg\\") and
                e.name.lower().endswith(".lif")]
    records = []
    excluded = []
    for index, entry in enumerate(selected, 1):
        relative = safe_relative(entry.name).with_suffix(".png")
        source = decoded / relative
        target = upscaled / relative
        output = (dds / relative).with_suffix(".dds")
        source.parent.mkdir(parents=True, exist_ok=True)
        target.parent.mkdir(parents=True, exist_ok=True)
        output.parent.mkdir(parents=True, exist_ok=True)
        try:
            image, _team0, _team1, meta = decode_lif(entry_bytes(blob, entry))
        except Exception as error:
            excluded.append({"source": entry.name, "reason": str(error)})
            continue
        size = fx_dimensions(image.width, image.height, args.min_dimension, args.max_dimension)
        if not source.is_file(): image.save(source, optimize=True)
        changed = not target.is_file()
        if not changed:
            with Image.open(target) as existing: changed = existing.size != size
        if changed:
            if args.backend == 'lanczos':
                # Keep the original authored RGB/alpha and animation framing.
                image.resize(size, Image.Resampling.LANCZOS).save(target, optimize=True)
            else:
                run_realesrgan(args.realesrgan, source, target, size, args.model, args.tile)
        if changed or not output.is_file():
            result = subprocess.run((str(args.converter), "coloronly", str(target), str(output)))
            if result.returncode: raise RuntimeError(f"BC7 conversion failed: {relative}")
        records.append({"source": entry.name, "source_size": [image.width, image.height],
                        "output_size": list(size), "dds": output.relative_to(dds).as_posix(),
                        "dds_bytes": output.stat().st_size, "sha256": digest(output),
                        "backend": args.backend,
                        "normal_generated": False})
        if index % 25 == 0 or index == len(selected):
            print(f"weapon FX {index}/{len(selected)}", flush=True)
    manifest = args.work / "weapon-fx-manifest.json"
    manifest.write_text(json.dumps(records, indent=2), encoding="utf-8")
    (args.work / "weapon-fx-exclusions.json").write_text(
        json.dumps(excluded, indent=2), encoding="utf-8")
    print(json.dumps({"textures": len(records), "dds_bytes": sum(r["dds_bytes"] for r in records),
                      "normal_maps": 0, "excluded_nontextures": len(excluded),
                      "manifest": str(manifest)}, indent=2))
    return 0

if __name__ == "__main__": raise SystemExit(main())
