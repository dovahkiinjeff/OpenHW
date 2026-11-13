"""Disposable synthetic fixtures: bounded packing, recovery and cache ownership."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import openhw_obig as obig
from release_compact import compact_stage_copies,stage_copy
from portable_convert import overlay,_convert,finish_release_archive
from release_import import pack_migration_input


class CompactInstallerDiskTests(unittest.TestCase):
    def setUp(self):
        (ROOT/'tmp').mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=ROOT/'tmp')
        self.addCleanup(self.temp.cleanup)
        self.install=Path(self.temp.name)
        self.work=self.install/'Development/SetupWork'
        self.source=self.work/'generated'
        self.source.mkdir(parents=True)
        self.output=self.install/'OpenHW-HW1.obig'
        self.journal=self.work/'runtime-pack.sqlite'

    def populate(self,count=6,size=1024):
        for i in range(count):
            path=self.source/f'r1/Ship{i:02d}/pixels.dds'
            path.parent.mkdir(parents=True)
            path.write_bytes(bytes([i])*size)
        (self.source/'empty.txt').write_bytes(b'')

    def consume(self,**kwargs):
        return obig.pack_consuming(self.source,self.output,self.journal,scratch=self.work,**kwargs)

    def test_consumed_output_identical_and_independent_of_deleted_scratch(self):
        self.populate()
        reference=self.install/'reference.obig'
        obig.pack(self.source,reference)
        self.assertEqual(self.consume(checkpoint_bytes=1024),7)
        self.assertFalse(any(p.is_file() for p in self.source.rglob('*')))
        self.assertEqual(self.output.read_bytes(),reference.read_bytes())
        shutil.rmtree(self.work)
        self.assertEqual(len(obig.validate(self.output,deep=True)),7)

    def test_resume_after_commit_removes_uncommitted_tail_and_keeps_previous_archive(self):
        self.populate()
        reference=self.install/'reference.obig';obig.pack(self.source,reference)
        self.output.write_bytes(b'previous archive remains until replacement')
        def stop(_):raise RuntimeError('power loss')
        with self.assertRaisesRegex(RuntimeError,'power loss'):
            self.consume(checkpoint_bytes=2048,progress=stop)
        self.assertEqual(self.output.read_bytes(),b'previous archive remains until replacement')
        self.assertFalse((self.source/'r1/Ship00/pixels.dds').exists())
        with self.output.with_suffix('.obig.pending').open('ab') as stream:stream.write(b'incomplete next resource')
        self.consume(checkpoint_bytes=2048)
        self.assertEqual(self.output.read_bytes(),reference.read_bytes())
        self.assertEqual(self.consume(),7)  # rename-before-final-receipt recovery

    def test_resume_after_journal_commit_but_before_source_unlink(self):
        self.populate()
        unlink=Path.unlink
        failed=False
        def interrupt(path,*args,**kwargs):
            nonlocal failed
            if path.suffix=='.dds' and not failed:
                failed=True;raise OSError('interrupted before unlink')
            return unlink(path,*args,**kwargs)
        with patch.object(Path,'unlink',interrupt),self.assertRaisesRegex(OSError,'interrupted'):
            self.consume(checkpoint_bytes=1024)
        self.assertTrue((self.source/'r1/Ship00/pixels.dds').exists())
        self.consume()
        self.assertEqual(len(obig.validate(self.output,deep=True)),7)

    def test_resume_rejects_corruption_of_committed_bytes(self):
        self.populate()
        def stop(_):raise RuntimeError('stop')
        with self.assertRaises(RuntimeError):self.consume(checkpoint_bytes=1024,progress=stop)
        pending=self.output.with_suffix('.obig.pending')
        with pending.open('r+b') as stream:
            stream.seek(obig.HEADER.size+obig.ENTRY.size*7);stream.write(b'corruption')
        with self.assertRaisesRegex(ValueError,'checksum'):self.consume()
        self.assertFalse(self.output.exists())

    def test_resume_rejects_changed_uncommitted_source(self):
        self.populate()
        def stop(_):raise RuntimeError('stop')
        with self.assertRaises(RuntimeError):self.consume(checkpoint_bytes=1024,progress=stop)
        (self.source/'r1/Ship05/pixels.dds').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'uncommitted'):self.consume()

    def test_manifest_cannot_be_reused_for_another_output_or_outside_scratch(self):
        self.populate()
        with self.assertRaisesRegex(ValueError,'contained'):
            obig.pack_consuming(self.source,self.output,self.journal,scratch=self.source)
        self.consume()
        with self.assertRaisesRegex(ValueError,'another installation'):
            obig.pack_consuming(self.source,self.install/'other.obig',self.journal,scratch=self.work)

    def test_native_stages_link_only_immutable_files_inside_opt_in_tree(self):
        cache=self.work/'material-cache';package=self.work/'package-cache'
        cache.mkdir();package.mkdir()
        binary=cache/'surface.dds';binary.write_bytes(b'unchanged texture')
        text=cache/'script.lua';text.write_text('original')
        with compact_stage_copies(self.work):
            stage_copy(binary,package/binary.name)
            stage_copy(text,package/text.name)
            stage_copy(binary,self.install/'outside.dds')
        self.assertTrue(os.path.samefile(binary,package/binary.name))
        self.assertFalse(os.path.samefile(binary,self.install/'outside.dds'))
        self.assertFalse(os.path.samefile(text,package/text.name))
        (package/text.name).write_text('new')
        self.assertEqual(text.read_text(),'original')

    def test_overlay_hardlink_failure_falls_back_without_losing_assets(self):
        self.populate(1)
        target=self.work/'destination'
        with compact_stage_copies(self.work),patch('release_compact.os.link',side_effect=OSError('filesystem has no hardlinks')):
            overlay(self.source,target)
        self.assertEqual((target/'r1/Ship00/pixels.dds').read_bytes(),(self.source/'r1/Ship00/pixels.dds').read_bytes())
        self.assertFalse(os.path.samefile(target/'r1/Ship00/pixels.dds',self.source/'r1/Ship00/pixels.dds'))

    def test_migration_archive_filters_directly_without_creating_a_second_tree(self):
        self.populate(1)
        (self.source/'r1/Ship00/ship.owshipdef').write_text('definition')
        (self.source/'r1/Ship00/hull.hod').write_bytes(b'HOD')
        pack_migration_input(self.source,self.work,self.output)
        self.assertEqual(set(obig.validate(self.output)),{'r1/ship00/ship.owshipdef','r1/ship00/hull.hod'})
        self.assertFalse((self.work/'migration-input').exists())

    def test_prepared_resume_never_reenters_generators_or_source_import(self):
        (self.install/'Development/release.json').write_text('{}')
        prepared=dict(schema=1,definitions=12,sky_size=1024)
        (self.work/'runtime-prepared.json').write_text(json.dumps(prepared))
        with patch('portable_convert.finish_release_archive',return_value={'resources':12}) as finish,patch('release_compact.source_locations',side_effect=AssertionError('sources must not be reread')):
            self.assertEqual(_convert(self.install,self.install/'tools'),{'resources':12})
        finish.assert_called_once_with(self.install,self.install/'tools',self.work,prepared)

    def test_finished_handoff_releases_old_stages_but_preserves_loose_assets_and_receipt(self):
        self.populate(2)
        sources=self.install/'Development/SetupSources'
        for folder in (self.work/'out',self.work/'background-work',self.work/'ship-documents-v2',sources/'Remastered'):
            folder.mkdir(parents=True,exist_ok=True)
            (folder/'old.dds').write_bytes(b'old scratch pixels')
        (sources/'source-receipt.json').write_text('keep source provenance')
        loose=self.install/'Data/Missions/mission01.rtxmap'
        loose.parent.mkdir(parents=True);loose.write_text('exact authored map')
        with patch('portable_convert.subprocess.run') as check:
            result=finish_release_archive(self.install,self.install/'tools',self.work,dict(definitions=9,sky_size=1024))
        self.assertEqual(result,dict(resources=3,definitions=9,sky_size=1024))
        self.assertEqual(loose.read_text(),'exact authored map')
        self.assertEqual((sources/'source-receipt.json').read_text(),'keep source provenance')
        for folder in (self.work/'out',self.work/'background-work',self.work/'ship-documents-v2',sources/'Remastered'):
            self.assertFalse(folder.exists())
        check.assert_called_once()
        self.assertEqual(len(obig.validate(self.output,deep=True)),3)

    def test_pack_peak_is_one_tree_plus_one_batch_not_two_complete_trees(self):
        mib=1024*1024
        self.populate(32,2*mib)
        payload=64*mib
        peak=0
        sync=os.fsync
        def measure(fd):
            nonlocal peak
            # Count physical identities once; hardlinks occupy one allocation.
            unique={}
            for p in self.install.rglob('*'):
                if p.is_file():
                    stat=p.stat();unique[(stat.st_dev,stat.st_ino)]=stat.st_size
            peak=max(peak,sum(unique.values()))
            return sync(fd)
        with patch('openhw_obig.os.fsync',side_effect=measure):self.consume(checkpoint_bytes=8*mib)
        self.assertLess(peak,payload+10*mib)
        self.assertGreaterEqual(peak,payload+8*mib)
        print('Synthetic 64 MiB payload: peak bytes',peak,'vs retained-tree baseline',payload+self.output.stat().st_size)


if __name__=='__main__':unittest.main()
