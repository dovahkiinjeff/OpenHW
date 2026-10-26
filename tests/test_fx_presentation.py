import struct
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from openhw_fx_presentation import parse_fx,compile_graph,dependencies,effect_choices,stage_resources,compile_resources,stage_selected

RING='''fx={style="STYLE_RING",properties={
 radius={name="Radius",type="VARTYPE_ARRAY_TIMEFLOAT",value={entry_01={1,8},entry_00={0,2}}},
 depth={name="UseDepthTest",type="VARTYPE_BOOL",value=99}}}'''
class FxGraphTests(unittest.TestCase):
    def test_typed_curves_preserve_size_and_sort_named_entries(self):
        parsed=parse_fx(RING)
        self.assertEqual(parsed['properties'][0]['values'],[0,2,1,8])
        self.assertEqual(parsed['properties'][1]['values'],[99])
    def test_graph_is_not_flattened_and_child_case_is_resolved(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            (root/'flash.lua').write_text(RING)
            (root/'root.lua').write_text('fx={style="STYLE_COMBO",properties={p={name="Fx1",type="VARTYPE_STRING",value="FLASH"}}}')
            blob,receipt=compile_graph(root,['root'])
            self.assertEqual(struct.unpack_from('<8sI',blob),(b'OWFX0001',2))
            self.assertFalse(receipt['flattened']);self.assertFalse(receipt['particle_count_reduction']);self.assertFalse(receipt['scale_adjustments'])
            self.assertEqual(len(receipt['effects']),2)
            (root/'flash.lua').write_text('fx={style="STYLE_COMBO",properties={p={name="Fx1",type="VARTYPE_STRING",value="root"}}}')
            with self.assertRaisesRegex(ValueError,'Cyclic'):compile_graph(root,['root'])
    def test_unknown_fields_types_and_executable_source_fail(self):
        for source in (RING+';os.execute("bad")',RING.replace('VARTYPE_BOOL','UNKNOWN'),
                       RING.replace('value=99','value=0.5'),RING.replace('Radius','../../Radius')):
            with self.assertRaises(ValueError):parse_fx(source)
    def test_frames_are_retained_separately_from_radius(self):
        parsed=parse_fx(RING[:-1]+',frames={timeStart={0,1}}}')
        self.assertEqual(parsed['properties'][-1],dict(name='@frames.timestart',type=8,string='',values=[0,1]))
    def test_missing_dependency_is_not_replaced_by_classic_effect(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'root.lua').write_text('fx={style="STYLE_SPRAY",properties={p={name="Particle_Fx",type="VARTYPE_STRING",value="missing"}}}')
            with self.assertRaises(FileNotFoundError):compile_graph(root,['root'])
    def test_texture_and_mesh_references_keep_original_values(self):
        parsed=parse_fx('fx={style="STYLE_RING",properties={p={name="Mesh",type="VARTYPE_STRING",value="Data:art/fx/box.hod"}}}')
        self.assertEqual(parsed['properties'][0]['string'],'Data:art/fx/box.hod')
        self.assertEqual(list(dependencies(parsed)),[])
    def test_event_choices_retain_weight_not_combo_fanout(self):
        self.assertEqual(effect_choices(',flash,ROOT,flash,'),['flash','root','flash'])
        self.assertEqual(len(effect_choices(','.join(['flash']*40))),32)
    def test_resource_closure_preserves_images_and_atlas_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            source=Path(temporary)/'source';out=Path(temporary)/'staged';fx=source/'art/fx';fx.mkdir(parents=True)
            (fx/'flash.tga').write_bytes(b'original test image bytes')
            (fx/'flash.anim').write_text('fps=10;startFrame=0;loopCount=1;layout={2,2};textureLayout={{"Data:/art/fx/flash.tga",1,0},{"data:art/fx/flash.tga",0,1}}')
            (fx/'flash.lua').write_text('fx={style="STYLE_RING",properties={p={name="Texture",type="VARTYPE_STRING",value="data:art/fx/flash.anim"}}}')
            _,receipt=compile_graph(fx,['flash']);resources=stage_resources(source,out,receipt)
            self.assertEqual((out/'art/fx/flash.tga').read_bytes(),b'original test image bytes')
            blob=compile_resources(source,resources)
            self.assertEqual(struct.unpack_from('<8sII',blob),(b'OWFXRS01',2,3))
            self.assertEqual(struct.unpack_from('<5I',blob,36+2*24)[1:],(1,0,2,2))
            (fx/'flash.lua').write_text('changed source')
            with self.assertRaisesRegex(ValueError,'changed'):stage_resources(source,out,receipt)
    def test_selected_import_packs_native_namespace_and_ignores_unrelated_hw2(self):
        with tempfile.TemporaryDirectory() as temporary:
            source=Path(temporary)/'source';runtime=Path(temporary)/'runtime';out=Path(temporary)/'output'
            fx=source/'art/fx';fx.mkdir(parents=True);runtime.mkdir()
            (fx/'flash.lua').write_text(RING)
            (fx/'unrelated.lua').write_text('not a supported source')
            (runtime/'hull.source.events').write_text('effects={animations={a={name="Fire",length=1}},events={e={{"anim","Fire"},{"fx","flash"}}}}')
            report=stage_selected(source,runtime,out)
            self.assertEqual(report['roots'],['flash']);self.assertEqual(report['nodes'],1)
            self.assertTrue((out/'RemasteredFX/Native/effects.fxbank').is_file())
            self.assertTrue((out/'RemasteredFX/Native/effects.fxresources').is_file())
            self.assertFalse((out/'effects.fxbank').exists())
    def test_hyperspace_edge_images_are_part_of_the_original_resource_closure(self):
        with tempfile.TemporaryDirectory() as temporary:
            source=Path(temporary)/'source';out=Path(temporary)/'output';fx=source/'art/fx';fx.mkdir(parents=True)
            (fx/'edge.tga').write_bytes(b'original edge')
            (fx/'window.lua').write_text('fx={style="STYLE_HYPERSPACE",properties={p={name="EdgeTexture2",type="VARTYPE_STRING",value="data:art/fx/edge.tga"}}}')
            _,receipt=compile_graph(fx,['window']);rows=stage_resources(source,out,receipt)
            self.assertEqual([r['path'] for r in rows],['art/fx/edge.tga'])
if __name__=='__main__':unittest.main()
