"""Prepare existing owner-supplied concept art for the portable launcher."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageEnhance,ImageOps,ImageDraw

ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'assets/Launcher';folder.mkdir(parents=True,exist_ok=True)
    source=Image.open(ROOT/'assets/UI/concept_hangar.png').convert('RGB')
    for width in (1000,1440,2200):
        size=(width,round(width*0.625))
        art=ImageOps.fit(source,size,method=Image.Resampling.LANCZOS,centering=(0.58,0.48))
        art=ImageEnhance.Color(art).enhance(0.80)
        # This is layout grading/cropping of the supplied art, not generated detail.
        mask=Image.new('L',(width,1));mask.putdata([
            round(225*max(0,min(1,(x/width-0.24)/0.53))**1.4) for x in range(width)])
        mask=mask.resize(size)
        image=Image.composite(art,Image.new('RGB',size,'#0b1820'),mask)
        image.save(folder/f'hangar-{width}.png',optimize=True)
    mark=Image.new('RGBA',(256,256),'#081218');draw=ImageDraw.Draw(mark)
    draw.rectangle((4,4,251,251),outline='#29444e',width=5)
    draw.polygon(((60,51),(146,128),(60,205),(93,128)),fill='#e6ae45')
    draw.line(((137,62),(207,128),(137,194)),fill='#73d4d2',width=9,joint='curve')
    mark.save(folder/'mark.png',optimize=True)
    mark.save(folder/'launcher.ico',format='ICO',sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])
    manifest=dict(art='assets/UI/concept_hangar.png',treatment='aspect-preserving crop, scale and interface grade; no generated detail',
        fonts=['https://github.com/google/fonts/tree/main/ofl/rajdhani','https://github.com/google/fonts/tree/main/ofl/sharetechmono'])
    manifest['sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.suffix in ('.ttf','.png','.txt','.ico')}
    (folder/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared launcher concept art and font provenance')
if __name__=='__main__':main()
