"""Build a local-only Kushan visual dependency plan; never execute source Lua.

No runtime overrides, gameplay stats or release assets are written. Subsystem
models belong to the hulls that reference them, not new playable ship types.
"""
import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

from inspect_remastered_scene import Reader
from translate_remastered_background import chunks

# Existing HW1 R1 identities only. Missile/mine projectiles and headshot asteroid
# are not fleet ships; Ambassador is not an added construction option.
R1_HULLS = {
    'AdvanceSupportFrigate': 'kus_supportfrigate', 'AttackBomber': 'kus_attackbomber',
    'Carrier': 'kus_carrier', 'CloakedFighter': 'kus_cloakedfighter',
    'CloakGenerator': 'kus_cloakgenerator', 'CryoTray': 'kus_cryotray',
    'DDDFrigate': 'kus_dronefrigate', 'Drone': 'kus_drone0',
    'GravWellGenerator': 'kus_gravwellgenerator', 'HeavyCorvette': 'kus_heavycorvette',
    'HeavyCruiser': 'kus_heavycruiser', 'HeavyDefender': 'kus_defender',
    'HeavyInterceptor': 'kus_interceptor', 'IonCannonFrigate': 'kus_ioncannonfrigate',
    'LightCorvette': 'kus_lightcorvette', 'LightDefender': 'kus_lightdefender',
    'LightInterceptor': 'kus_scout', 'MinelayerCorvette': 'kus_minelayercorvette',
    'MissileDestroyer': 'kus_missiledestroyer', 'Mothership': 'kus_mothership',
    'MultiGunCorvette': 'kus_multiguncorvette', 'Probe': 'kus_probe',
    'ProximitySensor': 'kus_proximitysensor', 'RepairCorvette': 'kus_repaircorvette',
    'ResearchShip': 'kus_researchship', 'ResourceCollector': 'kus_resourcecollector',
    'ResourceController': 'kus_resourcecontroller', 'SalCapCorvette': 'kus_salvagecorvette',
    'SensorArray': 'kus_sensorarray', 'StandardDestroyer': 'kus_destroyer',
    'StandardFrigate': 'kus_assaultfrigate', 'TargetDrone': 'kus_targetdrone',
}


def literal_calls(source, function, owner):
    """Recognize one audited Lua data-call shape, not a Lua evaluator."""
    source = re.sub(r'--[^\n]*', '', source)
    result = []
    for line in source.splitlines():
        line = line.strip()
        if not line.startswith(function + '('):
            continue
        match = re.fullmatch(re.escape(function) + r'\(\s*' + re.escape(owner) +
                             r'\s*,\s*((?:"[^"\\\n]*"\s*,?\s*)+)\);?', line)
        if not match:
            raise ValueError('Unsupported data-call syntax: ' + function)
        values = re.findall(r'"([^"\\\n]*)"', match[1])
        # Reject missing separators and trailing commas, even if the regex above
        # matched the string tokens. Nothing here is executed as a script.
        compact = re.sub(r'\s+', '', match[1])
        if compact != ','.join('"' + re.sub(r'\s+', '', v) + '"' for v in values):
            raise ValueError('Invalid literal argument separators')
        result.append(values)
    return result


def kus_name(name):
    value = name.lower()
    if value != 'hw1_researchmodule' and not re.fullmatch(r'kus_[a-z0-9_]+', value):
        raise ValueError('Dependency is not a Kushan model: ' + name)
    return value


def joint_names(path):
    root = dict(chunks(path.read_bytes()))
    reader = Reader(dict(chunks(root[b'DTRM']))[b'HIER'])
    count, = reader.unpack('<i')
    if not -10000 <= count < 0:
        raise ValueError('Unsupported hierarchy: ' + str(path))
    result = []
    for _ in range(-count):
        result.append(reader.string())
        reader.string()
        reader.unpack('<9f')
    reader.done()
    return Counter(result)


def resolve_subsystem(root, name, chain=(), validate_name=kus_name):
    name = validate_name(name)
    if name in chain:
        raise ValueError('Cyclic shared subsystem model: ' + name)
    folder = root / 'subsystem' / name
    script = folder / (name + '.subs')
    if not script.is_file():
        # An innate hull weapon can use the hull's own geometry, without a
        # separate .subs model. Retain the distinction in the plan.
        return dict(subsystem=name, model=None, kind='hull-weapon-or-no-model')
    source = script.read_text(encoding='utf-8')
    shared = literal_calls(source, 'LoadSharedModel', 'NewSubSystemType')
    if len(shared) > 1 or (shared and len(shared[0]) != 1):
        raise ValueError('Ambiguous shared model: ' + name)
    if shared:
        target = resolve_subsystem(root, shared[0][0], chain + (name,), validate_name)
        if not target['model']:
            raise ValueError('Shared model is missing: ' + name)
        return {**target, 'subsystem': name, 'shared_from': validate_name(shared[0][0])}
    model = folder / (name + '.hod')
    if not model.is_file():
        raise ValueError('Subsystem descriptor has no model: ' + name)
    return dict(subsystem=name, model=model.relative_to(root).as_posix(),
                kind='attached-model', source_visible=not bool(re.search(r'NewSubSystemType\.visible\s*=\s*0\b', source)),
                sha256=hashlib.sha256(model.read_bytes()).hexdigest())


def plan(root, hulls=None, race='R1', validate_name=None, joint_lookup=None):
    if hulls is None: hulls=R1_HULLS
    if validate_name is None: validate_name=kus_name
    if joint_lookup is None: joint_lookup=joint_names
    root = Path(root).resolve()
    ships = []
    for classic, source in hulls.items():
        folder = root / 'ship' / source
        hod, script = folder / (source + '.hod'), folder / (source + '.ship')
        names = joint_lookup(hod)
        attachments = []
        source_script = script.read_text(encoding='utf-8') if script.is_file() else ''
        for values in literal_calls(source_script, 'StartShipHardPointConfig', 'NewShipType'):
            if len(values) != 14:
                raise ValueError('Unsupported hardpoint argument count: ' + source)
            slot, anchor, role, _innate, _damageable, dependency = values[:6]
            position = anchor + '_Position'
            if names[position] != 1:
                raise ValueError('Missing/ambiguous attachment joint: ' + source + '/' + position)
            attachments.append(dict(slot=slot, anchor=position, role=role,
                                    **resolve_subsystem(root, dependency, validate_name=validate_name)))
        slot_counts = Counter(a['slot'] for a in attachments)
        instances = set()
        for attachment in attachments:
            # The shipped cruiser repeats Weapon_Turret_Ion1 at TWO anchors.
            # Keep the source weapon identity, but never collapse its visual
            # instances or fabricate a second gameplay weapon.
            identity = attachment['slot']
            if slot_counts[identity] > 1:
                identity += '@' + attachment['anchor']
            if identity in instances:
                raise ValueError('Duplicate visual attachment instance: ' + source + '/' + identity)
            attachment['visual_instance'] = identity
            instances.add(identity)
        ships.append(dict(classic_race=race, classic_type=classic, source=source,
                          hull=hod.relative_to(root).as_posix(), attachments=attachments,
                          source_descriptor_present=script.is_file(),
                          duplicate_marker_names=sorted(n for n, count in names.items() if count > 1),
                          enabled=False))
    return dict(schema=1, purpose='preactivation-visual-binding-audit',
                gameplay_stats_imported=False, runtime_files_modified=False, ships=ships)


def hierarchy_audit(result):
    """Headless renderer test input, not an activation/runtime manifest."""
    rows = []
    for ship in result['ships']:
        rows.append(('H', ship['classic_type'], ship['hull']))
        for attachment in ship['attachments']:
            if not attachment['model'] or not attachment.get('source_visible', True):
                continue
            role = 1 if attachment['role'] == 'Weapon' else 2 if attachment['slot'] == 'Engine' else 3
            rows.append(('A', attachment.get('visual_instance', attachment['slot']), attachment['anchor'], str(role), attachment['model']))
        rows.append(('E',))
    if any('\t' in field or '\n' in field or '\r' in field for row in rows for field in row):
        raise ValueError('Invalid hierarchy audit field')
    return ''.join('\t'.join(row) + '\n' for row in rows)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root', type=Path, required=True)
    cli.add_argument('--output', type=Path, required=True)
    cli.add_argument('--hierarchy-audit', type=Path)
    args = cli.parse_args()
    result = plan(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    if args.hierarchy_audit:
        args.hierarchy_audit.parent.mkdir(parents=True, exist_ok=True)
        args.hierarchy_audit.write_text(hierarchy_audit(result), encoding='utf-8')
    for ship in result['ships']:
        models = [a['model'] for a in ship['attachments'] if a['model']]
        print(f"R1/{ship['classic_type']}: {len(models)} attached model instances")


if __name__ == '__main__':
    main()
