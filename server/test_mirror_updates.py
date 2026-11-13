import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import mirror_updates as mirror


class MirrorTests(unittest.TestCase):
    def test_verified_publication_and_failed_replacement_keeps_previous_feed(self):
        good=b'MZ-candidate'
        row=dict(version='1.1',url=mirror.REPOSITORY_DOWNLOAD+'v1.1/OpenHW-Setup.exe',
            mirrors=['https://openhw.tech/downloads/v1.1/OpenHW-Setup.exe'],size=len(good),sha256=hashlib.sha256(good).hexdigest())
        feed=dict(schema=1,channel='stable',latest='1.1',patches=[],fresh_install=row)
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            with patch.object(mirror.urllib.request,'urlopen',side_effect=[io.BytesIO(json.dumps(feed).encode()),io.BytesIO(good)]):mirror.sync(root)
            previous=(root/'updates/stable.json').read_bytes()
            self.assertEqual(mirror.hosted_path(row['url'],root).read_bytes(),good)
            feed['latest']='1.2';feed['fresh_install']=dict(row,version='1.2',url=mirror.REPOSITORY_DOWNLOAD+'v1.2/OpenHW-Setup.exe')
            with patch.object(mirror.urllib.request,'urlopen',side_effect=[io.BytesIO(json.dumps(feed).encode()),io.BytesIO(b'x'*len(good))]):
                with self.assertRaisesRegex(ValueError,'checksum'):mirror.sync(root)
            self.assertEqual((root/'updates/stable.json').read_bytes(),previous)
            self.assertFalse(mirror.hosted_path(feed['fresh_install']['url'],root).exists())

    def test_path_traversal_and_other_repositories_refused(self):
        for url in ('https://github.com/other/repo/releases/download/v1/u.exe',mirror.REPOSITORY_DOWNLOAD+'v1/../../secrets',mirror.REPOSITORY_DOWNLOAD+'v1/%2e%2e/secret'):
            with self.assertRaises((ValueError,RuntimeError)):mirror.hosted_path(url)

if __name__=='__main__':unittest.main()
