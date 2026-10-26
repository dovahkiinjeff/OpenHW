import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from portable_convert import conversion_steps,NATIVE_PRESENTATION_RECIPE

class ConversionResumeTests(unittest.TestCase):
    def test_earlier_checkpoint_keeps_expensive_unchanged_source_work(self):
        upstream={'classic','canonical','payload','r1-plan','fx'}
        stale={'r1-material','r1-package','r2','p1','p2','native-overlay','generated','subsystems'}
        self.assertEqual(conversion_steps(dict(completed=sorted(upstream|stale))),upstream)
    def test_current_checkpoint_resumes_completed_native_steps(self):
        steps={'classic','r1-package','r2','generated'}
        self.assertEqual(conversion_steps(dict(completed=sorted(steps),
            native_presentation_recipe=NATIVE_PRESENTATION_RECIPE)),steps)
    def test_newer_or_invalid_checkpoint_does_not_silently_downgrade(self):
        for recipe in (NATIVE_PRESENTATION_RECIPE+1,-1,True,'1'):
            with self.assertRaises(ValueError):conversion_steps(dict(completed=[],native_presentation_recipe=recipe))

if __name__=='__main__':unittest.main()
