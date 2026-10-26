#include "RemasteredShipHod.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace {
struct Reader {
    const unsigned char *p; size_t n, at = 0;
    const unsigned char *take(size_t count) {
        if (count > n-at) throw std::runtime_error("truncated HOD field");
        const auto *v=p+at; at+=count; return v;
    }
    unsigned u32() { auto v=take(4); return unsigned(v[0])|(unsigned(v[1])<<8)|(unsigned(v[2])<<16)|(unsigned(v[3])<<24); }
    unsigned be32() { auto v=take(4); return (unsigned(v[0])<<24)|(unsigned(v[1])<<16)|(unsigned(v[2])<<8)|v[3]; }
    unsigned u16() { auto v=take(2); return unsigned(v[0])|(unsigned(v[1])<<8); }
    float f32() { unsigned bits=u32(); float v; std::memcpy(&v,&bits,4); if (!std::isfinite(v)) throw std::runtime_error("nonfinite HOD vertex"); return v; }
    std::string str() { unsigned len=u32(); if(len>4096) throw std::runtime_error("oversized HOD string"); auto s=take(len); std::string v((const char*)s,len); if(!v.empty() && v.back()==0) v.pop_back(); return v; }
    void done() { if(at!=n) throw std::runtime_error("unconsumed HOD fields"); }
};
struct Chunk { std::string tag; Reader body; };
std::vector<Chunk> chunks(Reader r) {
    std::vector<Chunk> out;
    while(r.at<r.n) {
        auto raw=r.take(4); std::string tag((const char*)raw,4); unsigned size=r.be32();
        if(size<4) throw std::runtime_error("invalid IFF size");
        Reader body{r.take(size),size};
        if(tag=="FORM" || tag=="NRML") { auto t=body.take(4); tag.assign((const char*)t,4); }
        out.push_back({tag,{body.p+body.at,body.n-body.at}});
        if(out.size()>10000) throw std::runtime_error("too many IFF chunks");
    }
    return out;
}
Reader find(const std::vector<Chunk>& list,const char* tag) {
    const Chunk *match=nullptr;
    for(const auto& c:list) if(c.tag==tag) { if(match) throw std::runtime_error("duplicate HOD chunk"); match=&c; }
    if(!match) throw std::runtime_error("missing HOD chunk"); return match->body;
}
std::vector<unsigned char> expand(Reader r,unsigned expected,unsigned limit=64*1024*1024) {
    if(expected>limit) throw std::runtime_error("oversized HOD pool");
    if(!expected) {
        if(r.n) throw std::runtime_error("nonempty compressed zero-size pool");
        return {};
    }
    if(r.n==expected) return {r.p,r.p+r.n};
    constexpr unsigned mask[]={255,65535,65535,16777215,255,65535,65535,0xffffffff};
    constexpr unsigned shift[]={2,2,6,8,2,2,6,11}, lm[]={0,0,15,31,0,0,15,255};
    constexpr unsigned ls[]={0,0,2,3,0,0,2,3}, consumed[]={1,2,2,3,1,2,2,4};
    std::vector<unsigned char> out; out.reserve(expected); unsigned flags=1;
    while(out.size()<expected) {
        if(flags==1) { flags=r.u32(); if(!(flags&0x80000000)) throw std::runtime_error("missing pool sentinel"); }
        if(!(flags&1)) out.push_back(*r.take(1));
        else {
            unsigned word=0; size_t available=std::min(size_t(4),r.n-r.at);
            for(size_t i=0;i<available;++i) word|=unsigned(r.p[r.at+i])<<(8*i);
            unsigned k=word&7, back=(word&mask[k])>>shift[k], count=((word>>ls[k])&lm[k])+3;
            r.take(consumed[k]);
            if(back>out.size() || count>expected-out.size()) throw std::runtime_error("invalid HOD pool match");
            size_t start=out.size()-back;
            for(unsigned i=0;i<count;++i) {
                if(!back) { out.push_back(0); continue; }
                ptrdiff_t source=ptrdiff_t(start)+i;
                if(back<4 && i>=3) source-=2+(back&1);
                if(source<0 || size_t(source)>=out.size()) throw std::runtime_error("invalid short pool match");
                out.push_back(out[size_t(source)]);
            }
        }
        flags>>=1;
    }
    return out;
}
void axis(float v[3],float scale) { float x=v[0]; v[0]=v[1]*scale; v[1]=-x*scale; v[2]*=scale; }
}

extern "C" void hwHodShipFree(HwHodShip *ship) {
    if(!ship) return; std::free(ship->vertices); std::free(ship->triangles); *ship={};
}
extern "C" int hwHodShipRead(const void *data,size_t size,HwHodShip *result,char *error,size_t errorSize) {
    if(!result) return 0; *result={};
    try {
        if(!data || size>64*1024*1024) throw std::runtime_error("invalid HOD input");
        auto top=chunks({(const unsigned char*)data,size});
        auto version=find(top,"VERS"); if(version.u32()!=0x200) throw std::runtime_error("not HOD2"); version.done();
        auto pools=find(top,"POOL"); pools.u32();
        std::array<std::vector<unsigned char>,3> stream;
        unsigned texturePoolSize=0;
        for(unsigned i=0;i<3;++i) {
            unsigned compressed=pools.u32(),expanded=pools.u32();
            if(expanded>64*1024*1024) throw std::runtime_error("oversized pool");
            Reader raw{pools.take(compressed),compressed};
            if(i) stream[i]=expand(raw,expanded); else texturePoolSize=expanded;
        }
        pools.done(); Reader vp{stream[1].data(),stream[1].size()}, ip{stream[2].data(),stream[2].size()};
        auto transform=chunks(find(top,"DTRM")); auto hierarchy=find(transform,"HIER");
        int joints=int(hierarchy.u32()); if(joints>=0 || joints < -100) throw std::runtime_error("invalid hierarchy count");
        bool hasMeshParent=false;
        for(int i=0;i<-joints;++i) {
            auto name=hierarchy.str(),parent=hierarchy.str();
            if(name=="HeavyInterceptor") hasMeshParent=true;
            // The first native test is static: reject translated/rotating joints
            // rather than silently applying an unsupported animation hierarchy.
            for(unsigned k=0;k<9;++k) {
                float value=hierarchy.f32();
                if((name=="Root" || name=="HeavyInterceptor") && k<6 && std::abs(value)>0.00001f) throw std::runtime_error("nonidentity interceptor hierarchy");
            }
            if(name=="HeavyInterceptor" && parent!="Root") throw std::runtime_error("unexpected interceptor parent");
        }
        hierarchy.done(); if(!hasMeshParent) throw std::runtime_error("not interceptor hierarchy");
        std::vector<std::string> shaders; unsigned textures=0; size_t textureBytes=0;
        std::vector<HwHodVertex> vertices; std::vector<HwHodTriangle> triangles; unsigned meshCount=0;
        for(auto c:chunks(find(top,"HVMD"))) {
            auto r=c.body;
            if(c.tag=="LMIP") {
                r.str(); auto fmt=r.take(4); std::string format((const char*)fmt,4); unsigned levels=r.u32();
                if(!levels || levels>16) throw std::runtime_error("invalid mip count");
                for(unsigned i=0;i<levels;++i) { unsigned w=r.u32(),h=r.u32(); if(!w||!h||w>16384||h>16384) throw std::runtime_error("invalid texture size");
                    if(format=="DXT1"||format=="DXT3"||format=="DXT5") textureBytes+=size_t((w+3)/4)*((h+3)/4)*(format=="DXT1"?8:16);
                    else if(format=="8888") textureBytes+=size_t(w)*h*4;
                    else throw std::runtime_error("unsupported texture format"); }
                r.done(); ++textures;
            } else if(c.tag=="STAT") {
                unsigned version=r.be32(); if(version!=1000 && version!=1001) throw std::runtime_error("unsupported material");
                r.str(); auto shader=r.str();
                if(shader!="ship" && shader!="thruster" && shader!="badge") throw std::runtime_error("unsupported ship shader");
                unsigned count=r.u32(); if(count>1024) throw std::runtime_error("too many material fields");
                for(unsigned i=0;i<count;++i) { unsigned type=r.u32(),len=r.u32(); Reader v{r.take(len),len}; r.str();
                    if(type==5 && (len!=4 || v.u32()>=textures)) throw std::runtime_error("invalid texture reference"); }
                r.done(); shaders.push_back(shader);
            } else if(c.tag=="MULT") {
                ++meshCount; unsigned ver=r.be32();
                if((ver!=1400 && ver!=1401) || r.str()!="HeavyInterceptor" || r.str()!="HeavyInterceptor" || r.u32()!=1) throw std::runtime_error("not the supported interceptor mesh");
                auto lods=chunks({r.p+r.at,r.n-r.at}); if(lods.size()!=1 || lods[0].tag!="BMSH") throw std::runtime_error("unsupported LOD container");
                auto b=lods[0].body;
                if(b.be32()!=1401 || b.u32()!=0) throw std::runtime_error("unsupported interceptor LOD");
                unsigned parts=b.u32(); if(parts!=3) throw std::runtime_error("unexpected interceptor parts");
                for(unsigned part=0;part<parts;++part) {
                    unsigned mat=b.u32(),mask=b.u32(),count=b.u32(),marker=b.u16(),indices=b.u32();
                    if(mat>=shaders.size() || mask!=0x601f || !count || count>65535 || marker!=65535 || indices%3 || indices>1000000 || vertices.size()+count>65535) throw std::runtime_error("unsupported interceptor vertex layout");
                    unsigned base=unsigned(vertices.size());
                    for(unsigned i=0;i<count;++i) {
                        HwHodVertex v{};
                        for(float& f:v.position) f=vp.f32(); vp.f32();
                        for(float& f:v.normal) f=vp.f32(); vp.f32(); vp.u32();
                        for(float& f:v.uv) f=vp.f32(); for(float& f:v.badgeUv) f=vp.f32();
                        for(unsigned k=0;k<6;++k) vp.f32(); // Tangent/binormal remain source-validated.
                        axis(v.position,2.885136604309082f); axis(v.normal,1);
                        float len=std::sqrt(v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2]);
                        if(len<0.00001f) throw std::runtime_error("zero vertex normal"); for(float& f:v.normal) f/=len;
                        vertices.push_back(v);
                    }
                    for(unsigned i=0;i<indices/3;++i) {
                        HwHodTriangle t{}; t.material=shaders[mat]=="thruster"?1:0;
                        for(auto& v:t.vertex) { unsigned index=ip.u16(); if(index>=count) throw std::runtime_error("invalid vertex index"); v=static_cast<unsigned short>(base+index); }
                        triangles.push_back(t);
                    }
                } b.done();
            } else throw std::runtime_error("unsupported interceptor HVMD block");
        }
        // Collision is validation-only. Gameplay retains original HW1 MEX.
        for(auto c:transform) if(c.tag=="COLD") {
            auto r=c.body; r.str();
            for(auto shape:chunks({r.p+r.at,r.n-r.at})) {
                auto s=shape.body;
                if(shape.tag=="TRIS") { unsigned count=s.u32(),indices=s.u32(); s.done();
                    if(count>65535 || indices>1000000 || indices%3) throw std::runtime_error("invalid collision counts");
                    for(unsigned i=0;i<count*3;++i) vp.f32();
                    for(unsigned i=0;i<indices;++i) if(ip.u16()>=count) throw std::runtime_error("invalid collision index");
                } else if(shape.tag=="BBOX" || shape.tag=="BSPH") {
                    unsigned fields=shape.tag=="BBOX"?6:4; for(unsigned i=0;i<fields;++i) s.f32(); s.done();
                } else throw std::runtime_error("unsupported collision shape");
            }
        }
        vp.done(); ip.done();
        if(meshCount!=1 || shaders.size()!=3 || textureBytes!=texturePoolSize || triangles.empty()) throw std::runtime_error("incomplete interceptor HOD");
        result->vertices=(HwHodVertex*)std::malloc(vertices.size()*sizeof(HwHodVertex));
        result->triangles=(HwHodTriangle*)std::malloc(triangles.size()*sizeof(HwHodTriangle));
        if(!result->vertices || !result->triangles) throw std::bad_alloc();
        std::memcpy(result->vertices,vertices.data(),vertices.size()*sizeof(HwHodVertex));
        std::memcpy(result->triangles,triangles.data(),triangles.size()*sizeof(HwHodTriangle));
        result->vertexCount=unsigned(vertices.size()); result->triangleCount=unsigned(triangles.size()); return 1;
    } catch(const std::exception& e) { hwHodShipFree(result); if(error && errorSize) std::snprintf(error,errorSize,"%s",e.what()); return 0; }
}

namespace {
struct JointStorage {
    std::string name, parent;
    float pose[9];
};
struct ParameterStorage {
    std::string name;
    unsigned type=0,texture=0;
    std::array<float,4> value{};
};
struct MaterialStorage {
    std::string name, shader;
    std::vector<ParameterStorage> parameters;
    std::vector<HwRmHodParameter> views;
};
struct TextureStorage {
    std::string name,format;
    std::vector<HwRmHodMip> mips;
};
struct PartStorage {
    unsigned material, mask;
    std::vector<HwHodVertex> vertices;
    std::vector<unsigned short> indices;
    std::vector<unsigned int> colors;
};
struct MeshStorage {
    std::string name, parent;
    unsigned lod;
    std::vector<PartStorage> parts;
    std::vector<HwRmHodPart> views;
};
struct DockPathStorage {
    std::string name, parent, compatibleShips, linkedPath;
    unsigned flags[5]{}, reserved=0;
    std::vector<HwRmHodDockPoint> points;
};
void sceneMessage(char *error, size_t size, const char *message) {
    if(error && size) std::snprintf(error,size,"%s",message);
}
}
struct HwRmHodScene {
    std::mutex textureMutex;
    std::vector<DockPathStorage> dockPaths;
    std::vector<HwRmHodDockPath> dockPathViews;
    std::vector<JointStorage> joints;
    std::vector<MaterialStorage> materials;
    std::vector<MeshStorage> meshes;
    std::vector<TextureStorage> textures;
    std::vector<HwRmHodTexture> textureViews;
    std::vector<unsigned char> compressedTextures,decodedTextures;
    size_t textureBytes=0;
    bool texturesDecoded=false;
    std::vector<HwRmHodJoint> jointViews;
    std::vector<HwRmHodMaterial> materialViews;
    std::vector<HwRmHodMesh> meshViews;
    size_t auxiliaryVertexBytes=0, auxiliaryIndexBytes=0;
};
extern "C" int hwRmHodSceneRead(const void *data,size_t size,HwRmHodScene **result,
                                 char *error,size_t errorSize) {
    if(!result) {sceneMessage(error,errorSize,"null HOD scene output");return 0;}
    *result=nullptr;
    try {
        if(!data || !size || size>64*1024*1024) throw std::runtime_error("invalid fleet HOD input");
        auto scene=std::make_unique<HwRmHodScene>();
        auto top=chunks({static_cast<const unsigned char*>(data),size});
        auto version=find(top,"VERS");
        if(version.u32()!=0x200) throw std::runtime_error("fleet reader requires HOD2");
        version.done();
        auto pool=find(top,"POOL");pool.u32();
        std::array<std::vector<unsigned char>,3> streams;
        size_t texturePoolBytes=0;
        for(unsigned i=0;i<3;++i) {
            const unsigned compressed=pool.u32(), expanded=pool.u32();
            if(expanded>(i?64u:512u)*1024*1024) throw std::runtime_error("oversized fleet HOD pool");
            Reader raw{pool.take(compressed),compressed};
            if(i) streams[i]=expand(raw,expanded); else {
                texturePoolBytes=expanded;scene->textureBytes=expanded;
                scene->compressedTextures.assign(raw.p,raw.p+raw.n);
            }
        }
        pool.done();
        Reader vp{streams[1].data(),streams[1].size()},ip{streams[2].data(),streams[2].size()};
        auto transform=chunks(find(top,"DTRM"));auto hierarchy=find(transform,"HIER");
        const int count=int(hierarchy.u32());
        if(count>=0 || count<-1024) throw std::runtime_error("invalid fleet hierarchy count");
        std::unordered_map<std::string,unsigned> jointIndices;
        std::unordered_set<std::string> ambiguousJoints;
        for(int i=0;i<-count;++i) {
            JointStorage joint;joint.name=hierarchy.str();joint.parent=hierarchy.str();
            if(joint.name.empty()) throw std::runtime_error("empty fleet joint");
            if(!jointIndices.emplace(joint.name,unsigned(i)).second) ambiguousJoints.insert(joint.name);
            for(float& v:joint.pose) v=hierarchy.f32();
            scene->joints.push_back(std::move(joint));
        }
        hierarchy.done();
        // A self-named identity child is a redundant export helper: both
        // candidate parent frames are exactly the same. Resolve only a pair
        // with ONE outer anchor and ONE all-zero child; arbitrary duplicate
        // names and non-neutral transforms remain ambiguous errors. This is
        // the same structural rule for carrier engines, frigate anchors and
        // missile nozzles, not a growing list of ship-name exceptions. Source
        // HOD bytes, outer marker name and every authored transform stay intact.
        const auto duplicateNames=ambiguousJoints;
        for(const auto& name:duplicateNames) {
            unsigned outer=0,inner=0,outers=0,inners=0;
            for(unsigned i=0;i<scene->joints.size();++i)if(scene->joints[i].name==name){
                if(scene->joints[i].parent==name){inner=i;++inners;}
                else {outer=i;++outers;}
            }
            const bool neutral=std::all_of(std::begin(scene->joints[inner].pose),std::end(scene->joints[inner].pose),[](float v){return v==0;});
            // Preserve the previously audited carrier-export handling. Its
            // three helpers carry real rotations, so it is NOT covered by the
            // neutral-frame rule and their transforms must not be discarded.
            const bool auditedCarrier=jointIndices.count("P1Mothership") &&
                (name=="Hardpoint_Engine_Position" || name=="Hardpoint_Engine_Direction" || name=="Hardpoint_Engine_Rest");
            if(outers!=1 || inners!=1 || (!neutral && !auditedCarrier))continue;
            const std::string unique=name+"_ExportHelper";
            if(jointIndices.count(unique))throw std::runtime_error("duplicate neutral export helper identity");
            scene->joints[inner].name=unique;
            jointIndices[name]=outer;jointIndices.emplace(unique,inner);ambiguousJoints.erase(name);
        }
        // HOD2 DOCK records: bounded extended metadata plus 64-byte points.
        // Preserve source data only; world-relative paths are not joint names.
        bool hasDock=false;
        for(const auto& block:transform) if(block.tag=="DOCK") {
            if(hasDock) throw std::runtime_error("duplicate fleet DOCK chunk");
            hasDock=true;auto r=block.body;unsigned pathCount=r.u32();
            if(pathCount>1024) throw std::runtime_error("oversized fleet docking paths");
            size_t totalPoints=0;std::unordered_set<std::string> names;
            for(unsigned i=0;i<pathCount;++i) {
                DockPathStorage path;path.name=r.str();path.parent=r.str();
                if(path.name.empty() || !names.insert(path.name).second)
                    throw std::runtime_error("invalid fleet docking path name");
                for(auto& flag:path.flags) flag=r.u32();
                path.compatibleShips=r.str();path.reserved=r.u32();path.linkedPath=r.str();
                unsigned points=r.u32();totalPoints+=points;
                if(points>4096 || totalPoints>16384 || points>(r.n-r.at)/64)
                    throw std::runtime_error("invalid fleet docking point count");
                for(unsigned p=0;p<points;++p) {
                    HwRmHodDockPoint point{};
                    for(float& v:point.position) v=r.f32();
                    for(float& v:point.rotation) v=r.f32();
                    point.tolerance=r.f32();point.maxSpeed=r.f32();
                    for(auto& flag:point.flags) flag=r.u32();
                    path.points.push_back(point);
                }
                scene->dockPaths.push_back(std::move(path));
            }
            r.done();
        }
        for(const auto& joint:scene->joints) {
            std::unordered_set<std::string> visited;
            const JointStorage *cursor=&joint;
            while(!cursor->parent.empty()) {
                if(!visited.insert(cursor->name).second) throw std::runtime_error("cyclic fleet hierarchy");
                const auto parent=jointIndices.find(cursor->parent);
                if(parent==jointIndices.end()) throw std::runtime_error("missing fleet joint parent");
                if(ambiguousJoints.count(cursor->parent)) throw std::runtime_error("ambiguous fleet joint parent");
                cursor=&scene->joints[parent->second];
            }
        }
        unsigned textureCount=0;size_t describedTextureBytes=0,totalVertices=0,totalIndices=0;
        std::unordered_set<std::string> lodNames;
        for(auto block:chunks(find(top,"HVMD"))) {
            auto r=block.body;
            if(block.tag=="LMIP") {
                TextureStorage texture;texture.name=r.str();
                texture.format.assign(reinterpret_cast<const char*>(r.take(4)),4);
                const auto& format=texture.format;
                const unsigned mips=r.u32();
                if(!mips || mips>16) throw std::runtime_error("invalid fleet mip count");
                for(unsigned m=0;m<mips;++m) {
                    size_t width=r.u32(),height=r.u32();
                    if(!width || !height || width>16384 || height>16384) throw std::runtime_error("invalid fleet texture dimensions");
                    const size_t offset=describedTextureBytes;
                    if(format=="8888") describedTextureBytes+=width*height*4;
                    else if(format=="DXT1" || format=="DXT3" || format=="DXT5")
                        describedTextureBytes+=((width+3)/4)*((height+3)/4)*(format=="DXT1"?8:16);
                    else throw std::runtime_error("unsupported fleet texture format");
                    if(describedTextureBytes>texturePoolBytes) throw std::runtime_error("fleet texture pool overrun");
                    texture.mips.push_back({unsigned(width),unsigned(height),offset,describedTextureBytes-offset});
                }
                scene->textures.push_back(std::move(texture));
                r.done();if(++textureCount>10000) throw std::runtime_error("too many fleet textures");
            } else if(block.tag=="STAT") {
                const unsigned materialVersion=r.be32();
                if(materialVersion!=1000 && materialVersion!=1001) throw std::runtime_error("unsupported fleet material version");
                MaterialStorage material;material.name=r.str();material.shader=r.str();
                unsigned parameters=r.u32();if(parameters>1024) throw std::runtime_error("oversized fleet material");
                for(unsigned p=0;p<parameters;++p) {
                    const unsigned type=r.u32(),len=r.u32();Reader value{r.take(len),len};
                    ParameterStorage parameter;parameter.name=r.str();parameter.type=type;
                    for(const auto& previous:material.parameters) if(previous.name==parameter.name)
                        throw std::runtime_error("duplicate fleet material parameter");
                    if(type==5) {
                        if(len!=4) throw std::runtime_error("invalid fleet material texture");
                        parameter.texture=value.u32();
                        if(parameter.texture>=textureCount) throw std::runtime_error("invalid fleet material texture");
                    } else if(type==4) {
                        if(len!=16) throw std::runtime_error("invalid fleet material color");
                        for(unsigned k=0;k<4;++k) parameter.value[k]=value.f32();
                    } else throw std::runtime_error("unsupported fleet material parameter");
                    value.done();
                    material.parameters.push_back(std::move(parameter));
                }
                r.done();scene->materials.push_back(std::move(material));
                if(scene->materials.size()>1024) throw std::runtime_error("too many fleet materials");
            } else if(block.tag=="MULT") {
                const unsigned multVersion=r.be32();
                if(multVersion!=1400 && multVersion!=1401) throw std::runtime_error("unsupported fleet MULT version");
                const auto name=r.str();auto parent=r.str();const unsigned lodCount=r.u32();
                // Audited RM FX exports name their neutral mesh parent "None".
                // Permit only a single, truly unposed Root. Never reinterpret
                // missing parents in a ship skeleton or an animated hierarchy.
                if((parent=="None" || parent=="NULL") && scene->joints.size()==1 && scene->joints[0].name=="Root" &&
                   scene->joints[0].parent.empty() &&
                   std::all_of(std::begin(scene->joints[0].pose),std::end(scene->joints[0].pose),[](float v){return v==0;}))
                    parent="Root";
                if(name.empty() || !jointIndices.count(parent) || ambiguousJoints.count(parent) || !lodCount || lodCount>16)
                    throw std::runtime_error("invalid fleet mesh parent/LOD count");
                unsigned foundLods=0;
                for(auto child:chunks({r.p+r.at,r.n-r.at})) {
                    auto b=child.body;
                    if(child.tag=="TAGS") {b.str();b.done();continue;}
                    if(child.tag!="BMSH") throw std::runtime_error("unsupported fleet MULT child");
                    const unsigned meshVersion=b.be32(),lod=b.u32(),parts=b.u32();
                    if((meshVersion!=1400 && meshVersion!=1401) || lod>15 || parts>1024 ||
                        !lodNames.insert(name+"\n"+std::to_string(lod)).second)
                        throw std::runtime_error("invalid/duplicate fleet BMSH");
                    MeshStorage mesh;mesh.name=name;mesh.parent=parent;mesh.lod=lod;
                    for(unsigned p=0;p<parts;++p) {
                        PartStorage part;part.material=b.u32();part.mask=b.u32();
                        const unsigned vertices=b.u32(),marker=b.u16(),indices=b.u32();
                        if(part.material>=scene->materials.size() || marker!=65535 || vertices>65536 ||
                            indices>1000000 || indices%3 || (!vertices && indices) ||
                            part.mask&~0x603f || !(part.mask&1) || (meshVersion==1400 && part.mask&48))
                            throw std::runtime_error("invalid fleet mesh part");
                        totalVertices+=vertices;totalIndices+=indices;
                        if(totalVertices>2000000 || totalIndices>6000000) throw std::runtime_error("oversized fleet geometry");
                        part.vertices.reserve(vertices);part.indices.reserve(indices);part.colors.reserve(vertices);
                        for(unsigned v=0;v<vertices;++v) {
                            HwHodVertex vertex{};
                            for(float& f:vertex.position) f=vp.f32();vp.u32();
                            if(part.mask&2) {for(float& f:vertex.normal) f=vp.f32();vp.u32();}
                            part.colors.push_back(part.mask&4?vp.u32():0xffffffffu);
                            if(part.mask&8) for(float& f:vertex.uv) f=vp.f32();
                            if(part.mask&16) for(float& f:vertex.badgeUv) f=vp.f32();
                            if(part.mask&32) {vp.u32();vp.u32();}
                            // Unused source tangent frames can contain NaNs on
                            // degenerate UV triangles (e.g. cloaked fighter).
                            // Bound and skip opaque fields; never publish them
                            // as normals/tangents to a shader.
                            if(part.mask&0x2000) for(unsigned k=0;k<3;++k) vp.u32();
                            if(part.mask&0x4000) for(unsigned k=0;k<3;++k) vp.u32();
                            part.vertices.push_back(vertex);
                        }
                        for(unsigned k=0;k<indices;++k) {
                            const unsigned index=ip.u16();
                            if(index>=vertices) throw std::runtime_error("invalid fleet vertex index");
                            part.indices.push_back(static_cast<unsigned short>(index));
                        }
                        mesh.parts.push_back(std::move(part));
                    }
                    b.done();scene->meshes.push_back(std::move(mesh));++foundLods;
                }
                if(foundLods!=lodCount) throw std::runtime_error("fleet LOD count mismatch");
            } else throw std::runtime_error("unsupported fleet HVMD block");
        }
        if(scene->meshes.empty() || scene->materials.empty() || describedTextureBytes!=texturePoolBytes)
            throw std::runtime_error("incomplete fleet render scene");
        scene->auxiliaryVertexBytes=vp.n-vp.at;scene->auxiliaryIndexBytes=ip.n-ip.at;
        // HODOR appends non-render SCAR/collision data to these pools. Never
        // consume those bytes as another mesh, or overwrite HW1 MEX collision.
        bool hasAuxiliary=false;
        for(const auto& block:transform) if(block.tag=="SCAR" || block.tag=="COLD" || block.tag=="KDOP") hasAuxiliary=true;
        if((scene->auxiliaryVertexBytes || scene->auxiliaryIndexBytes) && !hasAuxiliary)
            throw std::runtime_error("unexplained fleet pool tail");
        for(const auto& j:scene->joints) {
            HwRmHodJoint view{};view.name=j.name.c_str();view.parent=j.parent.c_str();
            std::memcpy(view.position,j.pose,12);std::memcpy(view.rotation,j.pose+3,12);std::memcpy(view.gimbalLimits,j.pose+6,12);
            scene->jointViews.push_back(view);
        }
        for(const auto& path:scene->dockPaths) {
            HwRmHodDockPath view{};
            view.name=path.name.c_str();view.parent=path.parent.c_str();
            view.compatibleShips=path.compatibleShips.c_str();view.linkedPath=path.linkedPath.c_str();
            std::copy(path.flags,path.flags+5,view.flags);view.reserved=path.reserved;
            view.pointCount=unsigned(path.points.size());view.points=path.points.data();
            scene->dockPathViews.push_back(view);
        }
        for(auto& m:scene->materials) {
            for(const auto& p:m.parameters) {
                HwRmHodParameter view{};view.name=p.name.c_str();view.type=p.type;view.texture=p.texture;
                std::copy(p.value.begin(),p.value.end(),view.value);m.views.push_back(view);
            }
            scene->materialViews.push_back({m.name.c_str(),m.shader.c_str(),unsigned(m.views.size()),m.views.data()});
        }
        for(const auto& t:scene->textures) scene->textureViews.push_back({t.name.c_str(),t.format.c_str(),unsigned(t.mips.size()),t.mips.data()});
        for(auto& m:scene->meshes) {
            for(const auto& p:m.parts) m.views.push_back({p.material,p.mask,unsigned(p.vertices.size()),
                unsigned(p.indices.size()),p.vertices.data(),p.indices.data(),p.colors.data()});
            scene->meshViews.push_back({m.name.c_str(),m.parent.c_str(),m.lod,unsigned(m.parts.size()),m.views.data()});
        }
        *result=scene.release();sceneMessage(error,errorSize,"");return 1;
    } catch(const std::exception& e) {sceneMessage(error,errorSize,e.what());return 0;}
}
extern "C" void hwRmHodSceneFree(HwRmHodScene *scene) {delete scene;}
extern "C" unsigned hwRmHodSceneDockPathCount(const HwRmHodScene *scene) {
    return scene?unsigned(scene->dockPathViews.size()):0;
}
extern "C" const HwRmHodDockPath *hwRmHodSceneDockPath(const HwRmHodScene *scene,unsigned index) {
    return scene && index<scene->dockPathViews.size()?&scene->dockPathViews[index]:nullptr;
}
extern "C" unsigned hwRmHodSceneJointCount(const HwRmHodScene *scene) {return scene?unsigned(scene->joints.size()):0;}
extern "C" unsigned hwRmHodSceneMeshCount(const HwRmHodScene *scene) {return scene?unsigned(scene->meshes.size()):0;}
extern "C" unsigned hwRmHodSceneMaterialCount(const HwRmHodScene *scene) {return scene?unsigned(scene->materials.size()):0;}
extern "C" const HwRmHodJoint *hwRmHodSceneJoint(const HwRmHodScene *scene,unsigned index) {
    return scene && index<scene->jointViews.size()?&scene->jointViews[index]:nullptr;
}
extern "C" const HwRmHodMesh *hwRmHodSceneMesh(const HwRmHodScene *scene,unsigned index) {
    return scene && index<scene->meshViews.size()?&scene->meshViews[index]:nullptr;
}
extern "C" const HwRmHodMaterial *hwRmHodSceneMaterial(const HwRmHodScene *scene,unsigned index) {
    return scene && index<scene->materialViews.size()?&scene->materialViews[index]:nullptr;
}
extern "C" unsigned hwRmHodSceneTextureCount(const HwRmHodScene *scene) {
    return scene?unsigned(scene->textureViews.size()):0;
}
extern "C" const HwRmHodTexture *hwRmHodSceneTexture(const HwRmHodScene *scene,unsigned index) {
    return scene && index<scene->textureViews.size()?&scene->textureViews[index]:nullptr;
}
extern "C" int hwRmHodSceneTextureData(HwRmHodScene *scene,const unsigned char **data,
    size_t *size,char *error,size_t errorSize) {
    if(data) *data=nullptr;if(size) *size=0;
    try {
        if(!scene || !data || !size) throw std::runtime_error("invalid HOD texture pool request");
        // Parsed models may be shared by independent LOD assets. Populate the
        // one mutable, lazy texture pool once; returned bytes never move again.
        std::lock_guard<std::mutex> lock(scene->textureMutex);
        if(!scene->texturesDecoded) {
            auto decoded=expand({scene->compressedTextures.data(),scene->compressedTextures.size()},
                unsigned(scene->textureBytes),512u*1024*1024);
            scene->decodedTextures.swap(decoded);scene->texturesDecoded=true;
            std::vector<unsigned char>().swap(scene->compressedTextures);
        }
        *data=scene->decodedTextures.data();*size=scene->decodedTextures.size();
        sceneMessage(error,errorSize,"");return 1;
    } catch(const std::exception& e) {sceneMessage(error,errorSize,e.what());return 0;}
}
extern "C" size_t hwRmHodSceneAuxiliaryVertexBytes(const HwRmHodScene *scene) {return scene?scene->auxiliaryVertexBytes:0;}
extern "C" int hwRmHodMaterialTexture(HwRmHodScene* scene,unsigned material,
    const char* parameter,unsigned mip,HwRmHodTextureView* view,char* error,size_t capacity){
    if(view)*view={};
    try{
        if(!view||!parameter)throw std::runtime_error("invalid native material texture request");
        const auto* source=hwRmHodSceneMaterial(scene,material);
        if(!source)throw std::runtime_error("native material index out of range");
        const HwRmHodParameter* selected=nullptr;
        for(unsigned i=0;i<source->parameterCount;++i)if(!strcmp(source->parameters[i].name,parameter)){
            if(selected)throw std::runtime_error("ambiguous native material parameter");selected=&source->parameters[i];
        }
        if(!selected||selected->type!=5)throw std::runtime_error("native texture parameter absent or not a texture");
        const auto* texture=hwRmHodSceneTexture(scene,selected->texture);
        if(!texture||mip>=texture->mipCount)throw std::runtime_error("native texture or mip out of range");
        const unsigned char* data=nullptr;size_t size=0;
        if(!hwRmHodSceneTextureData(scene,&data,&size,error,capacity))return 0;
        const auto& level=texture->mips[mip];
        if(level.offset>size||level.size>size-level.offset)throw std::runtime_error("native texture span out of range");
        *view={texture->name,texture->format,level.width,level.height,data+level.offset,level.size};
        sceneMessage(error,capacity,"");return 1;
    }catch(const std::exception& e){sceneMessage(error,capacity,e.what());return 0;}
}
extern "C" size_t hwRmHodSceneAuxiliaryIndexBytes(const HwRmHodScene *scene) {return scene?scene->auxiliaryIndexBytes:0;}
