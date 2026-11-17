import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('scan_release_security', Path(__file__).resolve().parents[1] / 'tools/scan_release_security.py')
security = importlib.util.module_from_spec(spec)
spec.loader.exec_module(security)


class ReleaseSecurityTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows Defender release gate')
    def test_failed_scan_invalidates_previous_pass_receipt(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            package = root / 'setup.exe'
            package.write_bytes(b'test fixture, not an executable')
            report = root / 'result.json'
            report.write_text('{"status":"passed"}')
            with self.assertRaises(RuntimeError):
                security.scan([package], report, lambda *a, **kw: subprocess.CompletedProcess(a, 2, b'Scanner unavailable', b''))
            self.assertEqual(json.loads(report.read_text())['status'], 'blocked')

    def test_scanner_detection_or_failure_blocks_release_and_keeps_log(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'scan.log'
            with self.assertRaises(RuntimeError):
                security.checked(['scanner'], log, lambda *a, **kw: subprocess.CompletedProcess(a, 2, b'Threat found', b''))
            self.assertIn(b'Threat found', log.read_bytes())

    def test_nested_installer_payload_is_unpacked_without_execution(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            inner = root / 'runtime.zip'
            with zipfile.ZipFile(inner, 'w') as archive:
                archive.writestr('engine.exe', b'MZ test data, never executed')
            outer = root / 'setup.exe'
            with zipfile.ZipFile(outer, 'w') as archive:
                archive.write(inner, 'Development/runtime.zip')
            security.unpack(outer, root / 'scan', [1024 * 1024])
            self.assertEqual((root / 'scan/Development/runtime.zip.payload/engine.exe').read_bytes(), b'MZ test data, never executed')

    def test_archive_traversal_and_expansion_limits_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for index, name in enumerate(('../outside.exe', 'C:/outside.exe', 'large.exe')):
                package = root / f'{index}.zip'
                with zipfile.ZipFile(package, 'w') as archive:
                    archive.writestr(name, b'large data')
                with self.assertRaises(ValueError):
                    security.unpack(package, root / f'scan{index}', [1])
            self.assertFalse((root / 'outside.exe').exists())


if __name__ == '__main__':
    unittest.main()
