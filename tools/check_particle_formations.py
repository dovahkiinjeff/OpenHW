"""Offline sampling of original mission placements; never starts the game.

Uses the loader's cylinder/sphere placement geometry and distribution weights.
Seeds vary the sample; these are not a dump of a live game's random state.
"""
import argparse
import math
import pathlib
import random
import subprocess
import tempfile


def particles(directory, seed):
    rng = random.Random(seed)
    for sphere in sorted(directory.glob('*.missphere')):
        for line in sphere.read_text().splitlines():
            if not line.startswith('Resources'):
                continue
            fields = line.split(None, 1)[1].split(',')
            if fields[0] != 'Asteroid':
                continue
            center = list(map(float, fields[1:4]))
            shape, distribution = fields[4:6]
            weights = {}
            path = directory / distribution
            for record in path.read_text().splitlines():
                parts = record.split()
                if len(parts) == 2 and parts[0].startswith('Asteroid'):
                    weights[parts[0]] = int(parts[1])
            number = int(fields[6])
            radius, length, ry, rz = map(float, fields[7:11])
            ry, rz = math.radians(ry), math.radians(rz)
            for _ in range(number):
                if shape == 'Cylinder':
                    x, y, z = rng.uniform(-length/2, length/2), rng.uniform(-radius, radius), rng.uniform(-radius, radius)
                    x, z = math.cos(ry)*x+math.sin(ry)*z, -math.sin(ry)*x+math.cos(ry)*z
                    x, y = math.cos(rz)*x-math.sin(rz)*y, math.sin(rz)*x+math.cos(rz)*y
                elif shape == 'Sphere':
                    while True:
                        x, y, z = [rng.uniform(-radius, radius) for _ in range(3)]
                        if x*x+y*y+z*z <= radius*radius:
                            break
                else:
                    raise ValueError(f'Unsupported placement: {shape}')
                asteroid = rng.choices(list(weights), list(weights.values()))[0]
                if asteroid == 'Asteroid0':
                    yield [center[0]+x, center[1]+y, center[2]+z]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mission', type=pathlib.Path)
    parser.add_argument('probe', type=pathlib.Path)
    parser.add_argument('--expected', type=int)
    parser.add_argument('--seeds', type=int, default=20)
    args = parser.parse_args()
    failures = 0
    with tempfile.TemporaryDirectory(prefix='particle-formations-') as temp:
        path = pathlib.Path(temp)/'positions.txt'
        for seed in range(args.seeds):
            path.write_text(''.join(' '.join(map(str, p))+'\n' for p in particles(args.mission, seed)))
            command = [str(args.probe.resolve()), str(path)]
            if args.expected is not None:
                command.append(str(args.expected))
            result = subprocess.run(command, capture_output=True, text=True)
            print(f'Seed {seed}: {result.stdout.strip()}')
            if result.stderr:
                print(result.stderr.strip())
            failures += result.returncode != 0
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
