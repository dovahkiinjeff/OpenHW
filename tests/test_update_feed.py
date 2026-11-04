from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from publish_update_manifest import generate
from openhw_updater import update_plan


def release(version):
    name='OpenHW-Patch-'+version+'.exe'
    return dict(tag_name='v'+version,draft=False,prerelease=False,assets=[dict(name=name,state='uploaded',size=100,
        digest='sha256:'+'a'*64,browser_download_url='https://github.com/dovahkiinjeff/OpenHW/releases/download/v'+version+'/'+name)])


class FeedTests(unittest.TestCase):
    def test_future_releases_automatically_extend_chain(self):
        versions=['1.0.1','1.0.2','1.0.3','1.0.4','1.0.4.5','1.0.5','1.0.6','1.1','2.0']
        feed=generate(list(reversed([release(v) for v in versions])))
        self.assertEqual(feed['latest'],'2.0')
        self.assertEqual([p['to'] for p in update_plan(feed,'1.0')],versions)
        self.assertEqual([p['to'] for p in update_plan(feed,'1.0.5')],versions[6:])
    def test_drafts_betas_and_unverified_assets_excluded(self):
        draft=release('3.0'); draft['draft']=True
        beta=release('4.0'); beta['prerelease']=True
        self.assertEqual(generate([release('1.0.1'),draft,beta])['latest'],'1.0.1')
        bad=release('1.0.2'); bad['assets'][0]['digest']=''
        with self.assertRaises(ValueError): generate([bad])
    def test_cumulative_prerequisites_and_updater_metadata(self):
        future=release('2.0')
        future['assets'] += [dict(name='OpenHW-Update.json',state='uploaded'),dict(name='OpenHW-Updater.json',state='uploaded')]
        own=dict(version='2.0.0',protocol=2,size=100,sha256='b'*64,
            url='https://github.com/dovahkiinjeff/OpenHW/releases/download/v2.0/Updater.exe')
        def meta(asset):
            return own if asset['name']=='OpenHW-Updater.json' else dict(**{'from':['1.0','1.0.1']},minimum_protocol=2,post_actions=[])
        feed=generate([release('1.0.1'),future],metadata=meta)
        self.assertEqual(len(update_plan(feed,'1.0')),1)
        self.assertEqual(feed['updater'],own); self.assertEqual(feed['minimum_protocol'],2)


if __name__=='__main__': unittest.main()
