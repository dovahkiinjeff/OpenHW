"""Stage native HOD packages for existing R1 identities; never deploy them.

No GEO export, Lua execution, gameplay stat import or archive modifications.
Each hull gets its own scale and component-qualified texture namespace.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from extract_vanilla_ship_textures import read_archive, entry_bytes
from classic_mesh_layout import translate, HEADER, OBJECT
from native_hod_surfaces import NativeSurfaces, load_reader
from stage_kushan_native_materials import select_static_material_variant, stage_active_engine_variant
from plan_kushan_hod_bindings import R1_HULLS

# Reviewed source joints, not fuzzy name matching. Mothership has two source
# nozzle markers versus four classic trails: retain classic offsets until that
# topology has a deliberate adapter rather than duplicating a nozzle.
ENGINE_JOINTS = {
    **{name: ('EngineMainDrive',) for name in (
        'AdvanceSupportFrigate','Carrier','CloakGenerator','DDDFrigate',
        'GravWellGenerator','HeavyCruiser','IonCannonFrigate','MissileDestroyer',
        'ResearchShip','ResourceController','SensorArray','StandardDestroyer','StandardFrigate')},
    **{name: ('EngineNozzle1',) for name in (
        'AttackBomber','CloakedFighter','HeavyCorvette','HeavyDefender',
        'HeavyInterceptor','LightCorvette','LightDefender','LightInterceptor',
        'MinelayerCorvette','MultiGunCorvette','ProximitySensor','RepairCorvette')},
    **{name: ('EngineNozzle1','EngineNozzle2') for name in (
        'ResourceCollector','SalCapCorvette','TargetDrone')},
}

def engine_markers(classic_type, joints):
    names=[joint['name'] for joint in joints]
    result=[]
    for i,name in enumerate(ENGINE_JOINTS.get(classic_type, ())):
        if names.count(name)!=1:
            raise ValueError(f'{classic_type}: missing/ambiguous reviewed engine marker {name}')
        result.append((f'engine_{i}',name))
    return result


def classic_bounds(data):
    # This validates legacy source data in memory only; no converted mesh is
    # written. Native replacements continue to load HODs, not exported GEOs.
    data, _ = translate(data)
    count = HEADER.unpack_from(data)[9]
    objects = [OBJECT.unpack_from(data,68+i*112) for i in range(count)]
    matrices, active = {}, set()
    def matrix(index):
        if index in matrices:
            return matrices[index]
        if index in active:
            raise ValueError('Cyclic classic parent transform')
        active.add(index)
        obj = objects[index]
        local = np.array(obj[14:]).reshape(4,4,order='F')
        parent = obj[11]
        if parent:
            if parent<68 or (parent-68)%112 or (parent-68)//112>=count:
                raise ValueError('Invalid classic parent transform')
            local = matrix((parent-68)//112) @ local
        matrices[index]=local;active.remove(index)
        return local
    spans=[]
    for i,obj in enumerate(objects):
        if not obj[4]:
            continue
        vertices=np.ndarray((obj[4],3),'<f4',buffer=data,offset=obj[8],strides=(16,4))
        pose=matrix(i)
        points=vertices @ pose[:3,:3].T + pose[:3,3]
        spans.append((points.min(0),points.max(0)))
    if not spans:
        raise ValueError('Classic hull has no geometry')
    return np.min([s[0] for s in spans],0),np.max([s[1] for s in spans],0)


def calibrated_scale(old_bounds, new_bounds):
    old_span=old_bounds[1]-old_bounds[0]
    new_span=new_bounds[1]-new_bounds[0]
    axis=int(np.argmax(old_span))
    if old_span[axis]<=0 or new_span[axis]<=0:
        raise ValueError('Degenerate scale calibration')
    scale=float(old_span[axis]/new_span[axis])
    if not np.isfinite(scale) or not .001<=scale<=1000:
        raise ValueError('Implausible native hull scale')
    return scale,axis


def cache_path(root, relative):
    path=(root/relative).resolve()
    if not path.is_relative_to(root):
        raise ValueError('Staging input escapes its cache')
    return path


def shared_surface_label(labels, model, material, component):
    """One file/texture identity per exact source model material, not per pose."""
    key=(model,material)
    if key not in labels:
        labels[key]=f'c{component}_m{material}'
    return labels[key]


def stage(reader, source, plan, materials, classic_archive, output, *,
          hulls=R1_HULLS, race='R1', identity_prefix='', engines=None):
    source,output=Path(source).resolve(),Path(output).resolve()
    materials=Path(materials).resolve()
    if not output.is_relative_to(source.parent/'package-cache'):
        raise ValueError('Output must remain inside the local package-cache sibling')
    if not materials.is_relative_to(source.parent/'material-cache'):
        raise ValueError('Material input must remain inside the local material-cache sibling')
    ships=plan['ships']
    if len(ships)!=len(hulls) or {s['classic_type'] for s in ships}!=set(hulls):
        raise ValueError('Native package plan must contain exactly the reviewed fleet identities')
    if race not in ('R1','R2','P2') or identity_prefix!=('Taiidan_' if race=='R2' else ''):
        raise ValueError('Unsupported fleet namespace')
    if any(s['classic_race']!=race or s['source']!=hulls[s['classic_type']] for s in ships):
        raise ValueError('Native package plan identity/source mismatch')
    material_data=json.loads((materials/'manifest.json').read_text())
    by_model={}
    for record in material_data['materials']:
        by_model.setdefault(record['model'],[]).append(record)
    blob,entries=read_archive(Path(classic_archive))
    classic={e.name.lower():e for e in entries}
    report={'schema':1,'runtime_deployed':False,'geometry_format':'source HOD',
            'gameplay_stats_imported':False,'scale_calibration':'longest classic hull axis',
            'hardpoint_alignment_verified':False,'ships':[]}
    for ship in ships:
        name=ship['classic_type'];folder=output/race/name/'RMNative'
        folder.mkdir(parents=True,exist_ok=True)
        source_hull=cache_path(source,ship['hull'])
        key=f'{race}\\{name}\\rl0\\lod0\\{name}'.lower()
        original=classic.get(key+'.geo') or classic.get(key+'.peo')
        if not original:
            raise ValueError('Classic scale reference missing: '+name)
        old=classic_bounds(entry_bytes(blob,original))
        with NativeSurfaces(reader,source_hull) as native:
            bounds=native.hull_bounds()
            if engines is None:
                ship_engines=engine_markers(name,native.joints())
            else:
                names=[j['name'] for j in native.joints()]
                ship_engines=[]
                for i,joint in enumerate(engines.get(name,())):
                    if names.count(joint)!=1:raise ValueError('Missing/ambiguous reviewed engine: '+joint)
                    ship_engines.append((f'engine_{i}',joint))
        scale,axis=calibrated_scale(old,bounds)
        shutil.copy2(source_hull,folder/'hull.hod')
        mad=source_hull.with_suffix('.mad')
        if mad.is_file():
            shutil.copy2(mad,folder/'animation.mad')
        lines=[f'HW_RM_{race}_VISUAL_1','type '+identity_prefix+name,f'scale {scale:.9g}',
               'hull hull.hod','mad '+('animation.mad' if mad.is_file() else '-')]
        from openhw_presentation_events import stage_events
        hull_events=stage_events(source_hull.with_suffix('.events'),folder,'hull_events')
        if hull_events:lines.append('presentation Hull '+hull_events)
        lines.extend(f'marker {semantic} Hull {joint}' for semantic,joint in ship_engines)
        if race=='R1' and name=='Mothership':
            lines.append('marker Hangardoor Hull moshipdoor')
        if race=='R1' and name=='HeavyCorvette':
            # MAD channels are hull-local absolute transforms: target the hull
            # attachment anchors, NOT the gun model's zero-relative pivots.
            lines.extend(f'animation_alias Weapon_Gun{i}_Position Hull Hardpoint_Gun{i}_Position'
                         for i in range(2))
        if race=='R2' and name=='HeavyCorvette':
            # Ambassador conversion tracks refer to the old integrated gun
            # rig, absent from RM's attached twin-barrel subsystems. HW1 R2
            # does not request those events. Keep only the four dock states.
            lines.extend('animation_clip '+clip for clip in (
                'Wings_Open','Wings_Close','Wings_Open_DMG','Wings_Close_DMG'))
        models=[ship['hull']];model_files={ship['hull']:'hull.hod'}
        for a in ship['attachments']:
            if not a.get('source_visible') or not a.get('model'):
                continue
            component=len(models);models.append(a['model'])
            file=model_files.get(a['model'])
            if file is None:
                file=f'component{component}.hod';model_files[a['model']]=file
                shutil.copy2(cache_path(source,a['model']),folder/file)
            role=1 if a['role']=='Weapon' else 2 if a['slot']=='Engine' else 3
            lines.append(f'attach {a["visual_instance"]} {a["anchor"]} {role} {file}')
            events=stage_events(cache_path(source,a['model']).with_suffix('.events'),folder,'component'+str(component)+'_events')
            if events:lines.append('presentation '+a['visual_instance']+' '+events)
        unresolved=[];labels={};copied_labels=set();surface_references=0
        for ci,model in enumerate(models):
            source_hash=hashlib.sha256(cache_path(source,model).read_bytes()).hexdigest()
            if model not in by_model:
                raise ValueError('Material records missing: '+model)
            for material in by_model[model]:
                if material['source_sha256']!=source_hash:
                    raise ValueError('Stale native material source: '+model)
                variants=material['variants']
                if not variants:
                    if material['shader']!='innateSS':
                        unresolved.append([ci,material['index'],material['shader']])
                    continue
                variant=select_static_material_variant(variants)
                label=shared_surface_label(labels,model,material['index'],ci)
                lines.append(f'surface {ci} {material["index"]} {label}')
                surface_references+=1
                if label in copied_labels:
                    continue
                for key,record in variant['files'].items():
                    file=cache_path(materials,record['path'])
                    if hashlib.sha256(file.read_bytes()).hexdigest()!=record['sha256']:
                        raise ValueError('Stale native material output: '+str(file))
                    suffix='.lif' if key=='bootstrap' else '.dds' if key=='diffuse' else '_'+key+'.dds'
                    shutil.copy2(file,folder/(label+suffix))
                stage_active_engine_variant(material, materials, folder, label)
                copied_labels.add(label)
        (folder/'visual.rmnative').write_text('\n'.join(lines)+'\n',encoding='ascii')
        report['ships'].append(dict(classic_type=name,scale=scale,calibration_axis=axis,
            classic_bounds=[b.tolist() for b in old],native_unit_bounds=[b.tolist() for b in bounds],
            components=len(models),unique_source_models=len(model_files),
            unique_surface_files=len(copied_labels),surface_references=surface_references,
            unresolved_materials=unresolved,
            source_sha256=hashlib.sha256(source_hull.read_bytes()).hexdigest()))
    (output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('reader','source','plan','materials','classic-archive','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    result=stage(load_reader(args.reader),args.source,json.loads(args.plan.read_text()),
                 args.materials,args.classic_archive,args.output)
    print(f'Staged {len(result["ships"])} native R1 packages; no GEO exports or deployment.')
