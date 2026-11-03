"""Upscale locally owned cutscenes without changing their original motion.

The source movies remain untouched. An output is installed only after a full
decode confirms its frame count; the game can use the original as a fallback.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--movies', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    parser.add_argument('--ffmpeg', required=True, type=Path)
    args = parser.parse_args()
    movies = args.movies.resolve()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    output_dir = movies / 'HD'
    output_dir.mkdir(exist_ok=True)
    manifest_path = work / 'original-motion-manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {}
    paths = sorted(movies.glob('[0-9][0-9]_[0-9][0-9].bik'))
    if not paths:
        raise RuntimeError('No story cutscenes found')
    for index, source in enumerate(paths, 1):
        start = time.monotonic()
        with source.open('rb') as f:
            sig, _, frames, _, _, width, height, rate, rate_div = struct.unpack('<4s8I', f.read(36))
        if not sig.startswith(b'BIK') or (width, height, rate, rate_div) != (640, 480, 15, 1):
            raise ValueError('Unexpected movie format: ' + source.name)
        source_hash = digest(source)
        target = output_dir / (source.stem + '.mp4')
        entry = manifest.get(source.name, {})
        if (target.is_file() and entry.get('source_sha256') == source_hash and
                entry.get('output_sha256') == digest(target)):
            print(f'[{index}/{len(paths)}] {source.name}: already verified', flush=True)
            continue
        partial = output_dir / (source.stem + '.partial.mp4')
        if partial.exists():
            raise RuntimeError('Previous partial file needs inspection: ' + str(partial))
        print(f'[{index}/{len(paths)}] {source.name}: upscaling {frames} original frames', flush=True)
        cmd = [str(args.ffmpeg), '-hide_banner', '-v', 'error', '-i', str(source),
               '-map', '0:v:0', '-vf', 'crop=608:342:16:69:exact=1,scale=1920:1080:flags=lanczos',
               '-frames:v', str(frames), '-fps_mode', 'passthrough', '-an', '-c:v', 'libx264',
               '-preset', 'medium', '-crf', '16', '-pix_fmt', 'yuv420p', '-bf', '0',
               '-movflags', '+faststart', str(partial)]
        subprocess.run(cmd, check=True)
        result = subprocess.run([str(args.ffmpeg), '-hide_banner', '-v', 'error', '-i', str(partial),
                                 '-map', '0:v:0', '-f', 'null', '-', '-progress', 'pipe:1'],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        counts = re.findall(r'^frame=\s*(\d+)', result.stdout, re.MULTILINE)
        if not counts or int(counts[-1]) != frames or digest(source) != source_hash:
            raise RuntimeError('Source hash or decoded output frame count mismatch: ' + source.name)
        os.replace(partial, target)
        manifest[source.name] = {'frames': frames, 'fps': 15, 'resolution': [1920, 1080],
                                 'crop': [16, 69, 608, 342], 'filter': 'lanczos',
                                 'source_sha256': source_hash, 'output_sha256': digest(target),
                                 'output_bytes': target.stat().st_size}
        tmp = manifest_path.with_suffix('.tmp')
        tmp.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        os.replace(tmp, manifest_path)
        print(f'[{index}/{len(paths)}] {source.name}: verified in {time.monotonic()-start:.1f}s', flush=True)
    print('All movies complete and verified.', flush=True)


if __name__ == '__main__':
    main()
