from pathlib import Path
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import release_full as full
import openhw_updater as updater


class ConsolidatedInstallTests(unittest.TestCase):
    def test_replaces_runtime_and_preserves_player_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'game';stage=Path(temp)/'stage';root.mkdir();stage.mkdir()
            for name,data in [('OpenHW.exe',b'old'),('Homeworld.cfg',b'player'),('mods/load-order.txt',b'custom')]:
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
                p=stage/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'new')
            (stage/'version.json').write_text('{"version":"1.1.15"}')
            full.commit(root,stage,lambda _:None)
            self.assertEqual((root/'OpenHW.exe').read_bytes(),b'new')
            self.assertEqual((root/'Homeworld.cfg').read_bytes(),b'player')
            self.assertEqual((root/'mods/load-order.txt').read_bytes(),b'custom')
            self.assertEqual(json.loads((root/'version.json').read_text())['version'],'1.1.15')

    def test_partial_replacement_rolls_back(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'game';stage=Path(temp)/'stage';root.mkdir();stage.mkdir()
            for name in ('OpenHW.exe','version.json'):
                (root/name).write_bytes(b'original');(stage/name).write_bytes(b'new')
            original=os.replace
            def fail(source,target):
                if str(source).endswith('version.json.full-install-pending'):raise OSError('interrupted')
                return original(source,target)
            with patch.object(full.os,'replace',side_effect=fail):
                with self.assertRaisesRegex(OSError,'interrupted'):full.commit(root,stage,lambda _:None)
            self.assertEqual((root/'OpenHW.exe').read_bytes(),b'original')
            self.assertEqual((root/'version.json').read_bytes(),b'original')

    def test_any_older_numeric_version_uses_full_baseline(self):
        baseline=dict(version='1.1.15',to='1.1.15',format='openhw-full-v1')
        feed=dict(latest='1.1.15',full_install=baseline,patches=[])
        for version in ('0.91.3','1.0','1.0.7','1.1','1.1.1','1.1.14','1.1.14.999'):
            self.assertEqual(updater.update_plan(feed,version),[baseline])
        self.assertEqual(updater.update_plan(feed,'1.1.15'),[])
        later=dict(to='1.1.16',**{'from':['1.1.15']})
        feed.update(latest='1.1.16',patches=[later])
        self.assertEqual(updater.update_plan(feed,'1.0.7'),[baseline,later])


if __name__=='__main__':unittest.main()
