"""Keep native runtime handles out of gameplay identities and save files."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]


class NativeVisualLifecycle(unittest.TestCase):
    def test_saved_animation_payload_grows_before_rebinding_native_hierarchy(self):
        source=(ROOT/'src/Game/SaveGame.c').read_text()
        load=source.split('Ship *LoadShip(',1)[1].split('void FixShip(',1)[0]
        self.assertIn('requiredMadSize > ship->madBindings->totalSize',load)
        self.assertIn('memAlloc(madOffset + requiredMadSize',load)
        self.assertIn('memcpy(expanded, ship, savedRecordSize)',load)
        self.assertIn('ship->madBindings->totalSize = requiredMadSize',load)
        self.assertLess(load.index('ship = expanded'),load.index('univMeshBindingsDupe(shipstaticinfo, ship)'))
        copy=source.split('void CopyPointersFromRealShipStaticInfo',1)[1].split('void SaveClampInfo',1)[0]
        self.assertIn('dynamicstaticinfo->hierarchySize = realstaticinfo->hierarchySize',copy)

    def test_original_paged_paths_activate_native_capital_ships(self):
        mesh=(ROOT/'src/Game/Mesh.c').read_text()
        loader=mesh.split('static meshdata *meshTryNativeFleetPackage',1)[1].split('meshdata *meshLoad',1)[0]
        self.assertIn('%s\\\\%s\\\\Rl0\\\\LOD%u\\\\%s.geo',loader)
        self.assertIn('%s\\\\%s\\\\Rl0\\\\LOD%u\\\\%s.peo',loader)
        self.assertIn('const char *end,*root="R1"',loader)
        self.assertIn('Derelicts\\\\',loader)
        self.assertIn('hwRmNativeDerelictType(type)',loader)
        self.assertIn('hwRmNativeP1Type(type)',loader)
        self.assertIn('derelict?"Derelict_":!strcmp(root,"R2")?"Taiidan_":""',loader)
        self.assertIn('strcasecmp(original,canonical)',loader)
        self.assertIn('if (lod==5) return NULL',loader)

    def test_mesh_ownership_is_released(self):
        mesh=(ROOT/'src/Game/Mesh.c').read_text()
        self.assertIn('meshNativeVisualRegister(mesh, asset',mesh)
        self.assertIn('meshNativeVisualReleaseMesh(mesh)',mesh.split('void meshFree',1)[1])

    def test_ship_and_load_paths_reconstruct_runtime_instances(self):
        update=(ROOT/'src/Game/UnivUpdate.c').read_text()
        self.assertIn('meshNativeVisualAcquire(lodInfo->level[index].pData, newShip)',update)
        deletion=update.split('void univMeshBindingsDelete',1)[1].split('void InitializeEngineTrails',1)[0]
        self.assertLess(deletion.index('meshNativeVisualReleaseShip(ship)'),deletion.index('ship->bindings == NULL'))
        save=(ROOT/'src/Game/SaveGame.c').read_text()
        self.assertIn('univMeshBindingsDupe(shipstaticinfo, ship)',save)

    def test_old_clock_drives_native_pose(self):
        animation=(ROOT/'src/Game/MeshAnim.c').read_text()
        self.assertIn('meshNativeVisualSampleShipEvent(ship,event->name,phase)',animation)
        self.assertIn('meshNativeVisualSampleShipEvent(ship,animation->name,1)',animation)
        self.assertIn('else madNativeVisualSample(ship)',animation)
        renderer=(ROOT/'src/SDL/render.c').read_text()
        self.assertIn('meshPrepareRemasteredShipPose(spaceobj',renderer)

    def test_no_native_handles_added_to_save_identity(self):
        for name in ('SpaceObj.h','SaveGame.c','SaveGame.h'):
            source=(ROOT/'src/Game'/name).read_text()
            self.assertNotIn('HwRmVisualAsset',source)
            self.assertNotIn('HwRmVisualInstance',source)

    def test_native_models_preserve_classic_combat_rest_frames(self):
        universe=(ROOT/'src/Game/Universe.c').read_text()
        bind=universe.split('static void setGunBindInfo(char *directory,char *field,void *dataToFillIn)\n{',1)[1].split('static void setAnimationBindInfo',1)[0]
        self.assertIn('classicGunRestPose(bytes',bind)
        self.assertIn('gun->defaultgunorientation',bind)
        self.assertIn('gun->defaultGunOrientationNonConcat',bind)
        update=(ROOT/'src/Game/UnivUpdate.c').read_text().split('shipbindings *univMeshBindingsDupe',1)[1].split('void univMeshBindingsDelete',1)[0]
        self.assertIn('gunNewGimbleUpdateCoordSys(gun,gun->gunstatic)',update)
        self.assertNotIn('gun->angle=',update)

    def test_native_meshes_do_not_reapply_classic_pivot_curves(self):
        update=(ROOT/'src/Game/UnivUpdate.c').read_text()
        self.assertIn('if (!nativeMesh && bindingListSource->function != NULL)',update)
        animation=(ROOT/'src/Game/MeshAnim.c').read_text()
        callback=animation.split('bool32 madBindingListCallback',1)[1].split('void madAnimBindingsDupe',1)[0]
        self.assertIn('meshNativeVisualParameters(mesh',callback)
        self.assertLess(callback.index('meshNativeVisualParameters(mesh'),callback.index('madDupeHeader->nObjects'))
        mesh=(ROOT/'src/Game/Mesh.c').read_text()
        prepare=mesh.split('static bool32 meshSyncRemasteredGuns',1)[1].split('static meshdata *meshTryNativeInterceptorHod',1)[0]
        self.assertIn('hwRmVisualInstanceAimGuns(instance,controls,gunCount',prepare)
        self.assertIn('gunRecoilVectorCompute',prepare)

    def test_projectile_origin_uses_pose_without_camera_lod_or_double_barrel(self):
        mesh=(ROOT/'src/Game/Mesh.c').read_text()
        query=mesh.split('bool32 meshRemasteredGunMuzzle',1)[1].split('bool32 meshPrepareRemasteredShipPose',1)[0]
        self.assertIn('level=0',query)
        self.assertIn('meshSyncRemasteredGuns(owner,instance)',query)
        self.assertIn('hwRmVisualInstanceGunMuzzle(instance,gun,scale',query)
        self.assertNotIn('currentLOD',query)
        gun=(ROOT/'src/Game/Gun.c').read_text()
        self.assertLess(gun.index('bullet->posinfo.position.z += gunheadingInWorldCoordSys.z * gunstatic->barrelLength'),
                        gun.index('gunNativeProjectilePosition(ship,gun,&bullet->posinfo.position)'))
        self.assertIn('meshRemasteredGunMuzzle(ship',gun)
        self.assertIn('gunNativeProjectilePosition(ship,gun,&missile->posinfo.position)',gun)

    def test_engine_trails_use_native_marker_with_classic_fallback(self):
        trails=(ROOT/'src/Game/Trails.c').read_text()
        nozzle=trails.split('void trailGetNozzleOffset',1)[1].split('void trailGetCoordsys',1)[0]
        self.assertIn('meshRemasteredMarkerPosition(ship,semantic',nozzle)
        self.assertIn('engineNozzleOffset[trail->trailNum]',nozzle)
        self.assertLess(nozzle.index('meshRemasteredMarkerPosition'),nozzle.index('engineNozzleOffset['))

    def test_native_door_does_not_use_missing_classic_geometry_index(self):
        source=(ROOT/'src/Game/MadLinkIn.c').read_text()
        door=source.split('bool32 madLinkInGetDoorInfo',1)[1].split('void madLinkInOpenSpecialShip',1)[0]
        self.assertIn('meshRemasteredMarkerTransform(ship,"Hangardoor"',door)
        self.assertLess(door.index('meshRemasteredMarkerTransform'),door.index('madGunBindingIndexFindByName'))


if __name__=='__main__':
    unittest.main()
