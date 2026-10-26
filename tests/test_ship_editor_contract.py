from pathlib import Path
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]

class ShipEditorContract(unittest.TestCase):
    def test_shared_subsystem_preview_uses_the_same_non_degenerate_basis_as_hulls(self):
        source=(ROOT/'src/Game/ShipEditorSubsystem.inc').read_text()
        match=re.search(r'basis\[16\]=\{([^}]+)\}',source)
        values=[float(v) for v in match.group(1).split(',')]
        self.assertEqual(values,[0,0,1,0,1,0,0,0,0,1,0,0,0,0,0,1])
        self.assertIn('meshLoadOpenHWSubsystem(',source)
        self.assertIn('meshValidateOpenHWDefinition(',source)
        self.assertIn('Subsystems/catalog.txt',source)
        self.assertIn('FindFirstFileA',source)
        rays=(ROOT/'src/Modern/ModernRaytracing.cpp').read_text()
        self.assertIn('namedKey.compare(0,11,"subsystems/")==0',rays)

    def test_modding_entry_and_owned_screen_cleanup(self):
        editor = (ROOT / 'src/Game/MilitaryParadeEditor.inc').read_text()
        self.assertIn('SHIP VIEWER / EDITOR / NEW SHIP PROJECT', editor)
        self.assertIn('if(mpe.stage==4){speDraw();return;}', editor)
        self.assertIn('if(mpe.stage==4)return speProcess(ID,event);', editor)
        self.assertIn('{speClose();mpeReleaseCamera();', editor)

    def test_real_models_and_explicit_import_dialogs(self):
        source = (ROOT / 'src/Game/ShipEditor.inc').read_text()
        for feature in ('mpeMesh(', 'speDrawRig(', 'hwAuthoringObjRead(',
                        'GetOpenFileNameA', 'OFN_NOCHANGEDIR', 'cameraMouseDrag',
                        'speParentVector', 'hwModernGraphicsRegisterSurfaceTexture'):
            self.assertIn(feature, source)
        self.assertIn('SAVED PROJECT AND SUBSYSTEM DECLARATIONS / NOT INSTALLED INTO GAMEPLAY', source)
        self.assertIn('if(ID==CKEY)speAction(11)', source)
        self.assertIn('if(ID==VKEY)speAction(12)', source)
        self.assertIn('meshRenderAuthoringPart(spe.mesh,object)', source)

    def test_portable_assets_not_native_archive_writes(self):
        source = (ROOT / 'src/Game/ShipAuthoring.c').read_text()
        self.assertIn('Modding/Ships/%s/%s', source)
        self.assertIn('MOVEFILE_WRITE_THROUGH', source)
        self.assertIn('strstr(s,"..")', source)
        self.assertIn('OPENHW_SHIP 1', source)
        self.assertNotIn('LoadLibrary', source)
        self.assertNotIn('.big', source.lower())

    def test_preview_textures_do_not_depend_on_mission_loading(self):
        source = (ROOT / 'src/Game/ShipEditor.inc').read_text()
        registry = (ROOT / 'src/SDL/texreg.c').read_text()
        preview = registry.split('static bool32 trPreparePreviewIndex', 1)[1].split('void trRegistryRefresh', 1)[0]
        self.assertIn('trPreparePreviewTexture(((trhandle*)m->texture)[0])', source)
        self.assertIn('list.nAllocated=list.nTextures=1;', preview)
        self.assertIn('trMeshSortListLoad(&list);', preview)
        self.assertIn('trPreparePreviewIndex((sdword)reg->sharedFrom,depth+1)', preview)
        self.assertNotIn('HorseRaceBeginBar(', preview)
        self.assertNotIn('trRegistryRefresh();', preview)
        self.assertNotIn('trTextureDeleteAllUnregistered();', preview)
        self.assertIn('glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE)', source)
        self.assertIn('spe.material=spe.obj.vertexCount?spe.obj.vertices[0].material:0;', source)

if __name__ == '__main__':
    unittest.main()
