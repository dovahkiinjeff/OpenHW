"""Check real 64-bit multiplayer chat HUD creation/dispatch/reuse, headlessly."""
import argparse
import os
from pathlib import Path
import subprocess
from protocol import default_config, LOAD_PREFIX, pack_config, envelope, WORKER_INIT


def run(exe, root, logs):
    logs.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env['PATH'] = str(root) + os.pathsep + env['PATH']
    config = default_config('GardenSalad%d')
    for i in range(2):
        config['slots'][i] = dict(kind=1 if i == 0 else 2, race=i,
            difficulty=1, ready=1, team=i + 1, base=0xff303030,
            stripe=0xff2020ff, emission=0xff2020ff, name=f'Player {i+1}')
    load = LOAD_PREFIX.pack(1, 0, 1, 2, 0) + pack_config(config)
    result = subprocess.run([str(exe), '--dedicated-replica', '--check-chat-overlay'],
        cwd=root, env=env, input=envelope(WORKER_INIT, load),
        capture_output=True, timeout=60)
    (logs/'chat-overlay.log').write_bytes(result.stderr)
    assert result.returncode == 0, (result.returncode, result.stderr[-3000:])
    assert b'64-bit multiplayer chat overlay draw and reuse passed' in result.stderr
    print('PASS: multiplayer chat keeps its high 64-bit atom address through draw and reuse', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('exe', type=Path)
    parser.add_argument('root', type=Path)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    run(args.exe.resolve(), args.root.resolve(), args.logs.resolve())
