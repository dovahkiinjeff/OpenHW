import sys
import unittest
from pathlib import Path, PureWindowsPath

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from subdivide_engine_glows import subdivide, HEADER, OBJECT, VERTEX, POLYGON, NAMES
from extract_vanilla_ship_textures import read_archive, entry_bytes

class EngineGlowSubdivision(unittest.TestCase):
    def test_owned_meshes_preserve_bounds_uvs_materials_and_transforms(self):
        archive=Path('F:/SteamLibrary/steamapps/common/Homeworld/Homeworld1Classic/Data/homeworld.big')
        if not archive.exists():self.skipTest('Owned Classic archive unavailable')
        blob,entries=read_archive(archive);count=0
        for entry in entries:
            path=PureWindowsPath(entry.name.lower())
            if str(path.parent)!='misc\\engineglows' or path.suffix!='.geo' or path.stem not in NAMES:continue
            count+=1;source=entry_bytes(blob,entry);output,stats=subdivide(source)
            self.assertEqual(stats['triangles_after'],stats['triangles_before']*4)
            for index in range(HEADER.unpack_from(source)[9]):
                old=OBJECT.unpack_from(source,68+index*112);new=OBJECT.unpack_from(output,68+index*112)
                self.assertEqual(old[:4],new[:4]);self.assertEqual(old[11:],new[11:])
                before=[VERTEX.unpack_from(source,old[8]+i*16) for i in range(old[4])]
                after=[VERTEX.unpack_from(output,new[8]+i*16) for i in range(new[4])]
                self.assertEqual(before,after[:len(before)])
                for axis in range(3):
                    self.assertAlmostEqual(min(v[axis] for v in before),min(v[axis] for v in after))
                    self.assertAlmostEqual(max(v[axis] for v in before),max(v[axis] for v in after))
                for i in range(old[7]):
                    p=POLYGON.unpack_from(source,old[10]+i*40)
                    children=[POLYGON.unpack_from(output,new[10]+(i*4+j)*40) for j in range(4)]
                    self.assertTrue(all(c[0]==p[0] and c[4]==p[4] and c[11:]==p[11:] for c in children))
                    for axis in range(2):
                        self.assertAlmostEqual(children[0][7+axis],(p[5+axis]+p[7+axis])/2,places=5)
                        self.assertAlmostEqual(children[0][9+axis],(p[5+axis]+p[9+axis])/2,places=5)
                    for axis in range(3):
                        expected=(before[p[1]][axis]+before[p[2]][axis])/2
                        self.assertAlmostEqual(after[children[0][2]][axis],expected,delta=max(1e-6,abs(expected)*1e-7))
        self.assertEqual(count,20)

    def test_energybolt_is_not_selected(self):
        self.assertEqual(len(NAMES),20)
        self.assertNotIn('energybolt',NAMES)

    def test_curved_subdivision_rounds_surface_without_moving_nozzle(self):
        source=ROOT/'out/engine-glow-subdivision/runtime/misc/engineglows/engine_glow00.geo'
        if not source.exists():self.skipTest('Generated owned mesh unavailable')
        data=source.read_bytes();linear,_=subdivide(data);curved,_=subdivide(data,smooth=True)
        old=OBJECT.unpack_from(data,68);a=OBJECT.unpack_from(linear,68);b=OBJECT.unpack_from(curved,68)
        before=[VERTEX.unpack_from(data,old[8]+i*16) for i in range(old[4])]
        plain=[VERTEX.unpack_from(linear,a[8]+i*16) for i in range(a[4])]
        smooth=[VERTEX.unpack_from(curved,b[8]+i*16) for i in range(b[4])]
        self.assertEqual(before,smooth[:len(before)])
        nozzle=max(v[2] for v in before)
        self.assertAlmostEqual(max(v[2] for v in smooth),nozzle)
        self.assertTrue(any(sum((p[j]-s[j])**2 for j in range(3))>1e-12 for p,s in zip(plain,smooth)))
        # Curvature changes geometry only, not topology, UVs or transforms.
        self.assertEqual(a[4:8],b[4:8]);self.assertEqual(a[11:],b[11:])
        for i in range(a[7]):
            self.assertEqual(POLYGON.unpack_from(linear,a[10]+i*40),POLYGON.unpack_from(curved,b[10]+i*40))

if __name__=='__main__':unittest.main()
