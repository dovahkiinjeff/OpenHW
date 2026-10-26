"""Freeze a windowed installer whose dependencies remain beside it, never Temp."""
import argparse,runpy,sys,shutil,importlib.metadata
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pyinstaller-modules',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();repo=Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(a.pyinstaller_modules.resolve()));sys.path.insert(0,str(repo/'tools'))
    modules=['release_compact','portable_setup','portable_convert','release_import','release_content_policy','release_local_art','release_event_edits','release_mission_authoring','openhw_obig','openhw_ship_definitions','openhw_update',
      'stage_openhw_subsystems','migrate_openhw_ship_documents','audit_openhw_weapon_subsystems',
      'extract_remastered_big','extract_vanilla_ship_textures','classic_mesh_layout','native_hod_surfaces',
      'plan_kushan_hod_bindings','stage_kushan_native_materials','stage_kushan_native_packages',
      'stage_taiidan_native','stage_raiders_derelicts_native','stage_kadeshi_native',
      'stage_bentusi_native','stage_ghostship_native','stage_research_station_native','stage_junkyardhq_native','stage_miningbase_native','stage_council_native',
      'audit_classic_kushan_bindings','import_remastered_interceptor','import_local_assets','import_kushan_weapon_fx',
      'import_remastered_audio','openhw_audio_presentation','openhw_presentation_events','openhw_weapon_presentation',
      'openhw_fx_presentation','inspect_remastered_scene','translate_remastered_scene','translate_remastered_background',
      'translate_remastered_shaders','generate_mission_skies','import_remastered_sky','ship_texture_pipeline','convert_ship_textures_to_dds']
    sys.argv=['PyInstaller','--noconfirm','--clean','--onedir','--windowed','--contents-directory','Development','--noupx',
      '--name','OpenHW-Setup','--distpath',str(a.output),'--workpath',str(repo/'out/release-freeze-work'),
      '--specpath',str(repo/'out/release-freeze-work'),'--paths',str(repo/'tools')]
    modules+=['release_patch','release_sky_lighting']
    sys.argv+=['--add-data',str(repo/'tools/sky_materials/btg_microstructure.png')+';sky_materials']
    for module in modules:sys.argv+=['--hidden-import',module]
    sys.argv+=[str(repo/'tools/release_setup.py')];runpy.run_module('PyInstaller',run_name='__main__')
    notices=a.output/'OpenHW-Setup/ThirdPartyNotices';notices.mkdir(parents=True,exist_ok=True)
    python_license=Path(sys.base_prefix)/'LICENSE.txt'
    if python_license.is_file():shutil.copyfile(python_license,notices/'Python-LICENSE.txt')
    for name in ('numpy','pillow'):
        distribution=importlib.metadata.distribution(name)
        for relative in distribution.files:
            if 'license' not in str(relative).lower() and 'copying' not in str(relative).lower():continue
            source=distribution.locate_file(relative)
            if not source.is_file():continue
            target=notices/name/str(relative);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
    import PyInstaller
    shutil.copyfile(a.pyinstaller_modules/f'pyinstaller-{PyInstaller.__version__}.dist-info/licenses/COPYING.txt',notices/'PyInstaller-COPYING.txt')
if __name__=='__main__':main()
