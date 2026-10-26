import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from openhw_obig import pack,validate,HEADER,ENTRY
from openhw_update import apply,create,rollback,allowed
from portable_setup import collection_paths
from portable_convert import install_streaming_audio
from openhw_ship_definitions import reviewed_bindings


class PortableInstallTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'tmp').mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=ROOT/'tmp',prefix='portable-test-')
        self.root=Path(self.temp.name)
    def tearDown(self):self.temp.cleanup()

    def test_streaming_audio_is_installed_at_runtime_override_root(self):
        sources=self.root/'SetupSources';classic=sources/'Classic'
        classic.mkdir(parents=True)
        for name in ('HW_Comp.vce','HW_Music.wxd'):
            (classic/name).write_bytes(name.encode())
        install_streaming_audio(sources,self.root)
        for name in ('HW_Comp.vce','HW_Music.wxd'):
            self.assertEqual((self.root/name).read_bytes(),name.encode())
            self.assertFalse((self.root/'Data'/name).exists())
        (classic/'HW_Music.wxd').unlink()
        with self.assertRaisesRegex(ValueError,'Required Classic audio'):
            install_streaming_audio(sources,self.root)

    def test_shared_subsystem_update_paths_are_game_assets_not_user_projects(self):
        self.assertTrue(allowed('Data/R1/StandardFrigate/RMNative/ship.owshipdef'))
        self.assertFalse(allowed('Data/R1/StandardFrigate/gameplay.shp'))
        self.assertFalse(allowed('Data/Mods/Mine/RMNative/ship.owshipdef'))
        self.assertTrue(allowed('Subsystems/kus_assaultgun/model.hod'))
        self.assertTrue(allowed('Subsystems/kus_assaultgun/subsystem.owsubsystem'))
        self.assertFalse(allowed('Modding/Ships/MyFrigate/ship.owship'))
        self.assertFalse(allowed('mods/MyFrigate/Subsystems/kus_assaultgun/model.hod'))

    def test_obig_payload_checksum_and_path_rejection(self):
        data=self.root/'source';(data/'R1').mkdir(parents=True)
        (data/'R1'/'sample.shp').write_bytes(b'NumberGuns 2\r\n')
        (data/'empty').write_bytes(b'')
        archive=self.root/'test.obig';self.assertEqual(pack(data,archive),2)
        rows=validate(archive,True);self.assertIn('r1/sample.shp',rows)
        with archive.open('r+b') as stream:
            stream.seek(rows['r1/sample.shp'][0]);stream.write(b'X')
        with self.assertRaisesRegex(ValueError,'checksum'):validate(archive,True)
        with archive.open('r+b') as stream:
            stream.seek(HEADER.size);entry=list(ENTRY.unpack(stream.read(ENTRY.size)));entry[0]=b'../escape';stream.seek(HEADER.size);stream.write(ENTRY.pack(*entry))
        with self.assertRaises(ValueError):validate(archive)

    @patch('openhw_update.game_running',return_value=False)
    def test_update_preserves_user_files_and_rolls_back(self,_):
        installed=self.root/'installed';changed=self.root/'changed'
        installed.mkdir();changed.mkdir()
        (installed/'setup-complete.json').write_text('{}')
        (installed/'version.json').write_text('{"version":"dev1"}')
        (installed/'OpenHW.exe').write_bytes(b'original')
        (installed/'SavedGames').mkdir();(installed/'SavedGames'/'mine').write_bytes(b'keep')
        (changed/'OpenHW.exe').write_bytes(b'updated')
        (changed/'new.dll').write_bytes(b'library')
        (changed/'OpenHW-Presentation.obig').write_bytes(b'locally imported presentation assets')
        package=self.root/'patch.oupdate';create(installed,changed,'dev2',package)
        apply(installed,package)
        self.assertEqual((installed/'OpenHW.exe').read_bytes(),b'updated')
        self.assertEqual((installed/'SavedGames'/'mine').read_bytes(),b'keep')
        self.assertEqual((installed/'OpenHW-Presentation.obig').read_bytes(),b'locally imported presentation assets')
        self.assertEqual(rollback(installed),'dev1')
        self.assertEqual((installed/'OpenHW.exe').read_bytes(),b'original')
        self.assertFalse((installed/'new.dll').exists())
        self.assertFalse((installed/'OpenHW-Presentation.obig').exists())

    @patch('openhw_update.game_running',return_value=False)
    def test_definition_update_refuses_new_local_edits(self,_):
        installed=self.root/'installed';changed=self.root/'changed'
        installed.mkdir();changed.mkdir()
        (installed/'setup-complete.json').write_text('{}')
        (installed/'version.json').write_text('{"version":"dev1"}')
        relative=Path('Data/R1/StandardFrigate/RMNative/ship.owshipdef')
        (changed/relative).parent.mkdir(parents=True)
        (changed/relative).write_text('new baseline')
        package=self.root/'patch.oupdate';create(installed,changed,'dev2',package)
        (installed/relative).parent.mkdir(parents=True)
        (installed/relative).write_text('my changes')
        with self.assertRaisesRegex(ValueError,'Locally modified definition preserved'):
            apply(installed,package)
        self.assertEqual((installed/relative).read_text(),'my changes')
        self.assertEqual(json.loads((installed/'version.json').read_text())['version'],'dev1')

    @patch('openhw_update.game_running',return_value=False)
    def test_update_invalid_hash_and_user_target_do_not_modify_game(self,_):
        installed=self.root/'installed';changed=self.root/'changed';installed.mkdir();changed.mkdir()
        (installed/'setup-complete.json').write_text('{}');(installed/'version.json').write_text('{"version":"dev1"}')
        (installed/'OpenHW.exe').write_bytes(b'original');(changed/'OpenHW.exe').write_bytes(b'changed')
        package=self.root/'patch.oupdate';manifest=create(installed,changed,'dev2',package)
        manifest['files'][0]['sha256']='0'*64
        with zipfile.ZipFile(package,'w') as archive:
            archive.writestr('update.json',json.dumps(manifest));archive.writestr('OpenHW.exe',b'changed')
        with self.assertRaisesRegex(ValueError,'hash'):apply(installed,package)
        self.assertEqual((installed/'OpenHW.exe').read_bytes(),b'original')
        self.assertFalse(allowed('Modding/Ships/ship.owship'))
        self.assertFalse(allowed('Missions/Mission06.rtxmap'))
        with self.assertRaises(ValueError):allowed('../OpenHW.exe')

    def test_reviewed_turret_mappings_export_without_number_guessing(self):
        bindings=reviewed_bindings(ROOT/'src/Game/NativeVisualEvents.c')
        self.assertEqual([row[0] for row in bindings['guns']['StandardFrigate']],[0,1,2,5])
        self.assertEqual(bindings['guns']['P1StandardCorvette'][0][3],'-')
        self.assertEqual(bindings['guns']['HeavyInterceptor'][0][2:5],['-','-','-'])
        self.assertEqual(bindings['events']['Mothership'][0],['HangarDoor_OPEN','Open'])


if __name__=='__main__':unittest.main()
