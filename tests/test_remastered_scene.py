"""Asset-independent tests for foreground strides, hierarchy and material data."""
import sys
import struct
import unittest
import tempfile
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from translate_remastered_scene import vertices_from,poses,surface_layers,decode_texture,campaign_material


class SceneTests(unittest.TestCase):
    def test_mission_three_uses_completed_burned_surface_without_mutating_source(self):
        material=dict(name='KharakBurn',shader='bg_planetmelt',parameters=[dict(name='$scorched',type=5,value_hex='02000000')])
        burned=campaign_material(material,3)
        self.assertEqual(burned['shader'],'bg_planetmelted')
        self.assertIs(burned['parameters'],material['parameters'])
        self.assertEqual(material['shader'],'bg_planetmelt')
        self.assertIs(campaign_material(material,1),material)
        other={**material,'name':'OtherPlanet'}
        self.assertIs(campaign_material(other,3),other)

    def test_tangent_stream_follows_uv_not_integer_bit_order(self):
        source=np.array([[1,2,3,1,0,0,1,0,.2,.8,1,0,0,0,1,0]],'<f4')
        part=dict(vertices=1,stride=64,mask=0x600b,vertex_pool_offset=0,face_pool_offset=0,triangles=1)
        v=vertices_from(part,source.tobytes(),struct.pack('<3H',0,0,0))
        np.testing.assert_allclose(v[0,16:18],[.2,.8])
        np.testing.assert_allclose(v[0,6:9],[1,0,0])
        np.testing.assert_allclose(v[0,9:12],[0,1,0])
        np.testing.assert_allclose(v[0,12:16],[1,1,1,1])

    def test_hierarchy_and_source_rotation_are_not_camera_dependent(self):
        info=dict(joints=[dict(name='Root',parent='',position=[10,0,0],rotation=[0,0,0]),
                          dict(name='Planet',parent='Root',position=[0,2,0],rotation=[0,0,0])])
        curves={'Planet_rotateY':np.array([[0,0,1,0,1,0],[1,2*np.pi,1,0,1,0]])}
        matrices,cycle=poses(info,curves,.01)
        self.assertAlmostEqual(cycle,100)
        np.testing.assert_allclose(matrices['Planet'][0,:3,3],[10,2,0])
        np.testing.assert_allclose(matrices['Planet'][0],matrices['Planet'][-1],atol=1e-6)
        np.testing.assert_allclose(matrices['Planet'][128,:3,:3],np.diag([-1,1,-1]),atol=1e-6)

    def test_source_aliases_override_program_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'gl_surf').mkdir()
            (root/'gl_surf/test.surf').write_text('local float4 BG_Test 1 2 3 4\nlayer\n program bg_cosmic_a\n blendOps srcAlpha srcInvAlpha\n cull none\n alias BG_Test inTest\n')
            material=dict(shader='test',parameters=[dict(name='Test',type=4,value_hex=struct.pack('<4f',5,6,7,8).hex())])
            name,aliases,defaults,flags=next(surface_layers(root,material))
            self.assertEqual(flags,1);self.assertEqual(aliases['inTest'],'BG_Test')
            self.assertEqual(defaults['BG_Test'],[5,6,7,8])

    def test_rgba_does_not_discard_opacity_or_normal_channels(self):
        source=bytes([128,129,255,17])
        row=dict(mips=[[1,1]],format='8888',pool_offset=0,bytes=4)
        self.assertEqual(decode_texture(row,source),source)


if __name__=='__main__':unittest.main()
