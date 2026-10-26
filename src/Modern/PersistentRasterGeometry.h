#pragma once
#include "PersistentMeshSource.h"
#include "../ThirdParty/meshoptimizer/src/meshoptimizer.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hwmodern {
struct PersistentRasterVertex {
    float position[4], color[4], uv[2], normal[4], fogFactor, clipDistance;
};
static_assert(sizeof(PersistentRasterVertex)==64,"Unchanged raster input layout");
struct PersistentRasterData {
    std::vector<PersistentRasterVertex> vertices;
    std::vector<unsigned int> indices;
};
template<class T> inline T persistentRead(const void *data,std::size_t offset) {
    T value;std::memcpy(&value,static_cast<const unsigned char *>(data)+offset,sizeof(T));return value;
}
inline bool validPersistentSource(const HWPersistentMeshSource &v) {
    if(!v.identity||!v.vertices||!v.normals||!v.polygons||!v.vertexCount||
       !v.normalCount||!v.polygonCount||v.polygonCount>1000000u||
       v.vertexStride<12||v.normalStride<12||v.polygonStride<8)return false;
    if(v.normalIndexOffset>v.vertexStride-4||v.faceNormalOffset>v.polygonStride-4||
       v.materialOffset>v.polygonStride-2)return false;
    for(unsigned i=0;i<3;++i)
        if(v.indexOffset[i]>v.polygonStride-2||v.uvOffset[i]>v.polygonStride-8)return false;
    return v.vertexStride<=4096&&v.normalStride<=4096&&v.polygonStride<=4096;
}
inline PersistentRasterData buildPersistentRaster(const HWPersistentMeshSource &v,
    unsigned first,unsigned count,bool smooth) {
    if(!validPersistentSource(v)||first>=v.polygonCount||!count||count>v.polygonCount-first)
        throw std::runtime_error("Invalid immutable mesh run");
    // A cache admission failure uses the existing immediate draw. Bound the
    // one-time remap scratch too; there is no geometry/texture quality policy.
    if(std::size_t(count)*3*sizeof(PersistentRasterVertex)>32u*1024u*1024u)
        throw std::runtime_error("Immutable mesh run exceeds cache scratch budget");
    std::vector<PersistentRasterVertex> expanded(std::size_t(count)*3);
    for(unsigned p=0;p<count;++p)for(unsigned corner=0;corner<3;++corner) {
        const std::size_t po=std::size_t(first+p)*v.polygonStride;
        const unsigned index=persistentRead<std::uint16_t>(v.polygons,po+v.indexOffset[corner]);
        if(index>=v.vertexCount)throw std::runtime_error("Immutable vertex index out of bounds");
        const auto no=smooth?persistentRead<std::int32_t>(v.vertices,std::size_t(index)*v.vertexStride+v.normalIndexOffset):
            persistentRead<std::int32_t>(v.polygons,po+v.faceNormalOffset);
        if(no<0||unsigned(no)>=v.normalCount)throw std::runtime_error("Immutable normal index out of bounds");
        auto &out=expanded[std::size_t(p)*3+corner];
        std::memcpy(out.position,static_cast<const unsigned char *>(v.vertices)+std::size_t(index)*v.vertexStride,12);
        out.position[3]=1;
        out.color[0]=out.color[1]=out.color[2]=out.color[3]=1;
        std::memcpy(out.normal,static_cast<const unsigned char *>(v.normals)+std::size_t(no)*v.normalStride,12);
        std::memcpy(out.uv,static_cast<const unsigned char *>(v.polygons)+po+v.uvOffset[corner],8);
        out.fogFactor=out.clipDistance=1;
    }
    std::vector<unsigned int> remap(expanded.size());
    const auto unique=meshopt_generateVertexRemap(remap.data(),nullptr,expanded.size(),
        expanded.data(),expanded.size(),sizeof(PersistentRasterVertex));
    PersistentRasterData result;result.vertices.resize(unique);result.indices.resize(expanded.size());
    meshopt_remapVertexBuffer(result.vertices.data(),expanded.data(),expanded.size(),sizeof(PersistentRasterVertex),remap.data());
    // This remaps values ONLY. It never changes triangle/corner order, winding,
    // degenerate triangles, alpha draw order, or provoking-vertex selection.
    meshopt_remapIndexBuffer(result.indices.data(),nullptr,expanded.size(),remap.data());
    return result;
}
inline void shadePersistentReference(const HWLegacyShadeState &s,const float normal[3],float out[4]) {
    float sums[3]={s.base[0],s.base[1],s.base[2]};
    for(unsigned light=0;light<unsigned(s.parameters[1]);++light) {
        const float n=normal[0]*s.direction[light][0]+normal[1]*s.direction[light][1]+normal[2]*s.direction[light][2];
        if(n>0)for(unsigned c=0;c<3;++c)sums[c]+=n*s.diffuse[light][c];
    }
    if(s.dock[3]!=0) {
        float t=1.1f-s.dock[3];if(t>1)t=1;
        for(unsigned c=0;c<3;++c){sums[c]*=t;sums[c]+=s.dock[3]*s.dock[c];}
    }
    for(unsigned c=0;c<3;++c)out[c]=std::min(sums[c],1.0f);
    out[3]=s.base[3];
}
}
