"""Source-dependent event changes cannot silently alter the imported baseline."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from release_event_edits import changes,apply,rebuild


class ReleaseEventEditTests(unittest.TestCase):
    def test_nested_edits_remove_and_append_without_mutating_source(self):
        original={'timelines':[{'name':'old'},{'name':'special'}],
                  'events':[{'scale':'1','markers':['a','b']}], 'obsolete':True}
        wanted={'timelines':[{'name':'new'}],
                'events':[{'scale':'0.75','markers':['a','b','c']}]}
        before=copy.deepcopy(original)
        self.assertEqual(apply(original,changes(original,wanted)),wanted)
        self.assertEqual(original,before)

    def test_whole_document_replacement_is_refused(self):
        with self.assertRaisesRegex(ValueError,'Whole-document'):
            apply({},[{'path':[],'value':{'retail':'entire document'}}])

    def test_source_version_mismatch_does_not_publish_event_file(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'Development/SetupSources/Remastered/a.events'
            source.parent.mkdir(parents=True);source.write_bytes(b'different retail version')
            (root/'Development/event-edits.json').write_text(json.dumps({'schema':1,'edits':[
                {'source':'a.events','target':'subsystems/x/events.owevents','source_sha256':'0'*64,
                 'output_sha256':'0'*64,'edits':[],'classic_harvest':False}]}))
            with self.assertRaisesRegex(ValueError,'source version differs'):rebuild(root,root/'generated')
            self.assertFalse((root/'generated/subsystems/x/events.owevents').exists())


if __name__=='__main__':unittest.main()
