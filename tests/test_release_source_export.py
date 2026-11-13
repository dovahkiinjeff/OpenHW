"""Keep cumulative public source complete without exporting installed/private data."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from export_release_source import export
from release_content_policy import digest
from release_source_manifest import write


class ReleaseSourceExportTests(unittest.TestCase):
    def fixture(self, root):
        files = {
            'assets/Release/content-inventory.json': '{"schema":2,"files":[]}',
            'assets/UI/ART_PROVENANCE.md': 'Reviewed supplied artwork.\n',
            'assets/UI/CONCEPT_ART_MANIFEST.json': '{}',
            'tools/sky_materials/btg_microstructure.png': 'shader noise fixture',
            'src/Network/OnlineProtocol.h': '#define PROTOCOL 1\n',
            'server/coordinator.py': '# public server source\n',
            'server/openhw-multiplayer.service': '[Service]\n',
            'server/openhw-update-mirror.timer': '[Timer]\n',
            'server/requirements.txt': 'websockets==17.3\n',
            'server/state/config.json': '{"private":"fixture"}',
            'server/game/OpenHW-HW1.obig': 'private owned content fixture',
            'server/logs/capture.py': '# private runtime fixture\n',
            'out/dependency.dll': 'build output fixture',
            'SOURCE_MANIFEST.json': '{"obsolete":"do not hash or copy"}',
            'docs/media/release.jpg': 'illustration fixture',
            'tools/launcher.rc': 'resource fixture',
        }
        for name, text in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)

    def test_export_includes_server_unit_and_network_source_without_private_state(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'repo'
            output = Path(folder) / 'source'
            self.fixture(root)
            export(root, output, version='1.1')
            manifest = json.loads((output / 'SOURCE_MANIFEST.json').read_text())
            names = {row['path'] for row in manifest['files']}
            self.assertEqual(manifest['version'], '1.1')
            self.assertTrue({'src/Network/OnlineProtocol.h', 'server/coordinator.py',
                             'server/openhw-multiplayer.service', 'server/openhw-update-mirror.timer',
                             'server/requirements.txt','docs/media/release.jpg','tools/launcher.rc'} <= names)
            self.assertFalse(any(name.startswith(('server/state/', 'server/game/', 'server/logs/', 'out/'))
                                 for name in names))
            self.assertNotIn('SOURCE_MANIFEST.json', names)
            for row in manifest['files']:
                self.assertEqual(digest(output / row['path']), row['sha256'])

    def test_manifest_version_is_explicit_and_never_hashes_itself(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'source.c').write_text('int value;\n')
            (root / 'SOURCE_MANIFEST.json').write_text('{}')
            value = write(root, '1.1.2')
            self.assertEqual(value['version'], '1.1.2')
            self.assertEqual([row['path'] for row in value['files']], ['source.c'])


if __name__ == '__main__':
    unittest.main()
