"""Prevent private development/retail files leaking through release packaging."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from release_content_policy import verify_profile,audit_archive


class ReleaseContentPolicyTests(unittest.TestCase):
    def test_raw_texture_is_rejected_even_when_not_in_an_archive(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'Content/r1').mkdir(parents=True)
            (root/'Content/r1/original.lif').write_bytes(b'retail pixels')
            (root/'content-inventory.json').write_text(json.dumps({'schema':2,'files':[]}))
            with self.assertRaisesRegex(ValueError,'Uninventoried'):verify_profile(root)

    def test_renamed_binary_cannot_claim_to_be_authored_source(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'Authored').mkdir();data=b'HOD\0binary'
            (root/'Authored/fake.owshipdef').write_bytes(data)
            (root/'content-inventory.json').write_text(json.dumps({'schema':2,'files':[
                {'path':'Authored/fake.owshipdef','category':'authored-text','sha256':hashlib.sha256(data).hexdigest()}]}))
            with self.assertRaisesRegex(ValueError,'Non-text'):verify_profile(root)

    def test_check_nested_runtime_instead_of_just_zip_filename(self):
        nested=io.BytesIO()
        with zipfile.ZipFile(nested,'w') as archive:archive.writestr('UI/stolen.png',b'pixels')
        outer=io.BytesIO()
        with zipfile.ZipFile(outer,'w') as archive:
            archive.writestr('Development/content-inventory.json',json.dumps({'schema':2,'files':[]}))
            archive.writestr('Development/runtime.zip',nested.getvalue())
        outer.seek(0)
        with self.assertRaisesRegex(ValueError,'nested runtime ZIP'):audit_archive(outer)

    def test_even_approved_art_requires_the_reviewed_bytes(self):
        outer=io.BytesIO()
        with zipfile.ZipFile(outer,'w') as archive:
            archive.writestr('Development/content-inventory.json',json.dumps({'schema':2,'files':[
                {'path':'Content/ui/concept_hangar.png','category':'supplied-concept-art','sha256':'0'*64}]}))
            archive.writestr('Development/Content/ui/concept_hangar.png',b'changed')
        outer.seek(0)
        with self.assertRaisesRegex(ValueError,'approval/hash'):audit_archive(outer)

    def test_clean_runtime_with_no_art_is_accepted(self):
        nested=io.BytesIO()
        with zipfile.ZipFile(nested,'w') as archive:archive.writestr('OpenHW.exe',b'binary')
        outer=io.BytesIO()
        with zipfile.ZipFile(outer,'w') as archive:
            archive.writestr('Development/content-inventory.json',json.dumps({'schema':2,'files':[]}))
            archive.writestr('Development/runtime.zip',nested.getvalue())
        outer.seek(0)
        self.assertTrue(audit_archive(outer)['nestedRuntimeChecked'])


if __name__=='__main__':unittest.main()
