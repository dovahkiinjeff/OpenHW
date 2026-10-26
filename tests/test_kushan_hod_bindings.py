"""Asset-independent tests for the scoped, non-executing RM dependency planner."""
import sys
import tempfile
import unittest
from unittest.mock import patch
from collections import Counter
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from plan_kushan_hod_bindings import literal_calls, kus_name, resolve_subsystem, R1_HULLS
from plan_kushan_hod_bindings import plan, hierarchy_audit


class KushanBindingsTests(unittest.TestCase):
    def test_hull_identity_scope(self):
        self.assertEqual(len(R1_HULLS), 32)
        self.assertTrue(all(name.startswith('kus_') for name in R1_HULLS.values()))
        self.assertNotIn('Ambassador', R1_HULLS)
        self.assertNotIn('HeadshotAsteroid', R1_HULLS)
        self.assertEqual(R1_HULLS['HeavyInterceptor'], 'kus_interceptor')

    def test_literal_parser_does_not_run_lua(self):
        source = '--LoadSharedModel(NewSubSystemType,"Ignore");\n' \
                 'os.execute("bad");\nLoadSharedModel(NewSubSystemType, "Kus_CruiserGun1");'
        self.assertEqual(literal_calls(source, 'LoadSharedModel', 'NewSubSystemType'), [['Kus_CruiserGun1']])
        for source in ('LoadSharedModel(NewSubSystemType, calculateName());',
                       'LoadSharedModel(NewSubSystemType,"kus_a" "kus_b");',
                       'LoadSharedModel(NewSubSystemType,"kus_a",);'):
            with self.assertRaises(ValueError):
                literal_calls(source, 'LoadSharedModel', 'NewSubSystemType')

    def test_foreign_and_path_names_rejected(self):
        for name in ('tai_carrier', 'cata_acolyte', '../kus_a', 'kus_a/path', 'Kus_A\\path'):
            with self.assertRaises(ValueError):
                kus_name(name)
        self.assertEqual(kus_name('Kus_CruiserGun1'), 'kus_cruisergun1')
        self.assertEqual(kus_name('HW1_ResearchModule'), 'hw1_researchmodule')

    def test_shared_model_is_resolved_without_new_ship_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first, second = root/'subsystem/kus_first', root/'subsystem/kus_second'
            first.mkdir(parents=True); second.mkdir(parents=True)
            (first/'kus_first.subs').write_text('LoadHODFile(NewSubSystemType,1);')
            (first/'kus_first.hod').write_bytes(b'fake local source')
            (second/'kus_second.subs').write_text('LoadSharedModel(NewSubSystemType,"Kus_First");')
            resolved = resolve_subsystem(root, 'Kus_Second')
            self.assertEqual(resolved['model'], 'subsystem/kus_first/kus_first.hod')
            self.assertEqual(resolved['subsystem'], 'kus_second')
            self.assertEqual(resolved['shared_from'], 'kus_first')
            self.assertTrue(resolved['source_visible'])
            (first/'kus_first.subs').write_text('LoadSharedModel(NewSubSystemType,"Kus_Second");')
            with self.assertRaisesRegex(ValueError, 'Cyclic'):
                resolve_subsystem(root, 'Kus_First')

    def test_hidden_source_component_remains_hidden(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); folder=root/'subsystem/hw1_researchmodule'; folder.mkdir(parents=True)
            (folder/'hw1_researchmodule.subs').write_text('NewSubSystemType.visible=0\nLoadHODFile(NewSubSystemType,1);')
            (folder/'hw1_researchmodule.hod').write_bytes(b'fake local source')
            self.assertFalse(resolve_subsystem(root, 'HW1_ResearchModule')['source_visible'])

    def test_repeated_source_weapon_retains_two_visual_instances(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); folder = root/'ship/kus_test'; folder.mkdir(parents=True)
            def declaration(anchor):
                fields = ['Weapon_Turret_Ion1', anchor, 'Weapon', '', '', 'kus_cruiserion'] + ['']*8
                return 'StartShipHardPointConfig(NewShipType,' + ','.join('"'+v+'"' for v in fields) + ');\n'
            script = folder/'kus_test.ship'
            script.write_text(declaration('Ion1') + declaration('Ion2'))
            with patch('plan_kushan_hod_bindings.R1_HULLS', {'HeavyCruiser':'kus_test'}), \
                 patch('plan_kushan_hod_bindings.joint_names', return_value=Counter({'Ion1_Position':1,'Ion2_Position':1})), \
                 patch('plan_kushan_hod_bindings.resolve_subsystem', return_value=dict(model='subsystem/kus_ion/kus_ion.hod', source_visible=True)):
                result = plan(root); attachments = result['ships'][0]['attachments']
                self.assertEqual({a['slot'] for a in attachments}, {'Weapon_Turret_Ion1'})
                self.assertEqual(len({a['visual_instance'] for a in attachments}), 2)
                self.assertEqual(hierarchy_audit(result).count('\nA\t'), 2)
                self.assertFalse(result['ships'][0]['enabled'])
                script.write_text(declaration('Ion1')*2)
                with self.assertRaisesRegex(ValueError, 'Duplicate visual attachment'):
                    plan(root)

    def test_hierarchy_audit_omits_hidden_metadata(self):
        result = dict(ships=[dict(classic_type='ResearchShip', hull='ship/kus_researchship/kus_researchship.hod',
            attachments=[dict(slot='Research', model='subsystem/hw1_researchmodule/hw1_researchmodule.hod', source_visible=False)])])
        self.assertNotIn('hw1_researchmodule', hierarchy_audit(result))


if __name__ == '__main__':
    unittest.main()
