"""Synthetic archive and transactional cache tests; no game assets required."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import import_local_assets as importer
from extract_remastered_big import extract, safe_path
from audit_local_import_release import audit
from stage_local_import_release import stage


def archive_bytes(payload=b'example', folder=b'background\\ez02', filename=b'example.bin', compressed=False):
    stored = zlib.compress(payload) if compressed else payload
    # Minimal valid section metadata: TOC, one folder, one file, string table.
    names = folder+b'\0'+filename+b'\0'
    string_offset = 24+138+12+17
    metadata = struct.pack('<IHIHIHIH', 24,1,162,1,174,1,string_offset,2)
    metadata += bytes(138)+struct.pack('<I4H',0,1,1,0,1)
    metadata += struct.pack('<IBIII',len(folder)+1,0x20 if compressed else 0,264,len(stored),len(payload))+names
    header = b'_ARCHIVE\x02\0\0\0'+bytes(160)+struct.pack('<II',len(metadata),180+len(metadata))
    return header+metadata+bytes(260)+struct.pack('<I',zlib.crc32(payload))+stored


class LocalAssetImportTests(unittest.TestCase):
    def test_safe_paths(self):
        for name in ('../escape','C:/file','//server/share','a/../../x','a/NUL.dds','a/x:stream','a/file.','a/file\x00'):
            with self.subTest(name=name), self.assertRaises(ValueError): safe_path(name)
        self.assertEqual(safe_path('background\\ez02\\sky.hod').as_posix(),'background/ez02/sky.hod')

    def test_sga_stored_and_compressed_crc(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for compressed in (False, True):
                archive = root/'test.big'
                original = archive_bytes(compressed=compressed)
                archive.write_bytes(original)
                self.assertEqual(extract(archive,root/'cache'),['background/ez02/example.bin'])
                self.assertEqual((root/'cache/background/ez02/example.bin').read_bytes(),b'example')
                self.assertEqual(archive.read_bytes(),original)

    def test_sga_corruption_and_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for data in (archive_bytes()[:-1],archive_bytes(folder=b'background\\..\\..'),
                         archive_bytes()[:-1]+b'x',b'_ARCHIVE'+bytes(180)):
                (root/'bad.big').write_bytes(data)
                with self.assertRaises(ValueError): extract(root/'bad.big',root/'cache')

    def test_transaction_failure_preserves_previous_generation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, cache, tools = root/'source',root/'cache',root/'tools'
            source.mkdir(); tools.mkdir()
            (source/'HWBackgrounds.big').write_bytes(b'source archive fixture')
            (source/'Homeworld2.big').write_bytes(b'shader/effect archive fixture')
            def generate(folder,work,output,tools,side):
                (output/'fixture.bin').write_bytes(b'converted fixture')
                return dict(status='complete')
            arguments = ['importer','--cache',str(cache),'--tools',str(tools),'--only','remastered','--remastered',str(source)]
            with patch.object(sys,'argv',arguments), patch.object(importer,'import_backgrounds',generate):
                self.assertEqual(importer.main(),0)
            prior = (cache/'active.json').read_bytes()
            prior_root = (cache/'active-root.txt').read_bytes()
            with patch.object(sys,'argv',arguments+['--force']), patch.object(importer,'import_backgrounds',side_effect=RuntimeError('test failure')):
                with self.assertRaises(RuntimeError): importer.main()
            self.assertEqual((cache/'active.json').read_bytes(),prior)
            self.assertEqual((cache/'active-root.txt').read_bytes(),prior_root)
            self.assertEqual(json.loads((cache/'status.json').read_text())['status'],'failed')
            with patch.object(sys,'argv',arguments), patch.object(importer,'import_backgrounds',side_effect=AssertionError('must reuse cache')):
                self.assertEqual(importer.main(),0)

    def test_cache_checks_output_hash_and_source_change(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, output = root/'source.big',root/'out.bin'
            source.write_bytes(b'source'); output.write_bytes(b'output')
            receipt = dict(version=importer.VERSION,sources=[importer.stamp(source)],outputs=[
                dict(path='out.bin',size=6,sha256=importer.digest(output))])
            self.assertTrue(importer.validate_cache(root,receipt))
            output.write_bytes(b'broken')
            self.assertFalse(importer.validate_cache(root,receipt))
            output.write_bytes(b'output'); source.write_bytes(b'changed source')
            self.assertFalse(importer.validate_cache(root,receipt))

    def test_invalid_manual_install_not_silently_replaced(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError): importer.discover(explicit_rm=temp)

    def test_release_stage_excludes_loose_and_packed_imports(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); source=root/'build'; output=root/'release'
            (source/'AssetImporter').mkdir(parents=True)
            (source/'AssetImporter/HomeworldAssetImporter.exe').write_bytes(b'importer')
            (source/'HomeworldModern.exe').write_bytes(b'game')
            (source/'R1/HeavyInterceptor/RMNative').mkdir(parents=True)
            (source/'R1/HeavyInterceptor/RMNative/model.geo').write_bytes(b'private model')
            (source/'RemasteredBackgrounds').mkdir()
            (source/'RemasteredBackgrounds/mission02_sky.dds').write_bytes(b'imported background')
            entries=[(b'r1\\heavyinterceptor\\rmnative\\color.dds',b'private'),(b'r1\\mothership\\color.dds',b'keep')]
            offset=24+272*len(entries)
            with (source/'HomeworldTextures.hwt').open('wb') as stream:
                stream.write(struct.pack('<8sIIQ',b'HWTPACK1',1,len(entries),offset))
                for name,data in entries:
                    stream.write(struct.pack('<256sQII',name,offset,len(data),0)); offset+=len(data)
                for name,data in entries: stream.write(data)
            self.assertTrue(audit(source))
            stage(source,output)
            self.assertFalse(audit(output))
            self.assertTrue((source/'R1/HeavyInterceptor/RMNative/model.geo').is_file())
            self.assertFalse((output/'R1/HeavyInterceptor/RMNative/model.geo').exists())
            self.assertFalse((output/'RemasteredBackgrounds/mission02_sky.dds').exists())
            packed=(output/'HomeworldTextures.hwt').read_bytes()
            self.assertEqual(struct.unpack_from('<I',packed,12)[0],1)
            self.assertEqual(packed[-4:],b'keep')
            with self.assertRaises(ValueError): stage(source,output)

    def test_background_pass_refuses_other_imports(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            arguments=['importer','--cache',str(root/'cache'),'--tools',str(root/'tools'),
                       '--only','unsupported']
            with patch.object(sys,'argv',arguments), self.assertRaises(SystemExit):
                importer.main()
            self.assertFalse((root/'cache').exists())


if __name__ == '__main__': unittest.main()
