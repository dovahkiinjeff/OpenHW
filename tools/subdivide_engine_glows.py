"""Generate subdivided engine glow GEO overrides from the owned Classic BIG.

Source payload/materials/hierarchy remain intact; append refined streams and
redirect each object. Midpoint subdivision preserves UV seams per triangle.
Optional point-normal curvature rounds the plume while retaining nozzle depth.
Never subdivide energybolt (a separate acceleration effect).
"""
import argparse
import json
import math
import struct
from pathlib import Path, PureWindowsPath
from extract_vanilla_ship_textures import read_archive, entry_bytes
from classic_mesh_layout import HEADER, OBJECT, translate

VERTEX=struct.Struct('<3fi')
NORMAL=struct.Struct('<3f4s')
POLYGON=struct.Struct('<i4H6fH2s')
NAMES={f'{prefix}{i}' for prefix in ('engine_glow0','engine_glow_frigate0','r1moship','r2moship','p1moship') for i in range(4)}

def subdivide(data,smooth=False):
    data,_=translate(data)
    output=bytearray(data); h=HEADER.unpack_from(data); total=0
    for index in range(h[9]):
        obj=list(OBJECT.unpack_from(data,HEADER.size+index*OBJECT.size))
        nv,nf,nn,nt=obj[4:8]
        vertices=[VERTEX.unpack_from(data,obj[8]+i*16) for i in range(nv)]
        normals=[NORMAL.unpack_from(data,obj[9]+i*16) for i in range(nf+nn)]
        nozzle_z=max((v[2] for v in vertices),default=0)
        polygons=[]
        # Reuse geometric edge midpoints; UVs remain per-polygon, as in GEO.
        midpoints={}
        def midpoint(a,b):
            edge=tuple(sorted((a,b)))
            if edge not in midpoints:
                va,vb=vertices[a],vertices[b]; ni=-1
                point=[(va[j]+vb[j])*.5 for j in range(3)]
                if va[3]>=0 and vb[3]>=0:
                    xyz=[normals[va[3]][j]+normals[vb[3]][j] for j in range(3)]
                    length=math.sqrt(sum(v*v for v in xyz))
                    xyz=[v/length for v in xyz] if length>1e-12 else list(normals[va[3]][:3])
                    ni=len(normals);normals.append((*xyz,b'\0'*4))
                    if smooth:
                        # Cubic point-normal edge midpoint: round the surface,
                        # rather than just splitting its original flat facets.
                        na,nb=normals[va[3]][:3],normals[vb[3]][:3]
                        edge_vector=[vb[j]-va[j] for j in range(3)]
                        wa=sum(edge_vector[j]*na[j] for j in range(3))
                        wb=-sum(edge_vector[j]*nb[j] for j in range(3))
                        correction=[-(wa*na[j]+wb*nb[j])/8 for j in range(3)]
                        limit=math.sqrt(sum(v*v for v in edge_vector))*.125
                        magnitude=math.sqrt(sum(v*v for v in correction))
                        factor=min(1,limit/magnitude) if magnitude else 1
                        point=[point[j]+correction[j]*factor for j in range(3)]
                        # Never shift the attachment plane or bulge into hull.
                        point[2]=min(nozzle_z,point[2])
                        if abs(va[2]-nozzle_z)<1e-6 and abs(vb[2]-nozzle_z)<1e-6:point[2]=nozzle_z
                midpoints[edge]=len(vertices)
                vertices.append((*point,ni))
            return midpoints[edge]
        for i in range(nt):
            p=POLYGON.unpack_from(data,obj[10]+i*40)
            a,b,c=p[1:4]; ab,bc,ca=midpoint(a,b),midpoint(b,c),midpoint(c,a)
            uv=[p[5:7],p[7:9],p[9:11]]
            uv += [tuple((uv[a][j]+uv[b][j])*.5 for j in range(2)) for a,b in ((0,1),(1,2),(2,0))]
            for ids,tex in (((a,ab,ca),(0,3,5)),((ab,b,bc),(3,1,4)),((ca,bc,c),(5,4,2)),((ab,bc,ca),(3,4,5))):
                polygons.append((p[0],*ids,p[4],*[v for t in tex for v in uv[t]],p[11],p[12]))
        if len(vertices)>65535:raise ValueError('Subdivision exceeds GEO vertex index limit')
        obj[4]=len(vertices);obj[6]=len(normals)-nf;obj[7]=len(polygons)
        for slot,rows,codec in ((8,vertices,VERTEX),(9,normals,NORMAL),(10,polygons,POLYGON)):
            obj[slot]=len(output)
            for row in rows: output.extend(codec.pack(*row))
        OBJECT.pack_into(output,HEADER.size+index*OBJECT.size,*obj)
        total+=nt
    struct.pack_into('<I',output,16,len(output)-HEADER.size)
    result=bytes(output);translate(result) # Validate all resulting offsets/indices.
    return result,{'triangles_before':total,'triangles_after':total*4,'objects':h[9]}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--smooth',action='store_true',help='Curved point-normal subdivision of the plume contour')
    parser.add_argument('--passes',type=int,choices=(1,2),default=1)
    args=parser.parse_args();blob,entries=read_archive(args.archive);report={}
    for entry in entries:
        path=PureWindowsPath(entry.name.lower())
        if str(path.parent)!='misc\\engineglows' or path.suffix!='.geo' or path.stem not in NAMES:continue
        data=entry_bytes(blob,entry);stats=None
        for iteration in range(args.passes):
            data,current=subdivide(data,smooth=args.smooth)
            stats=current if stats is None else {**stats,'triangles_after':current['triangles_after']}
        stats.update(smooth=args.smooth,passes=args.passes)
        destination=args.output.joinpath(*path.parts);destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_bytes(data);report[path.name]=stats
    if len(report)!=20:raise ValueError('Expected all twenty active capital glow LOD meshes')
    (args.output/'subdivision-report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
