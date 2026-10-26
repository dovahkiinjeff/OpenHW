"""Freeze the portable setup/update tool with its Python dependencies."""
import argparse
from pathlib import Path
import runpy
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pyinstaller-modules',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parent.parent
    sys.path.insert(0,str(args.pyinstaller_modules.resolve()))
    sys.path.insert(0,str(repo/'tools'))
    modules=['portable_convert','openhw_obig','openhw_ship_definitions','openhw_update','stage_openhw_subsystems',
        'migrate_openhw_ship_documents','audit_openhw_weapon_subsystems',
        'extract_remastered_big','extract_vanilla_ship_textures','classic_mesh_layout',
        'native_hod_surfaces','plan_kushan_hod_bindings','stage_kushan_native_materials',
        'stage_kushan_native_packages','stage_taiidan_native','stage_raiders_derelicts_native',
        'stage_kadeshi_native','audit_classic_kushan_bindings','import_remastered_interceptor',
        'import_local_assets','import_kushan_weapon_fx','import_remastered_audio','openhw_audio_presentation','openhw_presentation_events','openhw_weapon_presentation','inspect_remastered_scene',
        'translate_remastered_scene','translate_remastered_background','translate_remastered_shaders',
        'generate_mission_skies','import_remastered_sky','ship_texture_pipeline','convert_ship_textures_to_dds']
    sys.argv=['PyInstaller','--noconfirm','--clean','--onedir','--console','--name','OpenHW-Setup',
        '--distpath',str(args.output),'--workpath',str(repo/'out/portable-freeze-work'),
        '--specpath',str(repo/'out/portable-freeze-work'),'--paths',str(repo/'tools')]
    for module in modules:sys.argv.extend(['--hidden-import',module])
    sys.argv.append(str(repo/'tools/portable_setup.py'))
    runpy.run_module('PyInstaller',run_name='__main__')


if __name__=='__main__':main()
