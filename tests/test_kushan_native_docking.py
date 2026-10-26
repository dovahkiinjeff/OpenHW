import math
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audit_kushan_native_docking import compare, hw_position


class DockAuditTests(unittest.TestCase):
    def test_basis_scale_once(self):
        self.assertEqual(hw_position([1,2,3],4),[8,-4,12])
        for scale in (0,-1,math.inf):
            with self.assertRaises(ValueError):
                hw_position([1,2,3],scale)

    def test_nearest_is_not_a_binding(self):
        points=[dict(position=[1,2,3])]
        paths=[dict(name='FighterIn',parent='world',points=points)]
        markers=[dict(name='DOCK0',semantic_name='IN',position=[8,-4,12])]
        result=compare(paths,markers,4)[0]
        self.assertEqual(result['status'],'unbound-audit-only')
        self.assertEqual(result['samples'][0]['nearest_classic'][0]['distance'],0)
        self.assertEqual(points[0]['position'],[1,2,3])
        self.assertNotIn('binding',result)

    def test_unknown_parent_not_guessed(self):
        result=compare([dict(name='Door',parent='MovingDoor',points=[])],[],1)[0]
        self.assertEqual(result['status'],'parent-transform-review-required')
        self.assertNotIn('samples',result)

    def test_absent_classic_points(self):
        result=compare([dict(name='In',parent='world',points=[dict(position=[0,0,0])])],[],1)[0]
        self.assertEqual(result['samples'][0]['nearest_classic'],[])


if __name__=='__main__':
    unittest.main()
