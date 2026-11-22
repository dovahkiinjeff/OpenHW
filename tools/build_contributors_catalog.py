"""Build public credits from reviewed project records and resolved dependency metadata.

Run cargo metadata --offline --locked --filter-platform x86_64-pc-windows-msvc
in tools/hod-editor/src-tauri and save it as out/contributors-cargo.json first.
No machine paths, package caches, credentials or retail files are published.
"""
from pathlib import Path
import argparse
import email
import json
import re
import sys
import urllib.parse

REPO = 'https://github.com/dovahkiinjeff/OpenHW'


def build(root, site, python_home, installed):
    output = site / 'public/data'
    notices = output / 'contributor-notices'
    notices.mkdir(parents=True, exist_ok=True)
    entries = []

    def add(name, version, group, role, license, upstream, evidence, authors='', files=(), featured=False):
        identity = re.sub('[^a-z0-9.-]+', '-', (group+'-'+name+'-'+version).lower()).strip('-')
        text = []
        for path in files:
            path = Path(path)
            if path.is_file():
                text.append('===== '+path.name+' =====\n'+path.read_text(encoding='utf-8', errors='replace'))
        url = ''
        if text:
            url = '/data/contributor-notices/'+identity+'.txt'
            (notices/(identity+'.txt')).write_text('\n\n'.join(text), encoding='utf-8', newline='\n')
        entries.append(dict(id=identity, name=name, version=version, group=group, role=role,
                            license=license or 'See upstream terms', upstream=upstream,
                            evidence=evidence, authors=authors, notices=url, featured=featured))

    def source(path):
        return REPO+'/blob/main/'+path

    def credit(name, version, group, role, license, upstream, evidence, authors='', paths=()):
        add(name, version, group, role, license, upstream, source(evidence), authors,
            [root/p for p in paths], True)

    credit('Homeworld / HomeworldSDL', 'Original engine baseline', 'Game & rendering',
           'Original simulation, campaign, source baseline and SDL portability work; includes Relic CRC and legacy Titan code.',
           'Relic non-commercial source agreement', 'https://github.com/HomeworldSDL/HomeworldSDL',
           'LICENSE.txt', 'Relic Entertainment; HomeworldSDL contributors', ['LICENSE.txt'])
    credit('HODEditorJS', '1.2.17 / 4cb39196f032', 'HOD editor',
           'Foundation of the integrated HOD editor and native HOD parser. OpenHW adds in-game hosting, mod saves and quick subsystem workflows.',
           'CC-BY-NC-4.0', 'https://github.com/GuillermoOM/HODEditorJS',
           'tools/hod-editor/README.md', 'GuillermoOM and contributors', ['tools/hod-editor/LICENSE'])
    credit('NVIDIA Streamline / DLSS / NGX', 'Streamline 2.12.0', 'Game & rendering',
           'Reconstruction integration and selected NVIDIA redistributable runtimes. Individual feature support is documented in the graphics guide.',
           'NVIDIA SDK and runtime terms', 'https://github.com/NVIDIA-RTX/Streamline',
           'THIRD_PARTY_NOTICES.md', 'NVIDIA and Streamline contributors')
    credit('AMD FidelityFX SDK', '2.3.0', 'Game & rendering',
           'FSR reconstruction, native AA and frame-generation integration.', 'SDK and component license terms',
           'https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK', 'THIRD_PARTY_NOTICES.md', 'AMD / GPUOpen and contributors')
    credit('Intel XeSS', '3.0.2', 'Game & rendering', 'XeSS reconstruction and native AA integration.',
           'Intel XeSS SDK terms', 'https://github.com/intel/xess', 'THIRD_PARTY_NOTICES.md', 'Intel and contributors')
    for name, version, path, upstream, role, authors in [
        ('D3D12 Memory Allocator','3.2.0','D3D12MemoryAllocator','GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator','Pooled Direct3D 12 GPU allocations.','AMD / GPUOpen and contributors'),
        ('meshoptimizer','1.0','meshoptimizer','zeux/meshoptimizer','Native geometry deduplication; the editor also uses its Rust binding.','Arseny Kapoulkine and contributors'),
        ('three-good-godrays','70ad1c41c787','ThreeGoodGodRays','Ameobea/three-good-godrays','Altered HLSL/D3D12 adaptation of the volumetric shaft renderer.','Casey Primozic; upstream credits n8python'),
        ('OpenMW volumetric clouds','c8830bcd2de0','OpenMWVolumetricClouds','zesterer/openmw-volumetric-clouds','Cloud shader/noise adaptation and retained upstream reference assets. No explicit license grant was found in the pinned source; the provenance record marks redistribution permission unresolved.','zesterer'),
    ]:
        folder=root/'src/ThirdParty'/path
        terms = 'Permission unresolved; see provenance' if path=='OpenMWVolumetricClouds' else 'Zlib' if path=='ThreeGoodGodRays' else 'MIT'
        files=[str(p.relative_to(root)) for p in folder.iterdir() if p.is_file() and p.name.lower().startswith(('license','copying','notice','provenance'))]
        credit(name,version,'Game & rendering',role,terms,'https://github.com/'+upstream,
               'src/ThirdParty/'+path+'/PROVENANCE.md' if (folder/'PROVENANCE.md').exists() else 'src/ThirdParty/'+path+'/UPSTREAM.json',authors,files)
    credit('stb headers', 'Vendored source', 'Game & rendering', 'Image and supporting single-header utilities.',
           'MIT OR public domain', 'https://github.com/nothings/stb', 'src/ThirdParty/stb/stb_image.h',
           'Sean Barrett and contributors', ['src/ThirdParty/stb/stb_image.h'])
    credit('LZSS / BitIO', 'Legacy source', 'Game & rendering',
           'Legacy compression code derived from The Data Compression Book and modified for Homeworld.',
           'See source headers and original source terms', 'https://marknelson.us/', 'src/ThirdParty/LZSS/LZSS.c',
           'Mark Nelson; Jean-loup Gailly; Darren Stone', ['src/ThirdParty/LZSS/LZSS.c','src/ThirdParty/LZSS/BitIO.c'])
    for name, author, path, upstream in [('Rajdhani','Indian Type Foundry','Rajdhani','rajdhani'),('Share Tech Mono','Ralph du Carrois / Carrois Type Design','ShareTechMono','sharetechmono')]:
        credit(name,'Bundled font','Fonts & artwork','Locally hosted interface typography.','OFL-1.1',
               'https://github.com/google/fonts/tree/main/ofl/'+upstream,'assets/Launcher/'+path+'-OFL.txt',author,['assets/Launcher/'+path+'-OFL.txt'])
    credit('Homeworld artwork and community captures','Supplied material','Fonts & artwork',
           'Original concept artists and rights holders retain ownership. Gallery captions and the provenance inventory identify supplied images and their treatment; retail assets are imported from each player’s owned Collection.',
           'Original artwork/retail rights; not an open-source grant', source('assets/UI/ART_PROVENANCE.md'),
           'assets/UI/ART_PROVENANCE.md','Original Homeworld artists; community contributors',['assets/UI/ART_PROVENANCE.md'])

    # All installed native ports, including the build helpers, with retained copyright files.
    native=root/'out/build/deploy-x64-clean/vcpkg_installed/x64-windows/share'
    status=(native.parent.parent/'vcpkg/status').read_text(encoding='utf-8')
    native_urls={'sdl2':'https://github.com/libsdl-org/SDL','sdl2-net':'https://github.com/libsdl-org/SDL_net',
        'ffmpeg':'https://github.com/FFmpeg/FFmpeg','ffmpeg-bin2c':'https://github.com/FFmpeg/FFmpeg',
        'directx-dxc':'https://github.com/microsoft/DirectXShaderCompiler','directxmath':'https://github.com/microsoft/DirectXMath',
        'directxtex':'https://github.com/microsoft/DirectXTex','pkgconf':'https://github.com/pkgconf/pkgconf',
        'vcpkg-tool-meson':'https://github.com/mesonbuild/meson'}
    licenses={'sdl2':'Zlib','sdl2-net':'Zlib','ffmpeg':'LGPL-2.1-or-later; component/build-dependent',
              'ffmpeg-bin2c':'See FFmpeg terms','directx-dxc':'LLVM/Clang and component terms','directxtex':'MIT','directxmath':'MIT','pkgconf':'ISC','vcpkg-tool-meson':'Apache-2.0'}
    for paragraph in status.split('\n\n'):
        row=dict(line.split(': ',1) for line in paragraph.splitlines() if ': ' in line and not line.startswith(' '))
        if not row.get('Version') or row.get('Feature') or row.get('Status')!='install ok installed': continue
        name=row['Package']; folder=native/name
        files=list(folder.glob('copyright'))+list((native/'licenses'/('SDL2_net' if name=='sdl2-net' else name)).glob('*'))
        add(name,row['Version'],'Native libraries & build tools',row.get('Description','Native dependency/build helper used by the vcpkg build.'),
            licenses.get(name,'MIT; see vcpkg notices'),native_urls.get(name,'https://github.com/microsoft/vcpkg'),source('vcpkg.json'),
            files=files,featured=name in ('sdl2','sdl2-net','ffmpeg','directxtex','directx-dxc'))

    # Metadata is resolved for Windows. Keep only nodes reachable from the editor root.
    cargo=json.loads((root/'out/contributors-cargo.json').read_text(encoding='utf-8'))
    nodes={n['id']:n for n in cargo['resolve']['nodes']}; todo=[cargo['resolve']['root']]; used=set()
    while todo:
        key=todo.pop()
        if key in used: continue
        used.add(key);todo.extend(d['pkg'] for d in nodes[key]['deps'])
    for p in cargo['packages']:
        if p['id'] not in used or not p.get('source'):continue
        folder=Path(p['manifest_path']).parent
        files=[x for x in folder.rglob('*') if x.is_file() and x.name.lower().startswith(('license','copying','notice','copyright'))]
        add(p['name'],p['version'],'HOD editor · Rust',p.get('description') or 'Editor dependency.',p.get('license'),
            p.get('repository') or 'https://crates.io/crates/'+p['name']+'/'+p['version'],
            source('tools/hod-editor/src-tauri/Cargo.lock'),'; '.join(p.get('authors',[])),files,
            featured=p['name'] in ('wry','image','meshopt','texpresso'))

    # Runtime and development/optional platform packages are explicitly distinguished.
    for folder, label, evidence in [(root/'tools/hod-editor','HOD editor · JavaScript','tools/hod-editor/package-lock.json'),(site,'Website · JavaScript','docs/CONTRIBUTORS.md')]:
        lock=json.loads((folder/'package-lock.json').read_text(encoding='utf-8'))
        for key,p in lock['packages'].items():
            if not key or not p.get('version'):continue
            name=p.get('name') or key.split('node_modules/')[-1]; local=folder/key
            meta=json.loads((local/'package.json').read_text(encoding='utf-8')) if (local/'package.json').is_file() else {}
            repo=meta.get('repository',{});repo=repo if isinstance(repo,str) else repo.get('url','')
            repo=repo.removeprefix('git+').removesuffix('.git')
            if repo.startswith('git://'):repo='https://'+repo[6:]
            if name=='dompurify':repo='https://github.com/cure53/DOMPurify'
            if not repo.startswith('https://'):repo='https://www.npmjs.com/package/'+name+'/v/'+p['version']
            role='Build/development dependency' if p.get('dev') else 'Bundled interface dependency'
            if p.get('optional'):role+='; optional/platform-specific locked package'
            author=meta.get('author','');author=author if isinstance(author,str) else author.get('name','')
            files=[x for x in local.glob('*') if x.is_file() and x.name.lower().startswith(('license','copying','notice','copyright'))]
            add(name,p['version'],label,role,p.get('license') or meta.get('license'),repo,source(evidence),author,files,
                featured=name in ('react','three','lucide-react','dompurify','marked'))

    # Frozen helper packages plus packaging dependencies. Copy their own notices verbatim.
    search=[root/'out/importer-build-deps',python_home/'Lib/site-packages',installed/'Development/Installer/Development']
    wanted={'numpy','pillow','charset-normalizer','typing-extensions','pyinstaller','pyinstaller-hooks-contrib','altgraph','packaging','pefile','pywin32-ctypes','setuptools'}
    found=set()
    for base in reversed(search):
        for folder in base.glob('*.dist-info'):
            if not (folder/'METADATA').is_file():continue
            m=email.message_from_string((folder/'METADATA').read_text(encoding='utf-8'))
            name=m['Name']; normalized=name.lower().replace('_','-')
            if normalized not in wanted or normalized in found:continue
            if normalized in {'pyinstaller','pyinstaller-hooks-contrib','altgraph','packaging','pefile','pywin32-ctypes','setuptools'} and base != root/'out/importer-build-deps':continue
            found.add(normalized)
            urls=[x.split(',',1)[-1].strip() for x in m.get_all('Project-URL',[]) if re.match(r'(source|source code|repository|github)',x,re.I)]
            upstream=next((u for u in urls if 'github.com' in u),None) or m.get('Home-page') or 'https://pypi.org/project/'+name+'/'+m['Version']
            files=[p for p in folder.rglob('*') if p.is_file() and (p.parent.name.lower()=='licenses' or p.name.lower().startswith(('license','copying','notice','copyright')))]
            terms=m.get('License-Expression') or (m.get('License','').splitlines() or ['See bundled license'])[0]
            add(name,m['Version'],'Installer & launcher',m.get('Summary','Python dependency.'),('BSD-3-Clause and bundled third-party terms' if normalized=='numpy' else terms),upstream,
                source('tools/package_updater.py'),m.get('Author') or m.get('Author-email',''),files,
                featured=normalized in ('numpy','pillow','pyinstaller'))
    if found != wanted:raise ValueError('Missing Python package attribution: '+str(wanted-found))
    pillow_notice=next(e for e in entries if e['name']=='pillow')['notices']
    pillow_text=(site/'public'/pillow_notice.lstrip('/')).read_text(encoding='utf-8')
    pillow_repos={'brotli':'google/brotli','freetype':'freetype/freetype','harfbuzz':'harfbuzz/harfbuzz',
        'lcms2':'mm2/Little-CMS','libavif':'AOMediaCodec/libavif','libjpeg-turbo':'libjpeg-turbo/libjpeg-turbo',
        'libpng':'pnggroup/libpng','libwebp':'webmproject/libwebp','openjpeg':'uclouvain/openjpeg',
        'xz':'tukaani-project/xz','zlib-ng':'zlib-ng/zlib-ng'}
    for title in re.findall(r'^===== (.+) =====$',pillow_text,re.M):
        match=re.fullmatch(r'(.+)-(\d.+)',title)
        if not match:continue
        name,version=match.groups()
        upstream='https://gitlab.com/libtiff/libtiff' if name=='tiff' else 'https://github.com/'+pillow_repos[name]
        add(name,version,'Pillow · bundled components','Component identified in the bundled Pillow wheel’s full third-party notice. Codec availability depends on the Pillow build.',
            'See component terms in Pillow notices',upstream,source('THIRD_PARTY_NOTICES.md'))
        entries[-1]['notices']=pillow_notice
    # CPython and codec/runtime components included in the frozen Windows distribution.
    for name,version,upstream,terms,role in [
        ('CPython','3.12.14','python/cpython','PSF-2.0 and bundled third-party notices','Python runtime and standard library for the installer and utilities.'),
        ('Tcl / Tk','8.6','tcltk/tk','Tcl/Tk license terms','Launcher and installer windows.'),
        ('OpenSSL','3.x','openssl/openssl','Apache-2.0','HTTPS/TLS in the frozen Python runtime.'),
        ('libffi','8 ABI','libffi/libffi','MIT','Foreign-function interface in the Python runtime.'),
        ('SQLite','Bundled runtime','sqlite/sqlite','Public domain dedication','Python runtime database library.'),
        ('zlib','Bundled runtime','madler/zlib','Zlib','Compression in Python and image processing.'),
        ('bzip2','Bundled runtime','libarchive/bzip2','bzip2 license','Python compression support.'),
        ('XZ / liblzma','Bundled runtime','tukaani-project/xz','See component license notices','Python LZMA compression support.'),
        ('Expat','Bundled runtime','libexpat/libexpat','MIT','Python XML parsing support.'),
        ('libmpdec','Bundled runtime','python/cpython','BSD-2-Clause','Python decimal arithmetic.'),
        ('OpenBLAS','Bundled NumPy wheel','OpenMathLib/OpenBLAS','BSD-3-Clause and included notices','Linear algebra runtime supplied with NumPy.'),
        ('websockets','17.3','python-websockets/websockets','BSD-3-Clause','Dedicated multiplayer server transport.'),
        ('Caddy','Server deployment','caddyserver/caddy','Apache-2.0','Website HTTPS and reverse proxy.'),
        ('Wine','Server deployment','wine-mirror/wine','LGPL-2.1-or-later and component terms','Runs the authoritative Windows simulation worker on the Linux server.'),
        ('CMake','Build tool','Kitware/CMake','BSD-3-Clause and included notices','Native build configuration.'),
        ('vcpkg','Build tool','microsoft/vcpkg','MIT','Native dependency resolution and build recipes.'),
        ('Node.js / npm','Build tools','nodejs/node','MIT and included third-party terms','Builds editor and website assets.'),
        ('Rust / Cargo','Build tools','rust-lang/rust','MIT OR Apache-2.0 and component terms','Compiles the embedded editor and parser.'),
    ]:
        files=[python_home/'Lib/LICENSE.txt'] if name=='CPython' else []
        add(name,version,'Runtime & infrastructure',role,terms,('https://www.bytereef.org/mpdecimal/' if name=='libmpdec' else 'https://github.com/'+upstream),source('THIRD_PARTY_NOTICES.md'),files=files,featured=name=='CPython')
    # NumPy's wheel explicitly lists additional bundled and source-only subcomponents.
    np_license=installed/'Development/Installer/Development/numpy-2.3.5.dist-info/LICENSE.txt'
    for chunk in re.split(r'(?m)^Name: ',np_license.read_text(encoding='utf-8'))[1:]:
        name=chunk.splitlines()[0].strip()
        if name.lower()=='openblas':continue
        terms=re.search(r'(?m)^License: (.+)',chunk)
        add(name,'NumPy notices','NumPy · bundled/source components','Component listed in the NumPy wheel’s full notice; its Files and Availability fields distinguish bundled binaries from source-only/build inputs.',terms.group(1) if terms else 'See NumPy notices',
            'https://github.com/numpy/numpy',source('THIRD_PARTY_NOTICES.md'),files=[np_license])
    add('Microsoft platform runtimes','Windows prerequisites','Platform prerequisites',
        'Direct3D/Windows SDK, Visual C++ runtime and Edge WebView2 support. These are platform/vendor prerequisites, not claimed as open-source packages.',
        'Applicable Microsoft SDK/runtime terms','https://developer.microsoft.com/microsoft-edge/webview2/',source('tools/hod-editor/README.md'))
    data=dict(schema=1, reviewed='2026-11-22', release='1.1.14',
              scope='Project source, bundled native/Python runtimes, Windows editor dependency closure, JavaScript lockfiles (including build and optional platform packages), fonts, and direct server/build tools. Operating-system distribution packages and retail game assets are outside the software dependency inventory.',
              entries=sorted(entries,key=lambda x:(x['group'],x['name'].casefold(),x['version'])))
    if len({e['id'] for e in entries})!=len(entries):raise ValueError('Duplicate credit identities')
    (output/'contributors.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    print('Built contributors catalog:',len(entries),'entries;',len({e['notices'] for e in entries if e['notices']}),'unique local notice documents')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--site',type=Path,required=True)
    p.add_argument('--installed',type=Path,required=True)
    p.add_argument('--python-home',type=Path,default=Path(sys.executable).parent)
    a=p.parse_args();build(a.root.resolve(),a.site.resolve(),a.python_home.resolve(),a.installed.resolve())
