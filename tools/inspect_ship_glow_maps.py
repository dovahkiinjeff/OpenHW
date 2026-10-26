"""Read-only asset inspection; emits a comparison sheet, never alters textures."""
from pathlib import Path
import struct
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
cache = root / 'out/hwrm-kushan-native/material-cache/v2/ship'
sheet = Image.new('RGB', (1536, 1280), '#151515')
draw = ImageDraw.Draw(sheet)
for row, (ship, index) in enumerate([('kus_resourcecontroller', 0), ('kus_mothership', 0),
                                    ('kus_mothership', 3), ('kus_mothership', 1)]):
    for column, suffix in enumerate(['', '_emissive', '_normal']):
        path = cache / ship / ship / f'm{index}{suffix}.dds'
        if not path.exists():
            continue
        data = path.read_bytes()
        height, width = struct.unpack_from('<II', data, 12)
        im = Image.frombytes('RGBA', (width, height), data[128:]).convert('RGB')
        im.thumbnail((500, 282))
        sheet.paste(im, (column*512, row*320+26))
        draw.text((column*512+4, row*320+4), f'{ship} m{index} {suffix or "diffuse"}', fill='white')
output = root / 'out/diagnostics/hod-glow-maps.png'
output.parent.mkdir(parents=True, exist_ok=True)
sheet.save(output)
print(output)
