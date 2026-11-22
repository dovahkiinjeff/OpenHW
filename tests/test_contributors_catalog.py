"""Ensure public credits cover the project's locked dependency trees."""
from pathlib import Path
import json
import tomllib
import unittest
from urllib.parse import urlparse

ROOT=Path(__file__).resolve().parents[1]


class ContributorCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((ROOT/'docs/contributors.json').read_text(encoding='utf-8'))
        cls.entries=cls.data['entries']

    def test_required_projects_and_honest_terms(self):
        names={e['name'] for e in self.entries}
        self.assertTrue({'HODEditorJS','NVIDIA Streamline / DLSS / NGX','AMD FidelityFX SDK','Intel XeSS',
                         'OpenMW volumetric clouds','three-good-godrays','sdl2','ffmpeg','DirectXTex'.lower(),
                         'CPython','numpy','pillow','websockets','Rajdhani','Share Tech Mono','dompurify','marked'} <= names)
        hod=next(e for e in self.entries if e['name']=='HODEditorJS')
        self.assertEqual(hod['license'],'CC-BY-NC-4.0')
        cloud=next(e for e in self.entries if e['name']=='OpenMW volumetric clouds')
        self.assertIn('unresolved',cloud['license'])

    def test_windows_editor_dependencies_are_credited(self):
        lock=tomllib.loads((ROOT/'tools/hod-editor/src-tauri/Cargo.lock').read_text(encoding='utf-8'))
        expected={(p['name'],p['version']) for p in lock['package'] if p.get('source')}
        actual={(p['name'],p['version']) for p in self.entries if p['group']=='HOD editor · Rust'}
        # Cargo.lock also retains Linux/macOS/WASM-only packages. This catalog
        # explicitly covers the resolved Windows runtime/build dependency closure.
        self.assertTrue(actual <= expected)
        direct=set()
        for path in ('tools/hod-editor/src-tauri/Cargo.toml','tools/hod-editor/parser/Cargo.toml'):
            manifest=tomllib.loads((ROOT/path).read_text(encoding='utf-8'))
            direct.update(manifest.get('dependencies',{}))
            for target in manifest.get('target',{}).values():
                direct.update(target.get('dependencies',{}))
        direct.discard('hwr_hod_parser')
        self.assertTrue(direct <= {name for name,version in actual})

    def test_editor_javascript_lock_is_credited(self):
        lock=json.loads((ROOT/'tools/hod-editor/package-lock.json').read_text(encoding='utf-8'))
        expected={(p.get('name') or key.split('node_modules/')[-1],p['version']) for key,p in lock['packages'].items() if key and p.get('version')}
        actual={(p['name'],p['version']) for p in self.entries if p['group']=='HOD editor · JavaScript'}
        self.assertEqual(expected,actual)

    def test_links_and_notices_are_complete_and_safe(self):
        self.assertEqual(len(self.entries),len({e['id'] for e in self.entries}))
        serialized=json.dumps(self.data,ensure_ascii=False)
        self.assertNotIn('C:\\Users\\',serialized)
        self.assertNotIn('Ã‚',serialized)
        self.assertNotIn('Ã¢',serialized)
        for entry in self.entries:
            for field in ('upstream','evidence'):
                url=urlparse(entry[field]);self.assertEqual(url.scheme,'https',entry['name']);self.assertTrue(url.hostname)
            self.assertTrue(entry['role']);self.assertTrue(entry['license'])
            if entry['notices']:
                self.assertTrue(entry['notices'].startswith('/data/contributor-notices/'))
                path=ROOT/'docs/contributor-notices'/Path(entry['notices']).name
                self.assertTrue(path.is_file(),entry['name'])
                self.assertGreater(path.stat().st_size,0)


if __name__=='__main__':unittest.main()
