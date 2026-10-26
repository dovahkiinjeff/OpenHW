"""Regressions for the 1.0 missing authored clouds/lighting deployment."""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from release_mission_authoring import select_mission_files,publish_mission_defaults,verify_published_missions


class MissionAuthoringReleaseTests(unittest.TestCase):
    def test_wip_runtime_exports_win_over_old_packaged_maps(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'openhw.loose').touch()
            for path,data in [('Data/Missions/Mission16.rtxmap','RTXMAP 8\nMISSION 16\n'),
                              ('Dev/RTXExports/Maps/Mission16.rtxmap','RTXMAP 9\nMISSION 16\nCLOUD_RENDERER latest\n'),
                              ('RTXLighting/Mission16.rtxlight','RTXMISSIONLIGHTING 5\nenabled 1\n'),
                              ('Dev/RTXExports/Missions/Mission16.rtxlight','unused export')]:
                target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(data)
            rows=select_mission_files(root)
            self.assertEqual([r['relativeSource'] for r in rows],
                ['Dev/RTXExports/Maps/Mission16.rtxmap','RTXLighting/Mission16.rtxlight'])
            self.assertFalse(any(r['mission']==15 and r['name'].endswith('rtxlight') for r in rows))

    def test_fresh_install_publishes_both_stdio_loader_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'generated/missions';source.mkdir(parents=True)
            (source/'mission01.rtxmap').write_bytes(b'RTXMAP 9\nMISSION 1\ncloud settings')
            (source/'mission01.rtxlight').write_bytes(b'RTXMISSIONLIGHTING 5\nenabled 1')
            override=root/'RTXExports/Maps/Mission01.rtxmap';override.parent.mkdir(parents=True);override.write_bytes(b'user override')
            self.assertEqual(publish_mission_defaults(source,root),2)
            for name in ('mission01.rtxmap','mission01.rtxlight'):
                self.assertEqual((root/'Data/Missions'/name).read_bytes(),(source/name).read_bytes())
                self.assertEqual((root/'missions'/name).read_bytes(),(source/name).read_bytes())
            self.assertEqual(override.read_bytes(),b'user override')

    def test_wrong_mission_number_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);path=root/'Data/Missions/Mission01.rtxmap';path.parent.mkdir(parents=True);path.write_text('RTXMAP 9\nMISSION 2\n')
            with self.assertRaisesRegex(ValueError,'Invalid mission map'):select_mission_files(root)

    def test_archive_or_root_missions_copy_cannot_pass_runtime_verification(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'profile/missions';source.mkdir(parents=True)
            data=b'RTXMAP 9\nMISSION 1\n'
            (source/'mission01.rtxmap').write_bytes(data)
            (root/'missions').mkdir();(root/'missions/mission01.rtxmap').write_bytes(data)
            with self.assertRaisesRegex(ValueError,'runtime loader path'):
                verify_published_missions(source,root)
            publish_mission_defaults(source,root)
            self.assertEqual(verify_published_missions(source,root)['maps'],1)


if __name__=='__main__':unittest.main()
