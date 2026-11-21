import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import repair_installed_reflections as repair

class ReflectionRepair(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)
        (self.root/"setup-complete.json").write_text("{}")
        self.key="r1/test/rmnative/c0_m0_material.dds"
        original=b"original official page"
        header=struct.pack("<8sIIQ",b"OBIGPK01",1,1,296)
        entry=struct.pack("<256sQII",self.key.encode(),296,len(original),zlib.crc32(original))
        (self.root/"OpenHW-HW1.obig").write_bytes(header+entry+original)
        self.reader=patch.object(repair,"load_reader",return_value=None)
        self.reader.start()
    def tearDown(self):
        self.reader.stop()
        self.temp.cleanup()
    def test_installs_each_map_without_staging_the_entire_set(self):
        def stage(archive,output,reader,writer):
            writer(self.key,b"upgraded")
            self.assertEqual((self.root/self.key).read_bytes(),b"upgraded")
            self.assertFalse(list(output.rglob("*_material.dds")))
        with patch.object(repair,"stage",side_effect=stage):
            repair.repair(self.root)
        receipt=json.loads((self.root/"Development/Patches/1.1.14/material-reflections.json").read_text())
        self.assertEqual(receipt["updated"],[self.key])
    def test_preserves_authored_override(self):
        target=self.root/self.key
        target.parent.mkdir(parents=True)
        target.write_bytes(b"player custom material")
        with patch.object(repair,"stage",side_effect=lambda a,o,r,writer:writer(self.key,b"upgraded")):
            repair.repair(self.root)
        self.assertEqual(target.read_bytes(),b"player custom material")
    def test_failure_restores_old_page_and_removes_new_page(self):
        target=self.root/self.key
        target.parent.mkdir(parents=True)
        target.write_bytes(b"original official page")
        new="r1/test/rmnative/c0_m1_material.dds"
        def stage(archive,output,reader,writer):
            writer(self.key,b"upgraded")
            writer(new,b"new")
            raise RuntimeError("interrupted")
        with patch.object(repair,"stage",side_effect=stage):
            with self.assertRaisesRegex(RuntimeError,"interrupted"):
                repair.repair(self.root)
        self.assertEqual(target.read_bytes(),b"original official page")
        self.assertFalse((self.root/new).exists())
if __name__=="__main__":
    unittest.main()
