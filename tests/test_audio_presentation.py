import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from openhw_audio_presentation import parse_parameters,parameter_inventory,compile_sound_bank
import struct

class AudioPresentationTests(unittest.TestCase):
    def test_absent_fields_are_not_defaulted(self):
        self.assertEqual(parse_parameters('version=4 randSampContainer=1'),
                         dict(version=4,randSampContainer=1))
    def test_envelope_and_effects_preserved(self):
        point='{distance=0,volume=.7,reverb=.1,duration=0,equalizer={1,1,1,1,1,1,1,.8}}'
        data=parse_parameters('version=4 envelope={'+point+'} effects={pitchShift=.9,equalizer={1,1,1,1,1,1,1,1}}')
        self.assertEqual(data['envelope'][0]['volume'],.7)
        self.assertEqual(data['envelope'][0]['equalizer'][-1],.8)
        self.assertEqual(data['effects']['pitchShift'],.9)
    def test_executable_and_invalid_parameters_rejected(self):
        for text in ('version=4 volume=os.execute("bad")','version=4 volume=1*2',
                     'version=3','version=4 volume=true','version=4 envelope={}',
                     'version=4 envelope={{distance=0}}','version=4 volume=1e999',
                     'version=4 version=4'):
            with self.subTest(text=text),self.assertRaises(ValueError):parse_parameters(text)
    def test_directory_and_sample_records_remain_distinct(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'etg';root.mkdir()
            (root/'etg.lua').write_text('version=4 volume=.5')
            (root/'hit.fda').write_bytes(b'FORM')
            (root/'hit.lua').write_text('version=4 maxPolyphony=3')
            group=root/'group';group.mkdir()
            (group/'group.lua').write_text('version=4 randSampContainer=1')
            (group/'a.fda').write_bytes(b'FORM');(group/'b.fda').write_bytes(b'FORM')
            nodes={n['identity']:n for n in parameter_inventory(root)}
            self.assertEqual(nodes['']['samples'],['hit.fda'])
            self.assertEqual(nodes['group']['samples'],['group/a.fda','group/b.fda'])
            self.assertEqual(nodes['hit']['scope'],'sample')
            self.assertNotIn('volume',nodes['hit']['authored'])
    def test_missing_sample_is_not_silently_dropped(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp)/'missing.lua').write_text('version=4')
            with self.assertRaises(ValueError):parameter_inventory(tmp)
    def test_runtime_bank_preserves_parent_profiles_and_sample_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'etg';root.mkdir()
            (root/'etg.lua').write_text('version=4 volume=.5 maxPolyphony=3')
            group=root/'group';group.mkdir()
            (group/'group.lua').write_text('version=4 randSampContainer=1')
            (group/'one.fda').write_bytes(b'FORM');(group/'two.fda').write_bytes(b'FORM')
            data=compile_sound_bank(root)
            magic,nodes,edges,profiles,envelopes,strings=struct.unpack_from('<8s5I',data)
            self.assertEqual((magic,nodes,edges,profiles,envelopes),(b'OWSFX001',4,3,2,0))
            self.assertEqual(len(data),28+nodes*28+edges*4+profiles*252+envelopes*48+strings)
            self.assertIn(b'remasteredaudio/sfx/etg/group/one.fda\0',data)
            self.assertIn(b'remasteredaudio/sfx/etg/group/two.fda\0',data)
    def test_unknown_bank_parameters_are_rejected_not_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'etg';root.mkdir();(root/'a.fda').write_bytes(b'FORM')
            (root/'a.lua').write_text('version=4 undocumented=1')
            with self.assertRaisesRegex(ValueError,'Unsupported sound bank parameter'):compile_sound_bank(root)

if __name__=='__main__':unittest.main()
