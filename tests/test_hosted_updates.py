"""Compatibility contracts for hosted 1.1 and the released GitHub-only updater."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import openhw_updater as updater
from publish_update_manifest import generate

GITHUB='https://github.com/dovahkiinjeff/OpenHW/releases/download/v1.1/'
HOST='https://openhw.tech/downloads/v1.1/'

def fresh():
    return dict(version='1.1',url=GITHUB+'OpenHW-Setup.exe',mirrors=[HOST+'OpenHW-Setup.exe'],size=100,sha256='a'*64)

def feed():
    return dict(schema=1,channel='stable',latest='1.1',minimum_protocol=3,patches=[],fresh_install=fresh())

class HostedUpdates(unittest.TestCase):
    def test_unavailable_or_malformed_primary_uses_github(self):
        for failure in (OSError('offline'),io.BytesIO(b'not JSON')):
            with patch.object(updater,'network_open',side_effect=[failure,io.BytesIO(json.dumps(feed()).encode())]) as opened:
                self.assertEqual(updater.fetch_manifest()['latest'],'1.1')
                self.assertTrue(opened.call_args_list[0].args[0].startswith(updater.MANIFEST_URL))
                self.assertTrue(opened.call_args_list[1].args[0].startswith(updater.FALLBACK_MANIFEST_URL))

    def test_primary_corruption_falls_back_to_verified_github_bytes(self):
        good=b'correct executable'; row=dict(url=GITHUB+'Updater.exe',mirrors=[HOST+'Updater.exe'],size=len(good),sha256=hashlib.sha256(good).hexdigest())
        class Incoming(io.BytesIO):
            status=200
            headers={}
        with tempfile.TemporaryDirectory() as folder,patch.object(updater,'network_open',side_effect=[Incoming(b'x'*len(good)),Incoming(good)]) as opened,patch.object(updater.time,'sleep'):
            result=updater.download(row,Path(folder),lambda *a:None,lambda:False)
            self.assertEqual(result.read_bytes(),good)
            self.assertEqual([c.args[0] for c in opened.call_args_list],[HOST+'Updater.exe',GITHUB+'Updater.exe'])

    def test_all_mirrors_reach_canonical_fallback(self):
        good=b'OK';row=dict(url=GITHUB+'Updater.exe',mirrors=[HOST+str(i)+'.exe' for i in range(4)],size=2,sha256=hashlib.sha256(good).hexdigest())
        class Incoming(io.BytesIO):status=200;headers={}
        with tempfile.TemporaryDirectory() as folder,patch.object(updater,'network_open',side_effect=[OSError('down')]*4+[Incoming(good)]) as opened,patch.object(updater.time,'sleep'):
            self.assertEqual(updater.download(row,folder,lambda *a:None,lambda:False).read_bytes(),good)
            self.assertEqual(opened.call_args.args[0],GITHUB+'Updater.exe')

    def test_fresh_install_leaves_old_install_untouched(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            originals={'setup-complete.json':b'{"version":"1.0.7"}','version.json':b'{"version":"1.0.7"}','OpenHW.exe':b'oldgame','Homeworld.cfg':b'usersettings'}
            for name,data in originals.items():(root/name).write_bytes(data)
            with patch.object(updater,'fetch_manifest',return_value=feed()),patch.object(updater,'game_running',return_value=False):
                with self.assertRaises(updater.FreshInstallRequired):updater.update(root,lambda *a:None,lambda:False,self_update=False)
            for name,data in originals.items():self.assertEqual((root/name).read_bytes(),data)

    def test_released_client_can_bootstrap_before_new_protocol(self):
        value=feed(); value['updater']=dict(version='1.1.0',protocol=3,url=GITHUB+'Updater.exe',mirrors=[HOST+'Updater.exe'],size=10,sha256='b'*64)
        # Simulate the installed 1.0.1 protocol/version. Its self-update branch
        # must run before the 1.1 protocol and fresh-install requirement.
        with tempfile.TemporaryDirectory() as folder,patch.object(updater,'UPDATER_VERSION','1.0.1'),patch.object(updater,'PROTOCOL',2),patch.object(updater,'detect_version',return_value='1.0.7'),patch.object(updater,'fetch_manifest',return_value=value),patch.object(updater,'game_running',return_value=False),patch.object(updater,'download',return_value=Path(folder)/'Updater.exe'),patch.object(updater,'request_self_update') as bootstrap:
            self.assertEqual(updater.update(Path(folder),lambda *a:None,lambda:False),'restart')
            bootstrap.assert_called_once()
        self.assertTrue(value['updater']['url'].startswith(updater.REPOSITORY_DOWNLOAD))

    def test_rejects_spoofed_or_traversing_host_urls(self):
        for url in ('http://openhw.tech/downloads/u.exe','https://openhw.tech.evil/downloads/u.exe','https://openhw.tech/downloads/../secrets','https://openhw.tech/downloads/%2e%2e/secrets','https://openhw.tech/downloads/%5c..%5csecret','https://openhw.tech:444/downloads/u.exe'):
            with self.assertRaises(updater.UpdateError):updater.official_url(url)

    def test_full_installer_release_updates_latest_without_fake_patch(self):
        setup=dict(name='OpenHW-Setup.exe',state='uploaded',browser_download_url=GITHUB+'OpenHW-Setup.exe',size=100,digest='sha256:'+'a'*64)
        value=generate([dict(tag_name='v1.1',assets=[setup,dict(name='OpenHW-Install.json',state='uploaded')])],metadata=lambda a:fresh())
        self.assertEqual(value['latest'],'1.1');self.assertEqual(value['fresh_install'],fresh());self.assertEqual(value['patches'],[])

    def test_future_patch_defaults_to_new_install_branch(self):
        setup=dict(name='OpenHW-Setup.exe',state='uploaded',browser_download_url=GITHUB+'OpenHW-Setup.exe',size=100,digest='sha256:'+'a'*64)
        patch_asset=dict(name='OpenHW-Patch-1.1.1.exe',state='uploaded',browser_download_url=GITHUB.replace('v1.1/','v1.1.1/')+'OpenHW-Patch-1.1.1.exe',size=20,digest='sha256:'+'b'*64)
        value=generate([dict(tag_name='v1.1.1',assets=[patch_asset]),dict(tag_name='v1.1',assets=[setup,dict(name='OpenHW-Install.json',state='uploaded')])],metadata=lambda a:fresh())
        self.assertEqual(value['patches'][0]['from'],['1.1'])
        self.assertEqual(updater.update_plan(value,'1.1')[0]['to'],'1.1.1')

if __name__=='__main__':unittest.main()
