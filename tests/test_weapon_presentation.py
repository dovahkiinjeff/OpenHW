import sys
import unittest
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from openhw_weapon_presentation import parse_weaponfire,SLOTS,weapon_configs,stage_weaponfire,stage_projectiles
from openhw_ship_definitions import author_weapon_cues,weapons


class WeaponPresentationTests(unittest.TestCase):
    def test_normal_activation_disambiguates_shared_special_attack_marker(self):
        source='effects={animations={a={name="Fire",length=1},b={name="Special",length=1}},events={a={{"anim","Fire"},{"marker","Weapon_Gun0_Muzzle"},{"fx","one"}},b={{"anim","Special"},{"marker","Weapon_Gun0_Muzzle"},{"fx","two"}}}}'
        configs={'Hull':[dict(mount='Weapon_Gun0',animation='Fire',activation='Normal'),dict(mount='Weapon_Gun0',animation='Special',activation='Special Attack')]}
        output,report=author_weapon_cues('GUN 0\n{\nDamageLo 40\n}\n',[[0,'Hull','-','-','-','Weapon_Gun0_Muzzle']],{'Hull':source},configs)
        self.assertIn('FireAnimation Fire',output);self.assertEqual(report[0]['status'],'bound')
    def test_weaponfire_can_supply_presentation_without_model_events(self):
        path='@/RemasteredFX/WeaponFire/hdefense.wf'
        configs={'Hull':[dict(mount='Weapon_Gun0',animation='Fire',activation='Normal',weaponfire_path=path)]}
        output,report=author_weapon_cues('GUN 0\n{\nDamageLo 40\n}\n',[[0,'Hull','-','-','-','Weapon_Gun0_Muzzle']],{},configs)
        self.assertIn('WeaponFire '+path,output);self.assertNotIn('FireAnimation',output)
        self.assertEqual(report[0]['status'],'weaponfire only')
    def test_source_weapon_header_import_does_not_import_ballistics(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);folder=root/'weapon/test';folder.mkdir(parents=True)
            (folder/'test.wepn').write_text('StartWeaponConfig(NewWeaponType,"Fixed","Bullet","HDefense","Normal",999999,888888);')
            rows=weapon_configs('StartShipWeaponConfig(NewShipType,"test","Weapon_Gun0","Fire");','ship',root)
            self.assertEqual(rows[0]['weaponfire'],'HDefense');self.assertEqual(rows[0]['activation'],'Normal')
            self.assertNotIn('speed',rows[0]);self.assertNotIn('range',rows[0])
    def test_native_weaponfire_staging_preserves_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source/hdefense';source.mkdir(parents=True)
            raw=b'firefx="flash"\r\nhit_clamp={.5,1.25}\r\n'
            (source/'hdefense.wf').write_bytes(raw)
            self.assertEqual(stage_weaponfire(root/'source',root/'output'),1)
            self.assertEqual((root/'output/RemasteredFX/WeaponFire/hdefense.wf').read_bytes(),raw)
    def test_missile_header_resolves_projectile_not_fabricated_weaponfire(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);weapon=root/'weapon/launcher';weapon.mkdir(parents=True)
            (weapon/'launcher.wepn').write_text('StartWeaponConfig(NewWeaponType,"Fixed","Missile","Kus_Missile","Normal",12345,88888);')
            missile=root/'missile/kus_missile';missile.mkdir(parents=True)
            (missile/'kus_missile.miss').write_text('NewShipType.mainEngineMaxSpeed=999999')
            row=weapon_configs('StartShipWeaponConfig(NewShipType,"launcher","Weapon_Gun0","Fire");','ship',root)[0]
            self.assertIsNone(row['weaponfire_path'])
            self.assertEqual(row['projectile_source'],'missile/kus_missile/kus_missile.miss')
            self.assertNotIn('speed',row)
    def test_projectile_staging_preserves_models_without_executing_simulation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source/missile/test';source.mkdir(parents=True)
            raw=b'NewShipType.mainEngineMaxSpeed=999999\nos.execute("not executed")\r\n'
            (source/'test.miss').write_bytes(raw);(source/'test.hod').write_bytes(b'FORMoriginal')
            (source/'test.events').write_text('effects={animations={a={name="MissileTrail",length=10,loop=1}},events={a={{"anim","MissileTrail"},{"animtime","-1"},{"fx","ignite"}}}}')
            self.assertEqual(stage_projectiles(root/'source',root/'output',{'test'}),1)
            target=root/'output/RemasteredFX/Projectiles/test'
            self.assertEqual((target/'test.source.miss').read_bytes(),raw)
            self.assertEqual((target/'test.hod').read_bytes(),b'FORMoriginal')
            self.assertEqual((target/'events.owevents').read_bytes()[:8],b'OWEVENT2')
            self.assertFalse((target/'test.miss').exists())
    def test_projectile_staging_validates_complete_selection_before_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source/missile/a';source.mkdir(parents=True)
            for extension,data in (('.miss',b'source'),('.hod',b'FORM'),('.events',b'effects={animations={},events={}}')):
                (source/('a'+extension)).write_bytes(data)
            with self.assertRaises(ValueError):stage_projectiles(root/'source',root/'output',{'a','missing'})
            self.assertFalse((root/'output').exists())
            with self.assertRaises(ValueError):stage_projectiles(root/'source',root/'output',{'../escape'})
    def test_weapon_cue_is_authored_once_inside_its_weapon(self):
        events='effects={animations={a={name="Fire",length=2}},events={a={{"anim","Fire"},{"marker","Muzzle"},{"fx","flash"}}}}'
        gameplay='maxhealth 12000\nmaxvelocity 300\nNUMBER_OF_GUNS 1\nGUN 0\n{\nType GUN_Fixed\nDamageLo 40\nFireTime 2.9\n}\n'
        output,report=author_weapon_cues(gameplay,[[0,'Turret','-','-','-','Muzzle']],{'Turret':events})
        settings,blocks=weapons(output)
        self.assertIn('maxhealth 12000',settings);self.assertIn('maxvelocity 300',settings)
        self.assertIn('FireAnimation Fire',blocks[0]);self.assertIn('DamageLo 40',blocks[0]);self.assertIn('FireTime 2.9',blocks[0])
        self.assertEqual(report[0]['animation'],'Fire')
    def test_unresolved_cue_is_reported_not_guessed_from_number(self):
        output,report=author_weapon_cues('GUN 5\n{\nDamageLo 40\n}\n',[[5,'Hull','-','-','-','Other']],{})
        self.assertNotIn('FireAnimation',output);self.assertEqual(report[0]['status'],'missing component events')
    def test_ambiguous_cue_requires_review(self):
        source='effects={animations={a={name="Fire",length=1},b={name="Other",length=1}},events={a={{"anim","Fire"},{"marker","Muzzle"},{"fx","one"}},b={{"anim","Other"},{"marker","Muzzle"},{"fx","two"}}}}'
        output,report=author_weapon_cues('GUN 0\n{\nDamageLo 40\n}\n',[[0,'Hull','-','-','-','Muzzle']],{'Hull':source})
        self.assertNotIn('FireAnimation',output);self.assertIn('Ambiguous',report[0]['status'])
    def test_seven_slots_and_options(self):
        source='\n'.join(s+'fx="'+s+'"\n'+s+'_sfx="SOUND/'+s+'"' for s in SLOTS)
        data=parse_weaponfire(source+'\nhit_clamp={.5,1.25}\nhit_nlips=0\nhit_sm=true')
        self.assertEqual([s['name'] for s in data['slots']],list(SLOTS))
        self.assertEqual(data['slots'][2],dict(name='hit',fx='hit',sound='SOUND/hit',clamp=[.5,1.25],nlips=0,sm=True))
        self.assertEqual(data['diagnostics'],[])
    def test_no_guessed_aliases(self):
        data=parse_weaponfire('nopentrate_sfx="typo" fire_clamp={1,2}')
        self.assertEqual(data['slots'][5]['sound'],'')
        self.assertEqual(len(data['diagnostics']),2)
        self.assertNotIn('clamp',data['slots'][0])
        data=parse_weaponfire('HitFX="not a native field"')
        self.assertEqual(data['slots'][2]['fx'],'');self.assertTrue(data['diagnostics'])
    def test_reject_execution_and_malformed_types(self):
        for source in ('hitfx=os.execute("bad")','hit_sm="on"','hit_clamp={1}',
                       'hit_nlips=true','hitfx=123','hit_clamp={1,"2"}','hit_nlips=2*3','hitfx="\u00e9"'):
            with self.assertRaises(ValueError):parse_weaponfire(source)
    def test_preserves_authored_clamps_without_using_them_as_world_scale(self):
        data=parse_weaponfire('hit_clamp={.5,1.25}')
        self.assertEqual(data['slots'][2]['clamp'],[.5,1.25])
        self.assertNotIn('scale',data['slots'][2])


if __name__=='__main__':unittest.main()
