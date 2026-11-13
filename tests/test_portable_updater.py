import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_updater as updater


def package(folder, version, files, extra=None):
    rows = [dict(path=name,size=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in files.items()]
    data = io.BytesIO()
    with zipfile.ZipFile(data,'w',zipfile.ZIP_DEFLATED) as archive:
        for name, contents in files.items(): archive.writestr(name,contents)
        archive.writestr('Development/Patches/'+version+'/patch.json',json.dumps(dict(version=version,files=rows)))
        if extra: archive.writestr(*extra)
    payload = data.getvalue(); head = b'MZ-test-only-no-execution'
    path = folder/(version+'.exe')
    path.write_bytes(head+payload+updater.FOOTER.pack(b'OPENHW1SETUPZIP\0',len(head),len(payload),hashlib.sha256(payload).digest()))
    return path


def entry(previous, target, path=None):
    return dict(to=target, **{'from':[previous]},format='openhw-exe-zip-v1',
        url='https://github.com/dovahkiinjeff/OpenHW/releases/download/v'+target+'/OpenHW-Patch-'+target+'.exe',
        sha256=updater.digest(path) if path else 'a'*64,size=path.stat().st_size if path else 100,
        post_actions=[])


class PortableUpdaterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='OpenHW-updater-test-')
        self.root = Path(self.temporary.name)
        (self.root/'setup-complete.json').write_text('{"version":"1.0"}')
        (self.root/'version.json').write_text('{"version":"1.0","custom":"keep"}')
        (self.root/'OpenHW.exe').write_bytes(b'old-normal')
        (self.root/'OpenHW-Daignostics.exe').write_bytes(b'old-diagnostic')
        (self.root/'Homeworld.cfg').write_bytes(b'user settings')
        (self.root/'SavedGames').mkdir(); (self.root/'SavedGames/save').write_bytes(b'user save')
        (self.root/'mods').mkdir(); (self.root/'mods/my-mod').write_bytes(b'user mod')
        self.running = patch.object(updater,'game_running',return_value=False); self.running.start()
    def tearDown(self):
        self.running.stop(); self.temporary.cleanup()
    def progress(self,*args): pass
    def unchanged_player_files(self):
        self.assertEqual((self.root/'Homeworld.cfg').read_bytes(),b'user settings')
        self.assertEqual((self.root/'SavedGames/save').read_bytes(),b'user save')
        self.assertEqual((self.root/'mods/my-mod').read_bytes(),b'user mod')
    def test_version_order_and_future_graph(self):
        versions = ['1.0','1.0.1','1.0.2','1.0.3','1.0.4','1.0.4.5','1.0.5','1.1','2.0','10.0']
        chain = [entry(a,b) for a,b in zip(versions,versions[1:])]
        manifest = dict(schema=1,channel='stable',latest='10.0',patches=chain)
        updater.validate_manifest(manifest)
        for index,version in enumerate(versions):
            self.assertEqual([p['to'] for p in updater.update_plan(manifest,version)], versions[index+1:])
        self.assertEqual(updater.compare_versions('1.0','1.0.0'),0)
        self.assertEqual(updater.update_plan(manifest,'11.0'),[])
        chain.append(entry('1.0','10.0'))
        self.assertEqual(len(updater.update_plan(manifest,'1.0')),1)
    def test_apply_and_metadata_preserve(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new-normal','OpenHW-Daignostics.exe':b'new-diagnostic'})
        value = updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertEqual(value,'1.0.1')
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'new-normal')
        record = updater.read_json(self.root/'version.json')
        self.assertEqual(record['custom'],'keep'); self.assertEqual(record['normal'],hashlib.sha256(b'new-normal').hexdigest())
        self.assertTrue((self.root/'Development/Patches/1.0.1/applied.json').is_file())
        self.unchanged_player_files()
    def test_failed_maintenance_rolls_back(self):
        before = (self.root/'version.json').read_bytes()
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new'})
        def failure(*args): raise RuntimeError('maintenance failed')
        with self.assertRaisesRegex(RuntimeError,'maintenance failed'):
            updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress,actions=failure)
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'old-normal')
        self.assertEqual((self.root/'version.json').read_bytes(),before)
        self.assertFalse((self.root/'Development/Patches/1.0.1/applied.json').exists())
        self.unchanged_player_files()

    def test_launcher_is_installed_with_patch_and_preserves_player_files(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new','Launcher.exe':b'launcher'})
        with patch.object(updater,'launcher_running',return_value=False):
            updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertEqual((self.root/'Launcher.exe').read_bytes(),b'launcher')
        self.unchanged_player_files()

    def test_running_launcher_refused_before_any_replacement(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new','Launcher.exe':b'launcher'})
        before=(self.root/'version.json').read_bytes()
        with patch.object(updater,'launcher_running',return_value=True):
            with self.assertRaisesRegex(updater.UpdateError,'Close Launcher.exe'):
                updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'old-normal')
        self.assertEqual((self.root/'version.json').read_bytes(),before)
        self.assertFalse((self.root/'Launcher.exe').exists())
        self.unchanged_player_files()
    def test_interrupted_commit_recovered_before_update(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new'})
        updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        journal_path = next((self.root/'Development/Updater/Transactions').glob('*/transaction.json'))
        journal = updater.read_json(journal_path); journal['state']='applying'; updater.atomic_json(journal_path,journal)
        updater.recover(self.root,self.progress)
        self.assertEqual(updater.detect_version(self.root),'1.0')
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'old-normal')
        self.unchanged_player_files()
    def test_player_data_and_unsafe_archive_rejected(self):
        for name in ('SavedGames/lost','mods/my-mod','Homeworld.cfg','RTXLighting/map.rtxlight',
                     '../outside','C:/outside','Data/x:stream','Data/CON.txt','Data/name.'):
            with self.subTest(name=name):
                path = package(self.root,'1.0.1',{name:b'overwrite forbidden'})
                with self.assertRaises(updater.UpdateError):
                    updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
                self.unchanged_player_files()
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new'},('../outside',b'invalid'))
        with self.assertRaises(updater.UpdateError): updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'old-normal')
    def test_corruption_rejected_before_install(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new'})
        data = bytearray(path.read_bytes()); data[32] ^= 1; path.write_bytes(data)
        with self.assertRaises(updater.UpdateError): updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertEqual((self.root/'OpenHW.exe').read_bytes(),b'old-normal')
    def test_running_game_does_not_change_files(self):
        path = package(self.root,'1.0.1',{'OpenHW.exe':b'new'})
        with patch.object(updater,'game_running',return_value=True):
            with self.assertRaises(updater.UpdateError): updater.apply_patch(self.root,entry('1.0','1.0.1',path),path,self.progress)
        self.assertFalse((self.root/'Development').exists())
    def test_reject_untrusted_feed_and_missing_path(self):
        for url in ('http://github.com/dovahkiinjeff/OpenHW/releases/download/x',
                    'https://github.com/other/project/releases/download/x','https://example.com/update.exe'):
            with self.assertRaises(updater.UpdateError): updater.official_url(url)
        with self.assertRaises(updater.UpdateError): updater.update_plan(dict(latest='5.0',patches=[]),'1.0')
    def test_cached_verified_download(self):
        cache = self.root/'cache'; cache.mkdir()
        content = b'already downloaded'; row = entry('1.0','1.0.1'); row.update(size=len(content),sha256=hashlib.sha256(content).hexdigest())
        expected = cache/(row['sha256']+'.exe'); expected.write_bytes(content)
        with patch.object(updater,'network_open',side_effect=AssertionError('network not needed')):
            self.assertEqual(updater.download(row,cache,self.progress,lambda:False),expected)
    def test_resumed_download_and_cancellation(self):
        cache = self.root/'cache'; cache.mkdir()
        data = b'0123456789'; row=entry('1.0','1.0.1'); row.update(size=len(data),sha256=hashlib.sha256(data).hexdigest())
        pending=cache/(row['sha256']+'.part'); pending.write_bytes(data[:4])
        class Response(io.BytesIO):
            status=206
            headers={'Content-Range':'bytes 4-9/10'}
        def response(url,headers):
            self.assertEqual(headers['Range'],'bytes=4-')
            return Response(data[4:])
        with patch.object(updater,'network_open',side_effect=response):
            result=updater.download(row,cache,self.progress,lambda:False)
        self.assertEqual(result.read_bytes(),data)
        result.unlink(); pending.write_bytes(data[:4])
        with patch.object(updater,'network_open',side_effect=response):
            with self.assertRaises(updater.Cancelled): updater.download(row,cache,self.progress,lambda:True)
        self.assertEqual(pending.read_bytes(),data[:4])
    def test_self_update_precedes_new_protocol_actions(self):
        own=dict(version='2.0.0',url=entry('1.0','2.0')['url'],sha256='a'*64,size=100)
        feed=dict(schema=1,updater=own,minimum_protocol=2,patches=[{'format':'future-format'}])
        candidate=self.root/'new-updater.exe'; candidate.write_bytes(b'fixture')
        with patch.object(updater,'download',return_value=candidate), patch.object(updater,'request_self_update') as restart:
            self.assertEqual(updater.update(self.root,self.progress,manifest=feed),'restart')
            restart.assert_called_once()
        self.assertEqual(updater.detect_version(self.root),'1.0')


if __name__ == '__main__': unittest.main()
