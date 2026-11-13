import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_launcher as launcher
import launcher_languages as languages


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.stopped=patch.object(launcher.updater,'stop_if_game_running');self.stopped.start()
    def tearDown(self): self.stopped.stop();self.temp.cleanup()
    def config(self,text):
        (self.root/'Homeworld.cfg').write_bytes(text)
        return launcher.Configuration(self.root)
    def test_preserves_unknowns_comments_names_and_aliases(self):
        cfg=self.config(b'; untouched\r\nPlayerName    Andr\xe9\r\nfutureOption    57\r\ndlaa    3\r\nantiAliasing    3\r\n')
        cfg.save({'antiAliasing':'11'})
        self.assertEqual((self.root/'Homeworld.cfg').read_bytes(),b'; untouched\r\nPlayerName    Andr\xe9\r\nfutureOption    57\r\ndlaa    11\r\nantiAliasing    11\r\n')
        self.assertEqual(len(list((self.root/'Development/Backups/Launcher').rglob('Homeworld.cfg'))),1)
    def test_refuses_concurrent_game_setting_changes(self):
        cfg=self.config(b'godRayQuality 1\n');(self.root/'Homeworld.cfg').write_text('godRayQuality 2\n')
        with self.assertRaisesRegex(Exception,'changed since'): cfg.save({'godRayQuality':'0'})
        self.assertEqual((self.root/'Homeworld.cfg').read_text(),'godRayQuality 2\n')
    def test_rejects_editor_paths_and_injection(self):
        cfg=self.config(b'PlayerName Pilot\n')
        for values in ({'HomeworldDataPath':'bad'},{'PlayerName':'Pilot\nraytracing 0'},{'screenWidth':'-2'}):
            with self.assertRaises((ValueError,launcher.updater.UpdateError)): cfg.save(values)
        self.assertEqual(cfg.path.read_bytes(),cfg.original)
    def test_resource_scaling_alias(self):
        cfg=self.config(b'DisableSPResourceScaling 1\nSPResourcePopulationPercent 100\n')
        cfg.save({'SPResourcePopulationPercent':'25'})
        self.assertEqual(cfg.values['DisableSPResourceScaling'],'0');self.assertEqual(cfg.values['SPResourcePopulationPercent'],'25')
    def test_mod_discovery_retains_enabled_priority_and_new_is_off(self):
        for name in ('Alpha','Beta','New'): (self.root/'mods'/name).mkdir(parents=True)
        (self.root/'mods/load-order.txt').write_text('Beta\n#disabled=Alpha\nBeta\n../escape\n')
        rows=launcher.read_mods(self.root)
        self.assertEqual(rows,[dict(name='Beta',enabled=True),dict(name='Alpha',enabled=False),dict(name='New',enabled=False)])
        rows[0],rows[1]=rows[1],rows[0];rows[0]['enabled']=True;launcher.save_mods(self.root,rows)
        self.assertEqual(launcher.read_mods(self.root),rows)
    def test_mod_changes_are_checked_against_actual_folders(self):
        (self.root/'mods/Alpha').mkdir(parents=True)
        for rows in ([dict(name='../outside',enabled=True)],[],[dict(name='Alpha',enabled=True),dict(name='Alpha',enabled=False)]):
            with self.assertRaises(Exception): launcher.save_mods(self.root,rows)
    def test_shader_reset_retains_data_and_moves_only_local_cache(self):
        cache=self.root/'Development/ShaderCache';cache.mkdir(parents=True);(cache/'test.cso').write_bytes(b'compiled')
        (self.root/'Homeworld.cfg').write_bytes(b'unchanged');(self.root/'Data').mkdir();(self.root/'Data/player.map').write_bytes(b'keep')
        text=launcher.rebuild_shader_cache(self.root)
        self.assertIn('next',text);self.assertFalse(cache.exists())
        self.assertEqual(next((self.root/'Development/Backups/ShaderCache').rglob('test.cso')).read_bytes(),b'compiled')
        self.assertEqual((self.root/'Data/player.map').read_bytes(),b'keep');self.assertEqual((self.root/'Homeworld.cfg').read_bytes(),b'unchanged')
    def test_game_running_blocks_all_writes(self):
        cfg=self.config(b'godRayQuality 1\n')
        with patch.object(launcher.updater,'stop_if_game_running',side_effect=RuntimeError('running')):
            for function in (lambda:cfg.save({'godRayQuality':'2'}),lambda:launcher.save_mods(self.root,[]),lambda:launcher.rebuild_shader_cache(self.root)):
                with self.assertRaisesRegex(RuntimeError,'running'): function()
        self.assertEqual(cfg.path.read_bytes(),cfg.original)
    def test_voice_languages_require_matching_bank_and_tables(self):
        folder=self.root/'Data/Localization/French'
        for i,name in enumerate(languages.FILES):
            path=folder/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(struct.pack('<II',1,123)+b'\0'*16)
        self.assertIn('1',languages.available(self.root))
        (folder/languages.FILES[1]).write_bytes(struct.pack('<II',1,321)+b'\0'*16)
        self.assertNotIn('1',languages.available(self.root))
        cfg=self.config(b'voiceLanguage 0\n')
        with self.assertRaisesRegex(ValueError,'complete matching'): cfg.save({'voiceLanguage':'1'})
    def test_catalogue_covers_game_options_without_authoring_files(self):
        keys={row['key'] for row in launcher.SETTINGS}
        self.assertTrue({'antiAliasing','frameGeneration','godRayQuality','keyboardLayout','voiceLanguage','StartingResourceFleet','vSync','frameRateLimit','FORMATION_CLAW','PlayerBaseColor'}<=keys)
        self.assertNotIn('HomeworldDataPath',keys)
        self.assertTrue(all('rtxmap' not in key.lower() for key in keys))


if __name__=='__main__': unittest.main()
