#include "ShipAuthoringObj.h"
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
namespace {
using V=std::array<float,3>;using UV=std::array<float,2>;
int index(const std::string&s,size_t count){
 size_t used=0;long value=std::stol(s,&used);if(used!=s.size()||value==0)throw std::runtime_error("invalid OBJ index");
 long result=value>0?value-1:static_cast<long>(count)+value;
 if(result<0||static_cast<size_t>(result)>=count)throw std::runtime_error("OBJ index out of range");return int(result);
}
void normalized(V&v){float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(l<1e-12f)throw std::runtime_error("degenerate OBJ normal or triangle");for(auto &n:v)n/=l;}
}
extern "C" void hwAuthoringObjFree(HwAuthoringObj *obj){if(obj){free(obj->vertices);*obj={};}}
extern "C" int hwAuthoringObjRead(const void *data,size_t size,HwAuthoringObj *out,char *error,size_t errorSize){
 if(!out)return 0;*out={};
 try {
  if(!data||!size||size>32u*1024u*1024u||memchr(data,0,size))throw std::runtime_error("OBJ must be text, up to 32 MiB");
  std::istringstream input(std::string(static_cast<const char*>(data),size));std::string line;
  std::vector<V> positions,normals;std::vector<UV> uv;std::vector<HwAuthoringVertex> triangles;
  std::unordered_map<std::string,unsigned> materials;materials["default"]=0;
  std::vector<std::string> names{"default"};unsigned material=0;
  while(std::getline(input,line)) {
   if(line.size()>16384)throw std::runtime_error("OBJ line too long");
   std::istringstream fields(line);std::string tag;fields>>tag;if(tag.empty()||tag[0]=='#')continue;
   if(tag=="v"||tag=="vn"){
    V v;if(!(fields>>v[0]>>v[1]>>v[2]))throw std::runtime_error("invalid OBJ vector");
    for(float f:v)if(!std::isfinite(f)||std::fabs(f)>1e7f)throw std::runtime_error("OBJ coordinate outside limits");
    if(tag=="vn"){normalized(v);normals.push_back(v);}else positions.push_back(v);
   }else if(tag=="vt"){
    UV v;if(!(fields>>v[0]>>v[1])||!std::isfinite(v[0])||!std::isfinite(v[1]))throw std::runtime_error("invalid OBJ UV");uv.push_back(v);
   }else if(tag=="usemtl"){
    std::string name;fields>>name;if(name.empty()||name.size()>63)throw std::runtime_error("OBJ material name missing/too long");
    auto it=materials.find(name);if(it==materials.end()){if(names.size()>=HW_OBJ_MATERIALS)throw std::runtime_error("maximum 32 OBJ materials");material=unsigned(names.size());names.push_back(name);materials[name]=material;}else material=it->second;
   }else if(tag=="f"){
    std::vector<HwAuthoringVertex> polygon;std::vector<bool> authoredNormal;std::string token;
    while(fields>>token){if(token[0]=='#')break;if(polygon.size()>=256)throw std::runtime_error("maximum 256 corners per OBJ face");
     size_t first=token.find('/'),second=first==std::string::npos?first:token.find('/',first+1);
     HwAuthoringVertex v={};auto p=positions.at(index(token.substr(0,first),positions.size()));std::copy(p.begin(),p.end(),v.position);
     if(first!=std::string::npos){auto t=token.substr(first+1,second==std::string::npos?second:second-first-1);if(!t.empty()){auto q=uv.at(index(t,uv.size()));std::copy(q.begin(),q.end(),v.uv);}}
     bool n=second!=std::string::npos&&second+1<token.size();if(n){auto q=normals.at(index(token.substr(second+1),normals.size()));std::copy(q.begin(),q.end(),v.normal);}
     v.material=material;polygon.push_back(v);authoredNormal.push_back(n);
    }
    if(polygon.size()<3)throw std::runtime_error("OBJ face has fewer than three corners");
    for(size_t i=2;i<polygon.size();i++){
     size_t corners[]={0,i-1,i};V a,b,n;
     for(int k=0;k<3;k++){a[k]=polygon[i-1].position[k]-polygon[0].position[k];b[k]=polygon[i].position[k]-polygon[0].position[k];}
     n={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};normalized(n);
     for(auto c:corners){auto v=polygon[c];if(!authoredNormal[c])std::copy(n.begin(),n.end(),v.normal);triangles.push_back(v);}
     if(triangles.size()>1000000)throw std::runtime_error("maximum one million imported corners");
    }
   }
   if(positions.size()>1000000||normals.size()>1000000||uv.size()>1000000)throw std::runtime_error("OBJ attribute count exceeds limit");
  }
  if(triangles.empty())throw std::runtime_error("OBJ contains no renderable faces");
  out->vertices=static_cast<HwAuthoringVertex*>(malloc(triangles.size()*sizeof(HwAuthoringVertex)));if(!out->vertices)throw std::bad_alloc();
  memcpy(out->vertices,triangles.data(),triangles.size()*sizeof(HwAuthoringVertex));out->vertexCount=unsigned(triangles.size());out->materialCount=unsigned(names.size());
  for(size_t i=0;i<names.size();i++)snprintf(out->materials[i],64,"%s",names[i].c_str());return 1;
 }catch(const std::exception&e){hwAuthoringObjFree(out);if(error&&errorSize)snprintf(error,errorSize,"%s",e.what());return 0;}
}
