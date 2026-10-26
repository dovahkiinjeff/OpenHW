"""Prepare an uninstalled, relocatable 1.0 installer from the reviewed WIP."""
import argparse,hashlib,json,re,shutil,zipfile
from pathlib import Path

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def defaults(text):
    # A preset config exists on first launch, so request display detection
    # explicitly. No developer machine paths or graphics-adapter IDs survive.
    omitted={'homeworlddatapath','screenwidth','screenheight','screendepth','refreshrate',
             'devicecrc','deviceindex','playerpassword','detectdisplayonfirstboot'}
    lines=[line for line in text.splitlines() if not line.split() or line.split()[0].lower() not in omitted]
    return '\n'.join(lines).rstrip()+'\n\ndetectDisplayOnFirstBoot    1\n'

def stage(repo, profile, destination, normal_build=None, diagnostic_build=None, tools_dir=None):
    from release_content_policy import verify_profile
    repo,profile,destination=map(lambda p:Path(p).resolve(),(repo,profile,destination))
    inventory=verify_profile(profile)
    if destination.exists() and any(destination.iterdir()):raise ValueError('Release staging directory must be empty')
    if destination==profile or destination.is_relative_to(profile):raise ValueError('Never stage inside release input')
    destination.mkdir(parents=True,exist_ok=True);dev=destination/'Development';dev.mkdir()
    for category in ('Content','Authored'):
        for row in inventory['files']:
            if not row['path'].startswith(category+'/'):continue
            target=dev/row['path'];target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(profile/row['path'],target)
    shutil.copyfile(profile/'content-inventory.json',dev/'content-inventory.json')
    shutil.copyfile(profile/'event-edits.json',dev/'event-edits.json')
    shutil.copyfile(profile/'mission-authoring.json',dev/'mission-authoring.json')
    build=repo/'out/build'
    normal=Path(normal_build) if normal_build else build/'release-no-diagnostics/Release'
    diag=Path(diagnostic_build) if diagnostic_build else build/'deploy-x64-clean/Release'
    imports=Path(tools_dir) if tools_dir else repo/'out/import-tools'
    runtime=dev/'Runtime';runtime.mkdir()
    shutil.copyfile(normal/'HomeworldModern.exe',runtime/'OpenHW.exe')
    shutil.copyfile(diag/'HomeworldModern.exe',runtime/'OpenHW-Daignostics.exe')
    libraries=('SDL2.dll','avcodec-63.dll','avformat-63.dll','avutil-61.dll','swscale-10.dll',
       'DirectXTex.dll','libxess.dll','nvngx_dlss.dll','nvngx_dlssd.dll','nvngx_dlssg.dll',
       'sl.common.dll','sl.interposer.dll','sl.dlss.dll','sl.dlss_d.dll','sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll',
       'amd_fidelityfx_loader_dx12.dll','amd_fidelityfx_upscaler_dx12.dll','amd_fidelityfx_framegeneration_dx12.dll')
    for name in libraries:shutil.copyfile(diag/name,runtime/name)
    for name in ('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll'):
        shutil.copyfile(imports/name,runtime/name)
    (runtime/'Homeworld.cfg').write_text(defaults((profile/'defaults.cfg').read_text()))
    for path in (profile/'Presets').glob('*.cfg'):shutil.copyfile(path,runtime/path.name)
    (runtime/'openhw.portable').write_text('OpenHW 1.0 portable installation\n')
    (runtime/'mods').mkdir();(runtime/'mods/load-order.txt').write_text('# Enabled mods, one per line; no custom mods enabled by default.\n')
    shutil.copytree(profile/'Licenses',runtime/'Licenses',dirs_exist_ok=True)
    shutil.copyfile(repo/'LICENSE.txt',runtime/'LICENSE.txt')
    shutil.copyfile(repo/'THIRD_PARTY_NOTICES.md',runtime/'THIRD_PARTY_NOTICES.md')
    (runtime/'version.json').write_text(json.dumps({'version':'1.0','normal':sha(runtime/'OpenHW.exe'),'diagnostic':sha(runtime/'OpenHW-Daignostics.exe')},indent=2))
    with zipfile.ZipFile(dev/'runtime.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for path in sorted(runtime.rglob('*')):
            if path.is_file():archive.write(path,path.relative_to(runtime).as_posix())
    if runtime.resolve().parent!=dev.resolve() or runtime.name!='Runtime':raise ValueError('Unsafe staging cleanup')
    shutil.rmtree(runtime)
    tools=dev/'SetupTools';tools.mkdir()
    for name in ('bc6h_encode.exe','ship_texture_dds.exe','DirectXTex.dll','msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll'):
        shutil.copyfile(imports/name,tools/name)
    for name in ('remastered_asset_reader.dll','OpenHWAssetCheck.exe'):shutil.copyfile(diag/name,tools/name)
    from openhw_ship_definitions import reviewed_bindings
    (tools/'native-bindings.json').write_text(json.dumps(reviewed_bindings(repo/'src/Game/NativeVisualEvents.c'),indent=2))
    symbols=dev/'Symbols';symbols.mkdir()
    shutil.copyfile(normal/'HomeworldModern.pdb',symbols/'OpenHW.pdb')
    shutil.copyfile(diag/'HomeworldModern.pdb',symbols/'OpenHW-Daignostics.pdb')
    records=[{'path':path.relative_to(destination).as_posix(),'size':path.stat().st_size,'sha256':sha(path)}
             for path in sorted(dev.rglob('*')) if path.is_file()]
    manifest={'schema':1,'version':'1.0','install_location':'installer-directory','sky_size':1024,
      'executables':['OpenHW.exe','OpenHW-Daignostics.exe'],'diagnostic_directory':'Development','files':records,
      'content_policy':2,'content_inventory':'Development/content-inventory.json',
      'excluded_mods':['Custom_Kushan_Assault_Frigate'],
      'settings':'Reviewed presets; display dimensions/refresh/adapter IDs detected on first boot'}
    (dev/'release.json').write_text(json.dumps(manifest,indent=2))
    (destination/'READ ME.txt').write_text('OpenHW 1.0\n\nRun OpenHW-Setup.exe and select your Homeworld Remastered Collection folder.\n'
      'Installation stays beside this installer, including saves, settings, imports and caches.\n'
      'Run OpenHW.exe for no logging, or OpenHW-Daignostics.exe for full logging in Development.\n'
      'Resolution and refresh rate detect on first boot. Setup does not launch the game.\n'
      'Retail data is imported locally. Approved concept art and upscaled Classic textures are inventoried separately.\n')
    print('Release staged:',len(records),'files;',sum(r['size'] for r in records),'bytes')
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('repo','profile','output'):p.add_argument('--'+key,type=Path,required=True)
    for key in ('normal-build','diagnostic-build','tools-dir'):p.add_argument('--'+key,type=Path)
    a=p.parse_args();stage(a.repo,a.profile,a.output,a.normal_build,a.diagnostic_build,a.tools_dir)
