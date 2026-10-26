import sys
import unittest
import tempfile
from PIL import Image
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from import_kushan_weapon_fx import read_table,properties,curve,muzzle_events,Translator,spray_schedule

class RemasteredWeaponFx(unittest.TestCase):
    def test_ship_engines_use_pre_rm_presentation(self):
        trails=(ROOT/'src/Game/Trails.c').read_text()
        self.assertNotIn('rmFxDrawEngine(',trails)
        self.assertIn('trailDrawCapitalGlow(trail, LOD)',trails)
        self.assertIn('rad * 2.25f, haloColor',trails)
        self.assertIn('rmFxDrawMissileTrail(', (ROOT/'src/SDL/render.c').read_text())
    def test_event_delay_preserved(self):
        text='wpn={{"anim","Weapon_Gun0"},{"animtime","0.1"},{"fx","steam"}}'
        self.assertEqual(muzzle_events(text,timed=True)[0],[('steam',1,.1)])
    def test_event_phase_uses_its_timeline_duration(self):
        text='effects={animations={a={name="Weapon_Gun0",length=5}},events={wpn={{"anim","Weapon_Gun0"},{"animtime","0.1"},{"fx","steam"}}}}'
        self.assertEqual(muzzle_events(text,timed=True)[0],[('steam',1,.5)])

    def test_spray_size_speed_and_position_are_independent(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); fx=root/'art/fx'; fx.mkdir(parents=True)
            Image.new('RGBA',(2,2),'white').save(fx/'dot.tga')
            (fx/'child.lua').write_text('fx={style="STYLE_RING",properties={p={name="Texture",value="art/fx/dot.tga"},r={name="Radius",value=1}}}')
            # Not smoke: birth-time size, speed and offset curves must all work.
            template='fx={style="STYLE_SPRAY",properties={a={name="Particle_Fx",value="child"},b={name="Emitter_Duration",value=1},c={name="Emitter_Rate",value=2},d={name="Particle_Scale",value={0,2,1,4}},e={name="Particle_Speed",value={0,10,1,20}},f={name="Emitter_Offset",value={0,0,0,4,1,0,0,8}},g={name="Emitter_ScaleSpeed",value=SPEED}}}'
            for scalespeed in (0,1):
                (fx/'parent.lua').write_text(template.replace('SPEED',str(scalespeed)))
                t=Translator(root); layers=t.flatten('parent',scale=3)
                self.assertEqual(len(layers),2)
                for layer,birth in zip(layers,(.25,.75)):
                    size=2+2*birth
                    self.assertAlmostEqual(layer[10],3*size)
                    self.assertAlmostEqual(layer[13]*layer[10],(10+10*birth)*(3 if scalespeed else 1))
                    offset=t.keys[layer[21]][3]
                    self.assertAlmostEqual(offset*layer[10],3*(4+4*birth))

    def test_all_sprays_integrate_rate_curve(self):
        self.assertEqual(len(spray_schedule([[0,0],[1,4]],1)),2)
    def test_standard_corvette_preserves_declination_only_classic_binding(self):
        source=(ROOT/'src/Game/NativeVisualEvents.c').read_text()
        self.assertIn('HINGE_GUN("P1StandardCorvette",0,"Weapon_Gun0")',source)
        self.assertIn('HINGE_GUN("P1StandardCorvette",1,"Weapon_Gun1")',source)
        self.assertNotIn('ATTACHED_GUN("P1StandardCorvette"',source)
    def test_compact_missile_impact_uses_no_capital_spray(self):
        source=ROOT/'out/hwrm-weapon-fx/source'
        if not source.exists(): self.skipTest('Local RM source unavailable')
        translator=Translator(source)
        effect=translator.corvette_missile_impact()
        self.assertNotEqual(effect,0xffffffff)
        first,count,lifetime=translator.effects[effect]
        self.assertLessEqual(count,24)
        self.assertLess(lifetime,1.5)
        self.assertTrue(all(abs(x[10]-2)<1e-6 for x in translator.layers[first:first+count]))
        self.assertFalse(any('panel' in x[0] or 'spike' in x[0] for x in translator.textures))
    def test_steam_ignition_curve_is_not_sampled_only_at_zero(self):
        births=spray_schedule([[0,0],[.0225,11.2],[.10625,0],[1,0]],4)
        self.assertEqual(len(births),3)
        self.assertTrue(all(0<t<.425 for t in births))
        self.assertEqual(spray_schedule([[0,0],[1,0]],4),[])
    def test_rm_atlas_row_column_order(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); fx=root/'art/fx'; fx.mkdir(parents=True)
            im=Image.new('RGBA',(8,6))
            # Three rows, four columns, each a distinct 2x2 cell.
            for row in range(3):
                for col in range(4):
                    im.paste((row*60,col*60,10,255),(col*2,row*2,col*2+2,row*2+2))
            im.save(fx/'sheet.tga')
            (fx/'test.anim').write_text('fps=30;layout={3,4};textureLayout={{"data:/art/fx/sheet.tga",0,3},{"data:/art/fx/sheet.tga",2,1}}')
            t=Translator(root); t.bitmap('art/fx/test.anim')
            _,w,h,data=t.textures[0]; result=Image.frombytes('RGBA',(w,h),data)
            self.assertEqual(result.getpixel((0,0)),(0,180,10,255))
            self.assertEqual(result.getpixel((2,0)),(120,60,10,255))
    def test_data_only_no_lua_execution(self):
        with self.assertRaises(ValueError): read_table('fx = os.execute("anything")')
        with self.assertRaises(ValueError): read_table('fx = {}; doSomething()')
    def test_out_of_order_source_curve_sorted(self):
        p=properties(read_table('fx={properties={p={name="Colour",value={entry_01={1,0,0,0,1},entry_00={0,1,1,1,1}}}}}'))
        self.assertEqual(curve(p,'colour',[1,1,1,1],4)[0],[0,1,1,1,1])
    def test_event_trailing_commas(self):
        m=muzzle_events('wpn={{"anim","Weapon_Gun0",},{"fx","muzzle_flash_combo",},{"fx_scale","0.5",}}')
        self.assertEqual(m[0],[('muzzle_flash_combo',.5)])
    def test_scope_and_lifetime(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('ship->shiprace!=R1 && ship->shiprace!=R2 && ship->shiprace!=P1 && ship->shiprace!=P2',source)
        self.assertIn('classic hull keeps classic FX',source)
        self.assertIn('glDepthMask(GL_FALSE)',source)
        self.assertIn('rmFxValidate(package',source)
        self.assertIn('rmWeaponFxReset();',(ROOT/'src/Game/ETG.c').read_text())
        self.assertIn('rmWeaponFxShutdown();',(ROOT/'src/Game/ETG.c').read_text())
    def test_no_save_layout_changes(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertNotIn('Bullet *bullet;',source)
        self.assertIn('static RmFxEvent events[RMFX_EVENTS]',source)
    def test_world_fx_state_and_atlas_isolation(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('glDisable(GL_FOG)',source)
        self.assertIn('glDisable(GL_SCISSOR_TEST)',source)
        self.assertIn('muzzle && (l->blend & RMFX_NO_DEPTH_TEST)',source)
        self.assertIn('u0+=du; u1-=du; v0+=dv; v1-=dv;',source)
    def test_diagnostics_are_opt_in_and_targeted(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('rmWeaponFxDiagnosticsEnabled=FALSE',source)
        self.assertIn('ship->shiptype==HeavyInterceptor',source)
        self.assertIn('diagnosticRows>=100000',source)
        self.assertIn('zero_live_heading',source)
        self.assertIn('positive_layers_submitted',source)
        self.assertIn('light_radius,light_intensity,gun_count,gun_angle,gun_declination,owner_visual_scale',source)
        self.assertIn('native_binding_failed',source)
        self.assertIn('rmWeaponFxDiagnosticLight(ship,gun,position,direction,emitter.radius,emitter.intensity)',
                      (ROOT/'src/Game/Gun.c').read_text())
        self.assertIn('entryVr("/muzzleFxLog"',(ROOT/'src/SDL/main.c').read_text())
    def test_native_pose_does_not_read_unused_legacy_gun_angles(self):
        source=(ROOT/'src/Game/Mesh.c').read_text()
        self.assertIn('nativeGunTrackingHeading(gun,controls[ordinal].heading)',source)
        self.assertIn('hwRmVisualInstanceAimGuns(instance,controls,gunCount',source)
        self.assertNotIn('controls[ordinal].azimuth=gun->angle',source)
        policy=(ROOT/'src/Game/NativeGunPosePolicy.h').read_text()
        self.assertIn('heading[0]=gun->curgunorientation.m13',policy)
        self.assertIn('guntype==GUN_NewGimble',policy)
    def test_weapon_spray_is_detached_but_flash_follows_live_muzzle(self):
        source=(ROOT/'src/Game/RemasteredWeaponFX.c').read_text()
        self.assertIn('layerPosition=emission->position',source)
        self.assertIn('RMFX_INHERIT_VELOCITY',source)
        self.assertIn('events[slot-1].velocity=ship->posinfo.velocity',source)
        translator=(ROOT/'tools/import_kushan_weapon_fx.py').read_text()
        self.assertIn("p.get('emitter_inheritvelocity',0)",translator)
        self.assertIn('P1_HULLS.items()',translator)

if __name__=='__main__': unittest.main()
