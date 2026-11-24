import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('authored_defaults', ROOT/'tools/authored_mission_defaults.py')
defaults = importlib.util.module_from_spec(spec)
spec.loader.exec_module(defaults)


class AuthoredDefaultsTests(unittest.TestCase):
    def test_actual_approved_authoring_survives_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary)/'missions'
            destination.mkdir()
            (destination/'mission01.rtxmap').write_bytes(b'old placeholder')
            (destination/'mission17.rtxmap').write_bytes(b'future authoring')
            manifest = json.loads((ROOT/'assets/Missions/authored-defaults.json').read_text())
            self.assertEqual(defaults.stage(ROOT, destination), len(manifest['files']))
            self.assertEqual({f['mission'] for f in manifest['files']}, set(range(1,17)))
            for entry in manifest['files']:
                self.assertEqual((destination/entry['path'].lower()).read_bytes(),
                                 (ROOT/'assets/Missions'/entry['path']).read_bytes())
            self.assertEqual((destination/'mission17.rtxmap').read_bytes(), b'future authoring')

    def test_staged_release_refreshes_old_profile_and_approval_hashes(self):
        with tempfile.TemporaryDirectory() as temporary:
            development = Path(temporary)/'Development'
            mission = development/'Content/missions/mission01.rtxmap'
            mission.parent.mkdir(parents=True)
            mission.write_bytes(b'old profile')
            untouched = dict(path='Content/ui/approved.png',category='supplied-concept-art',sha256='preserve')
            inventory = dict(schema=2,files=[dict(path='Content/missions/mission01.rtxmap',category='authored-text',sha256='old'),untouched])
            defaults.stage_release_content(ROOT,development,inventory)
            self.assertEqual(mission.read_bytes(),(ROOT/'assets/Missions/Mission01.rtxmap').read_bytes())
            staged = json.loads((development/'content-inventory.json').read_text())
            records = {row['path']:row for row in staged['files']}
            self.assertEqual(records[untouched['path']],untouched)
            self.assertEqual(inventory['files'][0]['sha256'],'old')
            manifest = json.loads((development/'mission-authoring.json').read_text())
            for entry in manifest['files']:
                self.assertEqual(records['Content/missions/'+entry['path'].lower()]['sha256'],entry['sha256'])
            self.assertFalse((development/'Content/missions/authored-defaults.json').exists())

    def test_reject_stale_source_before_changing_payload(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary = Path(temporary)
            repo = temporary/'repo'
            source = repo/'assets/Missions'
            source.mkdir(parents=True)
            manifest = json.loads((ROOT/'assets/Missions/authored-defaults.json').read_text())
            for entry in manifest['files']:
                shutil.copy2(ROOT/'assets/Missions'/entry['path'], source/entry['path'])
            (source/'authored-defaults.json').write_text(json.dumps(manifest))
            (source/manifest['files'][-1]['path']).write_bytes(b'unapproved edit')
            destination = temporary/'payload'
            with self.assertRaisesRegex(ValueError, 'changed without promotion'):
                defaults.stage(repo, destination)
            self.assertFalse(destination.exists())


if __name__ == '__main__':
    unittest.main()
