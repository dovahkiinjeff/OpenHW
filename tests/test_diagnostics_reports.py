import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import openhw_diagnostics as report


class Reports(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/'Development/Logs').mkdir(parents=True)
        (self.root/'SavedGames/SinglePlayer').mkdir(parents=True)
        self.log='Development/Logs/renderer.log'
        self.save='SavedGames/SinglePlayer/Mission 2'
        (self.root/self.log).write_text('diagnostic data\n')
        (self.root/self.save).write_bytes(b'owned save')
        (self.root/'secret.txt').write_text('not part of report')

    def test_selected_files_and_comment_round_trip(self):
        output=report.build_report(self.root,'Community Alias','Mission 2: salvage hangs',[self.log],self.save)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(set(archive.namelist()),{'report.json','logs/renderer.log','saves/Mission 2'})
            meta=json.loads(archive.read('report.json'))
            self.assertEqual(meta['alias'],'Community Alias')
            self.assertEqual(meta['comment'],'Mission 2: salvage hangs')
            self.assertEqual(meta['files'][1],{'path':'saves/Mission 2','size':10})
            self.assertNotIn(str(self.root), archive.read('report.json').decode())

    def test_alias_never_becomes_a_local_path(self):
        output=report.build_report(self.root,'../../alias','',[self.log])
        self.assertEqual(output.parent, self.root/'Development/Reports')
        self.assertTrue(output.name.startswith('report-'))

    def test_no_implicit_save_or_arbitrary_file(self):
        with self.assertRaises(ValueError):report.build_report(self.root,'alias','',['secret.txt'])
        with self.assertRaises(ValueError):report.build_report(self.root,'alias','',[self.log],'secret.txt')
        with zipfile.ZipFile(report.build_report(self.root,'alias','',[self.log])) as archive:
            self.assertFalse(any(name.startswith('saves/') for name in archive.namelist()))

    def test_input_limits(self):
        for alias,comment,logs in (('','',[self.log]),('x'*81,'',[self.log]),('alias','x'*4001,[self.log]),('alias','',[]),('alias','',[self.log,self.log])):
            with self.assertRaises(ValueError):report.build_report(self.root,alias,comment,logs)

    def test_size_limit_and_incomplete_archive_cleanup(self):
        with patch.object(report,'MAX_EXPANDED',3):
            with self.assertRaises(ValueError):report.build_report(self.root,'alias','',[self.log])
        with patch.object(report,'MAX_UPLOAD',10):
            with self.assertRaises(ValueError):report.build_report(self.root,'alias','',[self.log])
        self.assertEqual(list((self.root/'Development/Reports').glob('*.zip')),[])

    def test_upload_redirect_refused(self):
        with self.assertRaises(ValueError):report.NoRedirect().redirect_request(None,None,None,None,None)

    def test_success_receipt_and_streamed_upload(self):
        output=report.build_report(self.root,'alias','',[self.log])
        class Response:
            status=201
            def __enter__(self):return self
            def __exit__(self,*a):pass
            def read(self,n):return b'{"report_id":"alias-20261113-1234"}'
        class Opener:
            def open(inner, request, timeout):
                self.assertEqual(request.full_url,report.ENDPOINT)
                self.assertTrue(hasattr(request.data,'read'))
                self.assertEqual(int(request.headers['Content-length']),output.stat().st_size)
                return Response()
        with patch.object(report.urllib.request,'build_opener',return_value=Opener()):
            self.assertEqual(report.upload_report(output),'alias-20261113-1234')
        self.assertTrue(output.with_suffix('.receipt.json').is_file())


if __name__=='__main__':unittest.main()
