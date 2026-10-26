"""Explicit Taiidan scope and original gun/animation contracts."""
import json
import re
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from stage_taiidan_native import R2_HULLS, RETAIN_CLASSIC, ENGINES, tai_name

class TaiidanNative(unittest.TestCase):
    def test_exact_scope(self):
        self.assertEqual(len(R2_HULLS),29)
        scope=(ROOT/'src/Modern/NativeVisualScope.h').read_text().split('static int hwRmNativeR2Type')[1].split('static int hwRmNativeP1Type')[0]
        self.assertEqual(set(re.findall(r'"([A-Za-z0-9_]+)"',scope)),set(R2_HULLS))
        self.assertFalse(set(RETAIN_CLASSIC)&set(R2_HULLS))
        self.assertTrue(set(ENGINES)<=set(R2_HULLS))

    def test_dependency_names(self):
        self.assertEqual(tai_name('Tai_CorvetteGun'),'tai_corvettegun')
        self.assertEqual(tai_name('HW1_ResearchModule'),'hw1_researchmodule')
        for name in ('kus_corvettegun','tur_fighter','../tai_gun','tai_gun/evil','hw1_researchmodule1'):
            with self.assertRaises(ValueError):tai_name(name)

    def test_local_source_audit_matches_every_gameplay_gun(self):
        path=ROOT/'out/hwrm-taiidan-native/binding-audit.json'
        if not path.is_file():self.skipTest('Owned local source audit not available')
        source=(ROOT/'src/Game/NativeVisualEvents.c').read_text()
        mapped={name:[] for name in R2_HULLS}
        for name,index in re.findall(r'(?:GUN\(|\{)"Taiidan_([A-Za-z0-9_]+)",\s*(\d+)',source):
            mapped[name].append(int(index))
        audit=json.loads(path.read_text())
        self.assertEqual(len(audit['ships']),29)
        for ship in audit['ships']:
            self.assertEqual(sorted(mapped[ship['type']]),sorted(ship['gameplay_guns']),ship['type'])
        self.assertEqual(sum(map(len,mapped.values())),62)

    def test_race_qualified_activation(self):
        loader=(ROOT/'src/Game/Mesh.c').read_text()
        self.assertIn('strncasecmp(original,"R2\\\\",3)',loader)
        self.assertIn('!hwRmNativeR2Type(type)',loader)
        self.assertIn('"Taiidan_"',loader)
        package=(ROOT/'src/Modern/RemasteredVisualPackage.cpp').read_text()
        self.assertIn('hwRmNativeR2Type(type+8)',package)
        self.assertIn('HW_RM_R2_VISUAL_1',package)

    def test_only_wing_clips_selected_for_converted_heavy_rig(self):
        path=ROOT/'out/hwrm-taiidan-native/package-cache/v1/R2/HeavyCorvette/RMNative/visual.rmnative'
        if not path.is_file():self.skipTest('Owned local package not available')
        lines=path.read_text().splitlines()
        self.assertEqual({l.split()[1] for l in lines if l.startswith('animation_clip ')},
            {'Wings_Open','Wings_Close','Wings_Open_DMG','Wings_Close_DMG'})
        self.assertFalse(any('animation_alias' in l for l in lines))

if __name__=='__main__':unittest.main()
