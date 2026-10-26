"""Native package scale/path and explicitly reviewed engine marker policies."""
import sys
import unittest
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from stage_kushan_native_packages import calibrated_scale, cache_path, engine_markers, ENGINE_JOINTS
from stage_kushan_native_packages import shared_surface_label
from plan_kushan_hod_bindings import R1_HULLS


class NativePackages(unittest.TestCase):
    def test_shared_models_share_textures_not_component_identity(self):
        labels={}
        self.assertEqual(shared_surface_label(labels,'turret.hod',0,1),'c1_m0')
        self.assertEqual(shared_surface_label(labels,'turret.hod',0,8),'c1_m0')
        self.assertEqual(shared_surface_label(labels,'turret.hod',1,8),'c8_m1')
        self.assertEqual(shared_surface_label(labels,'other.hod',0,9),'c9_m0')
        self.assertEqual(len(labels),3)

    def test_per_hull_scale_uses_same_longest_classic_axis(self):
        old=(np.array([-2,-3,-10]),np.array([2,3,10]))
        new=(np.array([-5,-1,-2]),np.array([5,1,2]))
        self.assertEqual(calibrated_scale(old,new),(5.,2))
        with self.assertRaises(ValueError):
            calibrated_scale(old,(np.zeros(3),np.zeros(3)))

    def test_paths_cannot_escape_cache(self):
        root=Path(__file__).parent.resolve()
        self.assertEqual(cache_path(root,'fixture.hod'),root/'fixture.hod')
        with self.assertRaises(ValueError):
            cache_path(root,'../outside.hod')

    def test_engine_binding_is_exact_not_fuzzy(self):
        joints=[{'name':'EngineNozzle1'},{'name':'EngineNozzle2'}]
        self.assertEqual(engine_markers('ResourceCollector',joints),
                         [('engine_0','EngineNozzle1'),('engine_1','EngineNozzle2')])
        for invalid in ([],[{'name':'EngineNozzle10'}],joints+[{'name':'EngineNozzle1'}]):
            with self.assertRaises(ValueError):
                engine_markers('ResourceCollector',invalid)

    def test_no_cross_race_or_invented_mothership_trails(self):
        self.assertTrue(set(ENGINE_JOINTS)<=set(R1_HULLS))
        self.assertEqual(engine_markers('Mothership',[{'name':'EngineNozzle1'}]),[])
        self.assertEqual(engine_markers('P1Mothership',[]),[])


if __name__=='__main__':
    unittest.main()
