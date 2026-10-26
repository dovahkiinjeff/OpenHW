import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from openhw_presentation_events import parse_events, compile_events,parse_source_events,SOURCE_EVENT_CORRECTIONS
import struct
import hashlib
from unittest.mock import patch
from import_kushan_weapon_fx import read_table

SOURCE = '''effects={animations={a={name="Weapon_Gun0",length=2,loop=0,parent="",minimum=0,maximum=0,markers={""}}},
events={flash={{"anim","Weapon_Gun0"},{"animtime","0"},{"marker","Muzzle,Smoke"},{"fx","flash"},{"fx_scale","entity"}},
sound={{"anim","weapon_gun0"},{"animtime","0"},{"sound","WEAPON/ION"}}}}'''

class PresentationEventsTests(unittest.TestCase):
    def test_import_correction_is_exact_hash_only_not_an_alternate_grammar(self):
        broken=b'effects={animations={},event1={{"anim","Fire"}}}}'
        correction=(b'event1=',b'events={event1=','explicit test correction')
        with self.assertRaises(ValueError):parse_source_events(broken)
        with patch.dict(SOURCE_EVENT_CORRECTIONS,{hashlib.sha256(broken).hexdigest():correction}):
            parsed=parse_source_events(broken)
            self.assertIn('explicit test correction',parsed['diagnostics'])
            self.assertEqual(len(parsed['events']),1)
            with self.assertRaises(ValueError):parse_source_events(broken+b' ')
            with self.assertRaises(ValueError):parse_events(broken.decode())
    def test_all_simultaneous_actions_and_markers_survive(self):
        result=parse_events(SOURCE)
        self.assertEqual(len(result['events']),2)
        self.assertEqual(result['events'][0]['markers'],['Muzzle','Smoke'])
        self.assertEqual(result['events'][0]['actions']['fx_scale'],'entity')
        self.assertEqual(result['events'][1]['actions']['sound'],'WEAPON/ION')
        self.assertEqual(result['diagnostics'],[])
    def test_does_not_execute_lua(self):
        with self.assertRaises(ValueError): parse_events(SOURCE+'; os.execute("bad")')
    def test_unknown_actions_not_silently_dropped(self):
        with self.assertRaisesRegex(ValueError,'Unsupported event actions'):
            parse_events(SOURCE.replace('"sound","WEAPON/ION"','"damage","100"'))
    def test_bad_scale_and_time(self):
        for before,after in [('"entity"','"nan"'),('"animtime","0"','"animtime","-2"')]:
            with self.assertRaises(ValueError): parse_events(SOURCE.replace(before,after))
    def test_authored_negative_one_sentinel_is_not_clamped(self):
        result=parse_events(SOURCE.replace('"animtime","0"','"animtime","-1"'))
        self.assertEqual(result['events'][0]['time'],-1)
    def test_missing_timeline_reported(self):
        result=parse_events(SOURCE.replace('"anim","weapon_gun0"','"anim","missing"'))
        self.assertEqual(len(result['diagnostics']),1)
    def test_literal_product_duration(self):
        self.assertEqual(parse_events(SOURCE.replace('length=2','length=3.5*1.5'))['timelines'][0]['length'],5.25)
        with self.assertRaises(ValueError):
            parse_events(SOURCE.replace('length=2','length=3.5*someFunction()'))
    def test_empty_file_reported(self):
        self.assertEqual(parse_events('')['events'],[])
        self.assertTrue(parse_events('')['diagnostics'])
    def test_malformed_sections_and_markers(self):
        for source in ('effects={animations=1}', 'effects={events="bad"}',
                       SOURCE.replace('markers={""}', 'markers=2')):
            with self.assertRaises(ValueError): parse_events(source)
    def test_duplicate_records_follow_lua_with_diagnostic(self):
        result = parse_events('effects={events={broken=1},events={}}')
        self.assertEqual(result['events'], [])
        self.assertIn('Duplicate source key', result['diagnostics'][0])
    def test_parent_cycle_rejected(self):
        with self.assertRaisesRegex(ValueError,'Cyclic'):
            parse_events(SOURCE.replace('parent=""', 'parent="weapon_gun0"'))
    def test_record_identifier_case_preserved(self):
        self.assertEqual(parse_events(SOURCE.replace('flash={','Flash={'))['events'][0]['id'],'Flash')
    def test_compiled_events_retain_simultaneous_records(self):
        blob,diagnostics=compile_events(parse_events(SOURCE))
        magic,timelines,events,strings=struct.unpack_from('<8sIII',blob)
        self.assertEqual((magic,timelines,events),(b'OWEVENT2',1,2))
        self.assertEqual(len(blob),20+24+2*56+strings)
        first=struct.unpack_from('<IffIIIII',blob,44)
        self.assertEqual(first[:4],(0,0,1,1))
        self.assertFalse(diagnostics)
    def test_collector_harvest_uses_classic_visual_but_keeps_rm_sound(self):
        source=SOURCE.replace('Weapon_Gun0','Resourcing').replace('weapon_gun0','resourcing')
        source=source.replace('"flash"','"harvesting_beam_kus_combo"')
        normal,_=compile_events(parse_events(source))
        classic,diagnostics=compile_events(parse_events(source),classic_harvest=True)
        self.assertIn(b'harvesting_beam_kus_combo',normal)
        self.assertNotIn(b'harvesting_beam_kus_combo',classic)
        self.assertIn(b'WEAPON/ION',classic)
        self.assertTrue(any('HW1 resource ETG replaces RM' in row for row in diagnostics))
    def test_attachment_options_preserve_presence_and_modes(self):
        source=SOURCE.replace('{"fx_scale","entity"}', '{"fx_scale","radius"},{"fx_orphan","off"},{"fx_length","distance"},{"fx_other","target"},{"fx_transform","Between"},{"fx_othermarker","Impact"}')
        blob,_=compile_events(parse_events(source))
        self.assertEqual(struct.unpack_from('<IffI',blob,44)[3],8)
        self.assertEqual(struct.unpack_from('<IfII',blob,76),(7,1,2,5))
    def test_invalid_attachment_modes_rejected(self):
        for field,value in [('fx_other','enemy'),('fx_transform','guess'),('fx_length','nan')]:
            source=SOURCE.replace('{"fx_scale","entity"}', '{"fx_scale","entity"},{"'+field+'","'+value+'"}')
            with self.assertRaises(ValueError):compile_events(parse_events(source))
    def test_audio_data_assignments_do_not_execute_scripts(self):
        self.assertEqual(read_table('version=4; volume=.44 envelope={{distance=0,volume=1}}',None)['volume'],.44)
        with self.assertRaises(ValueError):read_table('version=4 os.execute("bad")',None)

if __name__=='__main__': unittest.main()
