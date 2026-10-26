"""Read-only comparison of packed wreck DDS images and original LIFs."""
import io
import struct
import sys
from pathlib import Path
from PIL import Image, ImageDraw
import extract_vanilla_ship_textures as classic

runtime = Path(r'F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release')
blob, entries = classic.read_archive(runtime.parents[2] / 'Data/homeworld.big')
originals = {e.name.lower(): e for e in entries}
tiles = []
model = sys.argv[1] if len(sys.argv) > 1 else 'shipwreck'
with (runtime / 'HomeworldTextures.hwt').open('rb') as stream:
    _, _, count, _ = struct.unpack('<8sIIQ', stream.read(24))
    records = [struct.unpack('<256sQII', stream.read(272)) for _ in range(count)]
    for raw, offset, size, _ in records:
        name = raw.split(b'\0')[0].decode()
        if not name.startswith('derelicts\\' + model + '\\rl0\\lod0\\') or name.endswith(('_normal.dds', '_teameffect0.dds', '_teameffect1.dds')):
            continue
        stream.seek(offset)
        image = Image.open(io.BytesIO(stream.read(size))).convert('RGB')
        original = originals.get(name[:-4] + '.lif')
        tile = Image.new('RGB', (400, 220), '#202020')
        draw = ImageDraw.Draw(tile)
        draw.text((4, 4), name.rsplit('\\', 1)[-1], fill='white')
        image.thumbnail((190, 190))
        tile.paste(image, (0, 25))
        if original:
            lif, _, _, _ = classic.decode_lif(classic.entry_bytes(blob, original))
            lif = lif.convert('RGB'); lif.thumbnail((190, 190))
            tile.paste(lif, (200, 25))
        tiles.append(tile)
sheet = Image.new('RGB', (1200, ((len(tiles) + 2)//3)*220))
for i, tile in enumerate(tiles):
    sheet.paste(tile, ((i%3)*400, (i//3)*220))
output = Path('out/' + model + '-texture-comparison.png')
sheet.save(output)
print(output.resolve())
