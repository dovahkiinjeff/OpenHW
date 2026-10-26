import re
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from import_remastered_audio import TRACKS, inventory, stage, stage_effects

class RemasteredMusicTests(unittest.TestCase):
    def test_importer_matches_runtime_track_indices(self):
        header=(Path(__file__).resolve().parents[1]/'src/Game/RemasteredMusicPaths.h').read_text()
        body=header.split('paths[] = {',1)[1].split('};',1)[0]
        tokens=re.findall(r'"([^"]+)"|\b(0)\b',body)
        self.assertEqual([name or None for name,zero in tokens],TRACKS)
        self.assertEqual(len(TRACKS),48)
        self.assertEqual(sum(t is not None for t in TRACKS),45)
    def test_missing_source_does_not_create_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with self.assertRaises(ValueError):stage(root/'source',root/'output')
            self.assertFalse((root/'output').exists())
    def test_overlapping_source_destination_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):stage(tmp,Path(tmp)/'output')
            with self.assertRaises(ValueError):stage(Path(tmp)/'source',tmp)
    def test_effect_audio_preserves_source_samples_and_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source';source.mkdir()
            (source/'sample.fda').write_bytes(b'FORMoriginal')
            (source/'sample.lua').write_text('version=4\nvolume=.77\nrandSampContainer=0')
            self.assertEqual(stage_effects(source,root/'output'),2)
            target=root/'output/RemasteredAudio/sfx/etg'
            self.assertEqual((target/'sample.fda').read_bytes(),(source/'sample.fda').read_bytes())
            self.assertEqual((target/'sample.lua').read_bytes(),(source/'sample.lua').read_bytes())
    def test_effect_audio_validates_before_copying(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source';source.mkdir()
            (source/'a.fda').write_bytes(b'FORMoriginal');(source/'z.fda').write_bytes(b'bad')
            with self.assertRaises(ValueError):stage_effects(source,root/'output')
            self.assertFalse((root/'output').exists())

if __name__=='__main__':unittest.main()
