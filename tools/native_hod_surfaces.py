"""Use the engine's native HOD reader for local material/texture staging.

No second geometry parser, Lua execution, GEO export or archive mutation.
The reader DLL is a build-only tool, not a runtime/distributed dependency.
"""
import ctypes as c
import io
import struct
from pathlib import Path
from PIL import Image


class Parameter(c.Structure):
    _fields_ = [('name', c.c_char_p), ('type', c.c_uint), ('texture', c.c_uint), ('value', c.c_float*4)]


class Material(c.Structure):
    _fields_ = [('name', c.c_char_p), ('shader', c.c_char_p), ('count', c.c_uint), ('parameters', c.POINTER(Parameter))]


class Mip(c.Structure):
    _fields_ = [('width', c.c_uint), ('height', c.c_uint), ('offset', c.c_size_t), ('size', c.c_size_t)]


class Texture(c.Structure):
    _fields_ = [('name', c.c_char_p), ('format', c.c_char_p), ('count', c.c_uint), ('mips', c.POINTER(Mip))]


class Joint(c.Structure):
    _fields_ = [('name', c.c_char_p), ('parent', c.c_char_p), ('position', c.c_float*3),
                ('rotation', c.c_float*3), ('gimbal', c.c_float*3)]

class DockPoint(c.Structure):
    _fields_ = [('position', c.c_float*3), ('rotation', c.c_float*9),
                ('tolerance', c.c_float), ('maxSpeed', c.c_float), ('flags', c.c_uint*2)]

class DockPath(c.Structure):
    _fields_ = [('name', c.c_char_p), ('parent', c.c_char_p),
                ('compatibleShips', c.c_char_p), ('linkedPath', c.c_char_p),
                ('flags', c.c_uint*5), ('reserved', c.c_uint), ('pointCount', c.c_uint),
                ('points', c.POINTER(DockPoint))]

class Vertex(c.Structure):
    _fields_ = [('position', c.c_float*3), ('normal', c.c_float*3),
                ('uv', c.c_float*2), ('badgeUv', c.c_float*2)]

class Part(c.Structure):
    _fields_ = [('material', c.c_uint), ('mask', c.c_uint), ('vertexCount', c.c_uint),
                ('indexCount', c.c_uint), ('vertices', c.POINTER(Vertex)),
                ('indices', c.POINTER(c.c_ushort)),
                ('vertexColors', c.POINTER(c.c_uint))]

class Mesh(c.Structure):
    _fields_ = [('name', c.c_char_p), ('parent', c.c_char_p), ('lod', c.c_uint),
                ('partCount', c.c_uint), ('parts', c.POINTER(Part))]

class Draw(c.Structure):
    _fields_ = [('component', c.c_uint), ('role', c.c_uint), ('slot', c.c_char_p),
                ('mesh', c.POINTER(Mesh)), ('part', c.POINTER(Part)),
                ('material', c.POINTER(Material)), ('matrix', c.c_float*16)]

DrawCallback = c.CFUNCTYPE(None, c.c_void_p, c.POINTER(Draw))


class Track(c.Structure):
    _fields_ = [('joint', c.c_char_p), ('count', c.c_uint), ('curves', c.POINTER(c.c_uint))]


class Clip(c.Structure):
    _fields_ = [('name', c.c_char_p), ('start', c.c_double), ('end', c.c_double),
                ('loopStart', c.c_double), ('loopEnd', c.c_double), ('count', c.c_uint),
                ('tracks', c.POINTER(Track)), ('playable', c.c_int)]


def load_reader(path):
    dll = c.CDLL(str(Path(path).resolve()))
    signatures = {
        'hwRmHodSceneRead': (c.c_int, [c.c_void_p, c.c_size_t, c.POINTER(c.c_void_p), c.c_char_p, c.c_size_t]),
        'hwRmHodSceneFree': (None, [c.c_void_p]),
        'hwRmHodSceneMaterialCount': (c.c_uint, [c.c_void_p]),
        'hwRmHodSceneMaterial': (c.POINTER(Material), [c.c_void_p, c.c_uint]),
        'hwRmHodSceneMeshCount': (c.c_uint, [c.c_void_p]),
        'hwRmHodSceneMesh': (c.POINTER(Mesh), [c.c_void_p, c.c_uint]),
        'hwRmHodSceneTextureCount': (c.c_uint, [c.c_void_p]),
        'hwRmHodSceneTexture': (c.POINTER(Texture), [c.c_void_p, c.c_uint]),
        'hwRmHodSceneTextureData': (c.c_int, [c.c_void_p, c.POINTER(c.c_void_p), c.POINTER(c.c_size_t), c.c_char_p, c.c_size_t]),
        'hwRmHodSceneJointCount': (c.c_uint, [c.c_void_p]),
        'hwRmHodSceneJoint': (c.POINTER(Joint), [c.c_void_p, c.c_uint]),
        'hwRmHodSceneDockPathCount': (c.c_uint, [c.c_void_p]),
        'hwRmHodSceneDockPath': (c.POINTER(DockPath), [c.c_void_p, c.c_uint]),
        'hwRmMadRead': (c.c_int, [c.c_void_p, c.c_size_t, c.POINTER(c.c_void_p), c.c_char_p, c.c_size_t]),
        'hwRmMadFree': (None, [c.c_void_p]),
        'hwRmMadClipCount': (c.c_uint, [c.c_void_p]),
        'hwRmMadClip': (c.POINTER(Clip), [c.c_void_p, c.c_uint]),
        'hwRmSubsystemLayerCreate': (c.c_void_p, [c.c_void_p, c.c_char_p, c.c_size_t]),
        'hwRmSubsystemLayerFree': (None, [c.c_void_p]),
        'hwRmSubsystemInstanceCreate': (c.c_void_p, [c.c_void_p]),
        'hwRmSubsystemInstanceFree': (None, [c.c_void_p]),
        'hwRmSubsystemJointTransform': (c.c_int, [c.c_void_p, c.c_uint, c.c_char_p,
            c.c_float, c.c_void_p, c.c_void_p, c.POINTER(c.c_float), c.c_char_p, c.c_size_t]),
        'hwRmSubsystemVisit': (c.c_int, [c.c_void_p, c.c_uint, c.c_float, c.c_void_p,
                                        c.c_void_p, DrawCallback, c.c_void_p, c.c_char_p, c.c_size_t]),
    }
    for name, (result, args) in signatures.items():
        function = getattr(dll, name); function.restype = result; function.argtypes = args
    return dll


def animation_clips(dll, path):
    data = c.create_string_buffer(Path(path).read_bytes())
    mad, error = c.c_void_p(), c.create_string_buffer(512)
    if not dll.hwRmMadRead(data, len(data)-1, c.byref(mad), error, len(error)):
        raise ValueError(error.value.decode())
    try:
        clips = []
        for i in range(dll.hwRmMadClipCount(mad)):
            clip = dll.hwRmMadClip(mad, i).contents
            clips.append(dict(index=i, name=clip.name.decode(), start=clip.start, end=clip.end,
                              playable=bool(clip.playable),
                              joints=[clip.tracks[j].joint.decode() for j in range(clip.count)]))
        return clips
    finally:
        dll.hwRmMadFree(mad)


class NativeSurfaces:
    def __init__(self, dll, path):
        self.dll, self.path, self.scene = dll, Path(path), c.c_void_p()
        self.pool, self.images = None, {}

    def __enter__(self):
        if not 0 < self.path.stat().st_size <= 64*1024*1024:
            raise ValueError('Invalid native HOD size')
        data = c.create_string_buffer(self.path.read_bytes())
        error = c.create_string_buffer(512)
        if not self.dll.hwRmHodSceneRead(data, len(data)-1, c.byref(self.scene), error, len(error)):
            raise ValueError(f'{self.path.name}: {error.value.decode()}')
        return self

    def __exit__(self, *_):
        self.dll.hwRmHodSceneFree(self.scene); self.scene = c.c_void_p()
        self.pool = None; self.images.clear()

    def materials(self):
        result = []
        for i in range(self.dll.hwRmHodSceneMaterialCount(self.scene)):
            material = self.dll.hwRmHodSceneMaterial(self.scene, i).contents
            parameters = {}
            for pi in range(material.count):
                parameter = material.parameters[pi]
                parameters[parameter.name.decode()] = parameter.texture if parameter.type == 5 else list(parameter.value)
            result.append(dict(index=i, name=material.name.decode(), shader=material.shader.decode(), parameters=parameters))
        return result

    def joints(self):
        result = []
        for i in range(self.dll.hwRmHodSceneJointCount(self.scene)):
            joint = self.dll.hwRmHodSceneJoint(self.scene, i).contents
            result.append(dict(index=i, name=joint.name.decode(), parent=joint.parent.decode(),
                               position=list(joint.position), rotation=list(joint.rotation)))
        return result

    def dock_paths(self):
        result=[]
        for i in range(self.dll.hwRmHodSceneDockPathCount(self.scene)):
            path=self.dll.hwRmHodSceneDockPath(self.scene,i).contents
            result.append(dict(name=path.name.decode(),parent=path.parent.decode(),
                compatible_ships=path.compatibleShips.decode(),linked_path=path.linkedPath.decode(),
                flags=list(path.flags),reserved=path.reserved,
                points=[dict(position=list(point.position),rotation=list(point.rotation),
                    tolerance=point.tolerance,max_speed=point.maxSpeed,flags=list(point.flags))
                    for point in path.points[:path.pointCount]]))
        return result

    def vertex_tints(self, material):
        colors=[]
        for mi in range(self.dll.hwRmHodSceneMeshCount(self.scene)):
            mesh=self.dll.hwRmHodSceneMesh(self.scene,mi).contents
            for pi in range(mesh.partCount):
                part=mesh.parts[pi]
                if part.material==material and part.indexCount:
                    if not part.vertexColors:
                        raise ValueError('Native vertex tint missing')
                    colors.extend(part.vertexColors[i] for i in range(part.vertexCount))
        return colors

    def marker_transforms(self, names, scale=1):
        """Actual runtime posed hierarchy, not a second basis/rotation parser."""
        error=c.create_string_buffer(512)
        layer=self.dll.hwRmSubsystemLayerCreate(self.scene,error,len(error))
        if not layer:
            raise ValueError(error.value.decode())
        instance=self.dll.hwRmSubsystemInstanceCreate(layer)
        try:
            if not instance:
                raise ValueError('Native marker instance allocation failed')
            result={}
            for name in names:
                matrix=(c.c_float*16)()
                if not self.dll.hwRmSubsystemJointTransform(instance,0,name.encode('ascii'),
                        scale,None,None,matrix,error,len(error)):
                    raise ValueError(error.value.decode())
                result[name]=list(matrix)
            return result
        finally:
            self.dll.hwRmSubsystemInstanceFree(instance)
            self.dll.hwRmSubsystemLayerFree(layer)

    def hull_bounds(self):
        """Use the actual runtime hierarchy/axis/material policy at unit scale."""
        import numpy as np
        error = c.create_string_buffer(512)
        layer = self.dll.hwRmSubsystemLayerCreate(self.scene, error, len(error))
        if not layer:
            raise ValueError(error.value.decode())
        instance = self.dll.hwRmSubsystemInstanceCreate(layer)
        if not instance:
            self.dll.hwRmSubsystemLayerFree(layer)
            raise ValueError('Native hull instance allocation failed')
        spans, errors = [], []
        @DrawCallback
        def collect(_, ptr):
            try:
                draw = ptr.contents
                part = draw.part.contents
                # Published native vertex layout is 10 float32 values.
                raw = c.cast(part.vertices, c.POINTER(c.c_float))
                positions = np.ctypeslib.as_array(raw, shape=(part.vertexCount*10,)).reshape(-1,10)[:,:3]
                matrix = np.asarray(draw.matrix).reshape(4,4,order='F')
                transformed = positions @ matrix[:3,:3].T + matrix[:3,3]
                spans.append((transformed.min(0), transformed.max(0)))
            except Exception as exc:
                errors.append(exc)
        try:
            if not self.dll.hwRmSubsystemVisit(instance, 0, 1, None, None, collect, None, error, len(error)):
                raise ValueError(error.value.decode())
            if errors:
                raise errors[0]
            if not spans:
                raise ValueError('Native hull has no visible geometry')
            return np.min([s[0] for s in spans],0), np.max([s[1] for s in spans],0)
        finally:
            self.dll.hwRmSubsystemInstanceFree(instance)
            self.dll.hwRmSubsystemLayerFree(layer)

    def image(self, index):
        if index in self.images:
            return self.images[index]
        if not isinstance(index, int) or not 0 <= index < self.dll.hwRmHodSceneTextureCount(self.scene):
            raise ValueError('Invalid native texture index')
        texture = self.dll.hwRmHodSceneTexture(self.scene, index).contents
        mip, fmt = texture.mips[0], texture.format.decode()
        if self.pool is None:
            data, size, error = c.c_void_p(), c.c_size_t(), c.create_string_buffer(512)
            if not self.dll.hwRmHodSceneTextureData(self.scene, c.byref(data), c.byref(size), error, len(error)):
                raise ValueError(error.value.decode())
            self.pool = c.string_at(data, size.value)
        if mip.offset+mip.size > len(self.pool):
            raise ValueError('Native texture mip exceeds pool')
        raw = self.pool[mip.offset:mip.offset+mip.size]
        if fmt == '8888':
            image = Image.frombytes('RGBA', (mip.width, mip.height), raw)
        else:
            header = bytearray(128); header[:4] = b'DDS '
            struct.pack_into('<7I', header, 4, 124, 0x81007, mip.height, mip.width, len(raw), 0, 1)
            struct.pack_into('<II4s', header, 76, 32, 4, fmt.encode())
            struct.pack_into('<I', header, 108, 0x1000)
            with Image.open(io.BytesIO(header+raw)) as decoded:
                image = decoded.convert('RGBA')
        self.images[index] = image
        return image
