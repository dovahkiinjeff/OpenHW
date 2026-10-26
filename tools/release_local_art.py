"""Generate presentation resources from the selected, read-only retail source."""
from pathlib import Path
from PIL import Image
from extract_vanilla_ship_textures import decode_lif


def restore_cursors(generated):
    generated = Path(generated)
    count = 0
    for source in sorted((generated/'cursorbitmaps').glob('*.lif')):
        image, _, _, _ = decode_lif(source.read_bytes())
        destination = generated/'cursors'/source.with_suffix('.png').name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_file():
            image.resize((image.width*8, image.height*8), Image.Resampling.LANCZOS).save(destination)
        count += 1
    if count < 20:
        raise ValueError('The selected Classic archive does not contain the required cursors')
    print('Restored high-resolution cursors from selected Classic archive:', count, flush=True)
