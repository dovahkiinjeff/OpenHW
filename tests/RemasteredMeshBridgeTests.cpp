#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
extern "C" {
#include "Memory.h"
#include "Vector.h"
#include "Mesh.h"
}
#include "RemasteredSubsystem.h"
#include "NativeShipHodFixture.h"
#include "TextureResolution.h"
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
#include "PersistentMeshSource.h"
static std::vector<HWPersistentMeshSource> registeredSources;
extern "C" void hwglRegisterImmutableMesh(const HWPersistentMeshSource *source) {
    // Capture the borrowed immutable descriptor without linking a renderer or
    // requiring a GPU for this native-loader/lifetime test.
    if(source && source->identity && source->polygonCount)
        registeredSources.push_back(*source);
}
#endif
static size_t allocationSize;
static unsigned char *allocation;
extern "C" void *memAllocFunction(sdword length,udword) {
    if(length<=0 || allocation) std::abort();
    allocationSize=size_t(length);allocation=(unsigned char*)std::calloc(allocationSize+16,1);
    if(!allocation) std::abort();std::memset(allocation+allocationSize,0xa7,16);return allocation;
}
extern "C" char *memStringDupe(char *s) {return _strdup(s);}
extern "C" void vecNormalize(vector *v) {
    const float length=std::sqrt(v->x*v->x+v->y*v->y+v->z*v->z);
    v->x/=length;v->y/=length;v->z/=length;
}
static void require(bool ok,const char *reason) {if(!ok) throw std::runtime_error(reason);}
static const char *material(void*,const HwRmSubsystemNode *node) {
    return std::strcmp(node->material->shader,"thruster")==0?"HWRM_Engine":"HWRM_Hull";
}
static const char *missingMaterial(void*,const HwRmSubsystemNode*) {return nullptr;}
static bool32 legacyBinding(udword,hmatrix*,hmatrix*,void*,smemsize) {return FALSE;}
static void within(const void *p,size_t bytes) {
    if(!p && !bytes) return;
    const auto *begin=static_cast<const unsigned char*>(p);
    require(begin>=allocation && begin<=allocation+allocationSize &&
        bytes<=size_t(allocation+allocationSize-begin),"mesh pointer outside allocation");
}
static void walk(meshdata *mesh,polygonobject *object,std::unordered_set<polygonobject*>& visited) {
    for(;object;object=object->pSister) {
        within(object,sizeof(*object));require(visited.insert(object).second,"cyclic/duplicated hierarchy link");
        require(object>=mesh->object && object<mesh->object+mesh->nPolygonObjects,"bad polygon object pointer");
        within(object->pName,std::strlen(object->pName)+1);
        within(object->pVertexList,object->nVertices*sizeof(vertexentry));
        within(object->pNormalList,(object->nFaceNormals+object->nVertexNormals)*sizeof(normalentry));
        within(object->pPolygonList,object->nPolygons*sizeof(polyentry));
        for(int i=0;i<object->nPolygons;++i) {
            const auto &p=object->pPolygonList[i];
            require(p.iV0<object->nVertices && p.iV1<object->nVertices && p.iV2<object->nVertices &&
                p.iMaterial<mesh->nPublicMaterials,"invalid copied triangle/material index");
        }
        for(auto *child=object->pDaughter;child;child=child->pSister)
            require(child->pMother==object,"child lost its native parent");
        walk(mesh,object->pDaughter,visited);
    }
}
static void verify(meshdata *mesh) {
    require(mesh && allocation==reinterpret_cast<unsigned char*>(mesh),"mesh allocation failed");
    std::unordered_set<polygonobject*> visited;walk(mesh,mesh->object,visited);
    require(visited.size()==size_t(mesh->nPolygonObjects),"unreachable native hierarchy node");
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    size_t expectedSources=0;
    for(unsigned oi=0;oi<mesh->nPolygonObjects;++oi) {
        const auto &object=mesh->object[oi];if(!object.nPolygons)continue;
        ++expectedSources;const HWPersistentMeshSource *found=nullptr;
        for(const auto &source:registeredSources)if(source.identity==object.pPolygonList) {
            require(!found,"native geometry registered more than once");found=&source;
        }
        require(found && found->vertices==object.pVertexList && found->normals==object.pNormalList &&
            found->polygons==object.pPolygonList,"immutable descriptor does not borrow native-owned geometry");
        require(found->vertexCount==unsigned(object.nVertices) &&
            found->normalCount==unsigned(object.nFaceNormals+object.nVertexNormals) &&
            found->polygonCount==unsigned(object.nPolygons),"immutable descriptor count mismatch");
        require(found->vertexStride==sizeof(vertexentry) && found->normalStride==sizeof(normalentry) &&
            found->polygonStride==sizeof(polyentry) && found->normalIndexOffset==offsetof(vertexentry,iVertexNormal) &&
            found->faceNormalOffset==offsetof(polyentry,iFaceNormal) && found->materialOffset==offsetof(polyentry,iMaterial),
            "immutable descriptor layout mismatch");
        for(unsigned corner=0;corner<3;++corner)
            require(found->indexOffset[corner]==offsetof(polyentry,iV0)+corner*sizeof(uword) &&
                found->uvOffset[corner]==offsetof(polyentry,s0)+corner*2*sizeof(real32),"immutable corner offset mismatch");
    }
    require(registeredSources.size()==expectedSources,"unexpected native geometry registration");
    registeredSources.clear();
#endif
    for(int i=0;i<mesh->nPublicMaterials;++i) {
        within(mesh->localMaterial+i,sizeof(materialentry));
        within(mesh->localMaterial[i].textureNameSave,std::strlen(mesh->localMaterial[i].textureNameSave)+1);
        require(!mesh->localMaterial[i].bTexturesRegistered,"bridge bypassed material registration");
    }
    for(unsigned i=0;i<16;++i) require(allocation[allocationSize+i]==0xa7,"native mesh allocation overrun");
#if MESH_RETAIN_FILENAMES
    std::free(mesh->fileName);
#endif
    std::free(allocation);allocation=nullptr;
}
static void compareInterceptor(meshdata *mesh,const Bytes& bytes) {
    HwHodShip old{};char error[256];
    require(hwHodShipRead(bytes.data(),bytes.size(),&old,error,sizeof(error))!=0,error);
    unsigned vertexBase=0,triangleBase=0;
    for(int oi=0;oi<mesh->nPolygonObjects;++oi) {
        const auto &object=mesh->object[oi];if(!object.nVertices) continue;
        for(auto *parent=&object;parent;parent=parent->pMother) {
            const float *m=reinterpret_cast<const float*>(&parent->localMatrix);
            for(unsigned k=0;k<16;++k) require(std::abs(m[k]-(k%5==0?1.f:0.f))<1e-6,"interceptor hierarchy changed rest transform");
        }
        for(int vi=0;vi<object.nVertices;++vi) {
            require(vertexBase+vi<old.vertexCount,"interceptor gained vertices");
            const float *v=&object.pVertexList[vi].x;
            for(unsigned axis=0;axis<3;++axis)
                require(std::abs(v[axis]-old.vertices[vertexBase+vi].position[axis])<1e-4,"interceptor bridge changed geometry/scale");
        }
        for(int pi=0;pi<object.nPolygons;++pi) {
            require(triangleBase+pi<old.triangleCount,"interceptor gained triangles");
            const auto &p=object.pPolygonList[pi];
            const auto &t=old.triangles[triangleBase+pi];
            require(t.vertex[0]==vertexBase+p.iV0 && t.vertex[1]==vertexBase+p.iV1 && t.vertex[2]==vertexBase+p.iV2,"interceptor triangle order changed");
            const char *label=t.material==1?"HWRM_Engine":"HWRM_Hull";
            require(std::strcmp(mesh->localMaterial[p.iMaterial].textureNameSave,label)==0,"interceptor badge/emission material changed");
            require(p.s0==old.vertices[t.vertex[0]].uv[0] && p.t0==old.vertices[t.vertex[0]].uv[1] &&
                p.s1==old.vertices[t.vertex[1]].uv[0] && p.t1==old.vertices[t.vertex[1]].uv[1] &&
                p.s2==old.vertices[t.vertex[2]].uv[0] && p.t2==old.vertices[t.vertex[2]].uv[1],"interceptor UVs changed");
        }
        vertexBase+=object.nVertices;triangleBase+=object.nPolygons;
    }
    require(vertexBase==old.vertexCount && triangleBase==old.triangleCount,"interceptor lost geometry");hwHodShipFree(&old);
}
static HwRmHodScene *loadScene(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);require(bool(file),"attachment HOD missing");
    Bytes bytes((std::istreambuf_iterator<char>(file)),{});HwRmHodScene *scene=nullptr;char error[512];
    require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);return scene;
}
static void auditFleet(const std::filesystem::path& root,const char *manifest,unsigned expected=32) {
    std::ifstream file(manifest);require(bool(file),"hierarchy audit input missing");
    HwRmSubsystemLayer *layer=nullptr;HwRmHodScene *hull=nullptr;
    std::vector<HwRmHodScene*> attachments;
    std::string line,name;unsigned ships=0;char error[512];
    while(std::getline(file,line)) {
        std::istringstream row(line);std::vector<std::string> fields;std::string field;
        while(std::getline(row,field,'\t')) fields.push_back(field);
        if(fields.size()==3 && fields[0]=="H") {
            require(!layer,"nested hierarchy audit ship");name=fields[1];
            hull=loadScene(root/fields[2]);layer=hwRmSubsystemLayerCreate(hull,error,sizeof(error));require(layer!=nullptr,error);
        } else if(fields.size()==5 && fields[0]=="A") {
            require(layer!=nullptr,"attachment without hull");
            auto *scene=loadScene(root/fields[4]);attachments.push_back(scene);
            require(hwRmSubsystemLayerAdd(layer,fields[1].c_str(),fields[2].c_str(),unsigned(std::stoul(fields[3])),scene,error,sizeof(error))!=0,error);
        } else if(fields.size()==1 && fields[0]=="E") {
            require(layer!=nullptr,"audit terminator without hull");
            auto *instance=hwRmSubsystemInstanceCreate(layer);require(instance!=nullptr,"fleet instance allocation");
            for(unsigned lod=0;lod<5;++lod) {
                auto *mesh=meshCreateRemasteredHierarchy(instance,lod,2.5f,"audit.geo",material,nullptr,nullptr);
                if(!mesh) std::fprintf(stderr,"Full attached hierarchy rejected: R1/%s LOD%u\n",name.c_str(),lod);
                require(mesh!=nullptr,"fleet attachment bridge failed");verify(mesh);
            }
            hwRmSubsystemInstanceFree(instance);hwRmSubsystemLayerFree(layer);layer=nullptr;
            hwRmHodSceneFree(hull);hull=nullptr;for(auto *scene:attachments) hwRmHodSceneFree(scene);attachments.clear();++ships;
        } else throw std::runtime_error("malformed hierarchy audit row");
    }
    require(!layer && ships==expected,"incomplete full hierarchy audit");
    std::printf("All %u hull identities and visible attachments passed LOD0-4 renderer bridge audit\n",ships);
}
#include "RemasteredEmission.h"
int main(int argc,char **argv) {
    require(hwTextureResolutionCap(0)==512 && hwTextureResolutionCap(1)==1024 &&
        hwTextureResolutionCap(2)==2048 && hwTextureResolutionCap(3)==0 &&
        hwTextureResolutionCap(-1)==0, "texture resolution caps");
    unsigned int textureWidth=2048,textureHeight=4096;
    hwTextureResolutionDimensions(&textureWidth,&textureHeight,1024);
    require(textureWidth==512 && textureHeight==1024,"texture aspect ratio changed");
    hwTextureResolutionDimensions(&textureWidth,&textureHeight,2048);
    require(textureWidth==512 && textureHeight==1024,"texture upsampled");
    textureWidth=4096;textureHeight=2048;
    hwTextureResolutionDimensions(&textureWidth,&textureHeight,0);
    require(textureWidth==4096 && textureHeight==2048,"source resolution reduced");
    const unsigned char black[]={0,0,0,255},glow[]={20,50,80,255},dim[]={1,0,0,255},partial[]={20,50,80,80};
    require(meshRemasteredEmissionCoverage(black,255)==0,"black hull protected from lighting");
    require(meshRemasteredEmissionCoverage(glow,255)==80,"glow protection must follow brightness");
    require(meshRemasteredEmissionCoverage(dim,255)==1,"dim glow must not exempt opaque hull");
    require(meshRemasteredEmissionCoverage(glow,0)==0,"cutout emits");
    require(meshRemasteredEmissionCoverage(partial,100)==25,"authored alpha must limit glow protection");
    const unsigned char bright[]={255,200,100,255};
    require(meshRemasteredEmissionCoverage(bright,255)==255,"bright lamp protection lost");
    try {
        char error[512];HwRmHodScene *scene=nullptr;auto bytes=sample();
        for(unsigned tint:{0xc8c8c8ffu,0xff0000ffu,0xc8c8c800u}) {
            auto tinted=sample(false,false,false,true,tint);
            require(hwRmHodSceneRead(tinted.data(),tinted.size(),&scene,error,sizeof(error))!=0,error);
            auto *tintLayer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(tintLayer!=nullptr,error);
            auto *tintInstance=hwRmSubsystemInstanceCreate(tintLayer);require(tintInstance!=nullptr,"tint instance missing");
            auto *tintMesh=meshCreateRemasteredHierarchy(tintInstance,0,2,"tint.geo",material,nullptr,nullptr);
            if((tint&255)==255) {
                require(tintMesh!=nullptr,"authored uniform opaque tint rejected");
                const auto &poly=tintMesh->object[2].pPolygonList[0];
                require(poly.s0==((float(tint>>24)+.5f)/256) && poly.s1==poly.s0 && poly.s2==poly.s0 &&
                    poly.t0==.5f && poly.t1==.5f && poly.t2==.5f,"vertex tint ramp coordinates lost");
                verify(tintMesh);
            } else require(!tintMesh && !allocation,"unsupported tint silently became an opaque surface");
            hwRmSubsystemInstanceFree(tintInstance);hwRmSubsystemLayerFree(tintLayer);hwRmHodSceneFree(scene);scene=nullptr;
        }
        {
            auto proxyBytes=sample(false,false,true);
            require(hwRmHodSceneRead(proxyBytes.data(),proxyBytes.size(),&scene,error,sizeof(error))!=0,error);
            auto *proxyLayer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(proxyLayer!=nullptr,error);
            auto *proxy=hwRmSubsystemInstanceCreate(proxyLayer);require(proxy!=nullptr,"proxy hierarchy instance missing");
            auto *proxyMesh=meshCreateRemasteredHierarchy(proxy,0,2,"proxy.geo",material,nullptr,nullptr);
            require(proxyMesh && proxyMesh->nPolygonObjects==4 && proxyMesh->nPublicMaterials==2,"proxy filtering lost markers or retained geometry");verify(proxyMesh);
            hwRmSubsystemInstanceFree(proxy);hwRmSubsystemLayerFree(proxyLayer);hwRmHodSceneFree(scene);scene=nullptr;
        }
        require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);
        auto *layer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(layer!=nullptr,error);
        auto *instance=hwRmSubsystemInstanceCreate(layer);require(instance!=nullptr,"instance allocation");
        require(!meshCreateRemasteredHierarchy(instance,0,2,"test.geo",missingMaterial,nullptr,nullptr) && !allocation,"missing material allocated incomplete ship");
        auto *mesh=meshCreateRemasteredHierarchy(instance,0,2,"test.geo",material,nullptr,nullptr);
        require(mesh && mesh->nPolygonObjects==5 && mesh->nPublicMaterials==3,"native hierarchy flattened or lost a part");
        require(mesh->object[2].pMother==&mesh->object[1] && mesh->object[2].pVertexList[1].y==-2,"native pivot/axis scale mismatch");
        // The legacy mesh must not retain pointers to source scene/layer storage.
        hwRmSubsystemInstanceFree(instance);hwRmSubsystemLayerFree(layer);hwRmHodSceneFree(scene);
        verify(mesh);
        require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);
        layer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(layer!=nullptr,error);
        require(hwRmSubsystemLayerAdd(layer,"GunA","HeavyInterceptor",HW_RM_TURRET,scene,error,sizeof(error))!=0,error);
        require(hwRmSubsystemLayerAdd(layer,"GunB","HeavyInterceptor",HW_RM_TURRET,scene,error,sizeof(error))!=0,error);
        instance=hwRmSubsystemInstanceCreate(layer);require(instance!=nullptr,"attached instance allocation");
        mesh=meshCreateRemasteredHierarchy(instance,0,2,"test.geo",material,nullptr,nullptr);
        require(mesh && mesh->nPolygonObjects==15 && mesh->nPublicMaterials==9 &&
            mesh->object[5].pMother==&mesh->object[1] && mesh->object[10].pMother==&mesh->object[1],"shared turret components collapsed");
        std::vector<mhlocalbinding> bindings(mesh->nPolygonObjects);
        require(meshApplyRemasteredHierarchyPose(instance,0,2,mesh,bindings.data(),unsigned(bindings.size()))!=0,"native binding publication failed");
        require(!meshApplyRemasteredHierarchyPose(instance,0,2,mesh,bindings.data(),unsigned(bindings.size())-1),"accepted truncated binding table");
        bindings.back().function=legacyBinding;
        const auto untouched=bindings;
        require(!meshApplyRemasteredHierarchyPose(instance,0,2,mesh,bindings.data(),unsigned(bindings.size())),"clobbered existing weapon callback");
        require(std::memcmp(untouched.data(),bindings.data(),bindings.size()*sizeof(mhlocalbinding))==0,"invalid binding request partially published");
        verify(mesh);
        require(hwRmSubsystemSetVisible(instance,1,0)!=0,"hide turret failed");
        mesh=meshCreateRemasteredHierarchy(instance,0,2,"test.geo",material,nullptr,nullptr);
        require(mesh && mesh->nPolygonObjects==12 && mesh->nPublicMaterials==6,"hidden component retained visible geometry");verify(mesh);
        hwRmSubsystemInstanceFree(instance);hwRmSubsystemLayerFree(layer);hwRmHodSceneFree(scene);
        if(argc==3 && std::strcmp(argv[1],"--taiidan")==0) {
            const auto root=std::filesystem::path(argv[2]);
            const auto audit=(root/"hierarchy-audit.tsv").string();
            auditFleet(root/"source",audit.c_str(),29);return 0;
        }
        if(argc==3 && std::strcmp(argv[1],"--raiders-derelicts")==0) {
            const auto root=std::filesystem::path(argv[2]);
            const auto audit=(root/"hierarchy-audit-all.tsv").string();
            auditFleet(root/"source",audit.c_str(),46);
            return 0;
        }
        if(argc>1) for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[1])) {
            if(!entry.is_regular_file() || entry.path().extension()!=".hod" || entry.path().stem()=="kus_headshotasteroid") continue;
            std::ifstream file(entry.path(),std::ios::binary);bytes=Bytes((std::istreambuf_iterator<char>(file)),{});
            require(hwRmHodSceneRead(bytes.data(),bytes.size(),&scene,error,sizeof(error))!=0,error);
            layer=hwRmSubsystemLayerCreate(scene,error,sizeof(error));require(layer!=nullptr,error);
            instance=hwRmSubsystemInstanceCreate(layer);require(instance!=nullptr,"real instance allocation");
            for(unsigned lod=0;lod<5;++lod) {
                mesh=meshCreateRemasteredHierarchy(instance,lod,2.5f,"native.geo",material,nullptr,nullptr);
                if(!mesh) std::fprintf(stderr,"Source renderer bridge: %s LOD%u\n",entry.path().string().c_str(),lod);
                require(mesh!=nullptr,"real source failed native renderer bridge");verify(mesh);
            }
            if(entry.path().stem()=="kus_interceptor") {
                mesh=meshCreateRemasteredHierarchy(instance,0,2.885136604309082f,"native.geo",material,nullptr,nullptr);
                require(mesh!=nullptr,"interceptor bridge allocation");compareInterceptor(mesh,bytes);verify(mesh);
            }
            if(entry.path().stem()=="kus_mothership") {
                auto madPath=entry.path();madPath.replace_extension(".mad");
                std::ifstream madFile(madPath,std::ios::binary);Bytes madBytes((std::istreambuf_iterator<char>(madFile)),{});HwRmMad *mad=nullptr;
                require(hwRmMadRead(madBytes.data(),madBytes.size(),&mad,error,sizeof(error))!=0,error);
                unsigned clipIndex=0;while(clipIndex<hwRmMadClipCount(mad) && std::strcmp(hwRmMadClip(mad,clipIndex)->name,"Open")) ++clipIndex;
                require(clipIndex<hwRmMadClipCount(mad),"native door clip missing");
                auto *other=hwRmSubsystemInstanceCreate(layer);require(other!=nullptr,"second native ship instance");
                mesh=meshCreateRemasteredHierarchy(instance,0,2.5f,"native.geo",material,nullptr,nullptr);require(mesh!=nullptr,"native mothership mesh");
                std::vector<hmatrix> staticMatrices;for(int i=0;i<mesh->nPolygonObjects;++i) staticMatrices.push_back(mesh->object[i].localMatrix);
                std::vector<mhlocalbinding> first(mesh->nPolygonObjects),second(mesh->nPolygonObjects);
                require(meshApplyRemasteredHierarchyPose(other,0,2.5f,mesh,second.data(),unsigned(second.size()))!=0,"second ship pose bridge");
                const auto secondRest=second;
                require(hwRmSubsystemSampleAnimation(instance,0,mad,clipIndex,.5,error,sizeof(error))!=0,error);
                require(meshApplyRemasteredHierarchyPose(instance,0,2.5f,mesh,first.data(),unsigned(first.size()))!=0,"animated door binding bridge");
                bool moved=false;
                for(size_t i=0;i<first.size();++i) {
                    moved|=std::memcmp(&first[i].matrix,&second[i].matrix,sizeof(hmatrix))!=0;
                    require(std::memcmp(&mesh->object[i].localMatrix,&staticMatrices[i],sizeof(hmatrix))==0,"animation changed shared mesh rest pose");
                }
                require(moved,"native door sample didn't reach renderer bindings");
                require(std::memcmp(second.data(),secondRest.data(),second.size()*sizeof(mhlocalbinding))==0,"pose update leaked into other ship bindings");
                hwRmMadFree(mad);hwRmSubsystemInstanceFree(other);verify(mesh);
            }
            hwRmSubsystemInstanceFree(instance);hwRmSubsystemLayerFree(layer);hwRmHodSceneFree(scene);
        }
        if(argc>2) {
            auto root=std::filesystem::path(argv[1]);if(root.filename()=="ship") root=root.parent_path();
            auditFleet(root,argv[2]);
        }
        std::puts("Native hierarchy renderer bridge tests passed");return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
