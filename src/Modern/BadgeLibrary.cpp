#define NOMINMAX
#include "BadgeLibrary.h"
#include "ModernGraphics.h"
#include "../ThirdParty/stb/stb_image.h"
#include <SDL2/SDL.h>
#include <windows.h>
#include <commdlg.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <regex>
#include <array>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cstdlib>
namespace {
namespace fs=std::filesystem;
constexpr size_t maxFile=16*1024*1024;
struct Record {std::string id,label;fs::path path;std::vector<unsigned char> pixels;unsigned width=0,height=0;};
std::mutex guard;std::map<std::string,std::unique_ptr<Record>> records;std::vector<Record*> catalog;
std::array<Record*,8> remote{};std::thread scan;std::atomic<bool> scanning=false,stopping=false;
fs::path root;std::string selected,status="No badge selected";bool initialized=false;
std::vector<unsigned char> read(const fs::path &p,size_t cap=maxFile){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)return {};auto n=f.tellg();if(n<=0||uint64_t(n)>cap)return {};std::vector<unsigned char>b(size_t(n),0);f.seekg(0);if(!f.read((char*)b.data(),n))return {};return b;}
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
void copy(char *out,size_t cap,const std::string&s){if(out&&cap)std::snprintf(out,cap,"%s",s.c_str());}
std::string digest(const std::vector<unsigned char>&b){BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char result[32];std::string out;
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 if(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0){if(BCryptHashData(h,(PUCHAR)b.data(),ULONG(b.size()),0)>=0&&BCryptFinishHash(h,result,32,0)>=0){char hex[3];for(auto c:result){std::snprintf(hex,3,"%02x",c);out+=hex;}}BCryptDestroyHash(h);}BCryptCloseAlgorithmProvider(a,0);return out;}
uint32_t u32(const unsigned char*p);
bool decode(const std::vector<unsigned char>&b,Record&r){unsigned char *p=nullptr;bool dds=b.size()>=4&&!std::memcmp(b.data(),"DDS ",4);unsigned w=0,h=0;
 if(dds){if(b.size()<128||!u32(b.data()+12)||!u32(b.data()+16)||u32(b.data()+12)>4096||u32(b.data()+16)>4096||u32(b.data()+24)>1||(u32(b.data()+112)&0xfe00u))return false;
 if(u32(b.data()+84)==0x30315844u&&(b.size()<148||u32(b.data()+132)!=3||u32(b.data()+140)!=1||(u32(b.data()+136)&4u)))return false;
 if(!hwModernGraphicsDecodeDds(b.data(),unsigned(b.size()),&w,&h,&p))return false;}
 else {int x=0,y=0,c=0;if(!stbi_info_from_memory(b.data(),int(b.size()),&x,&y,&c)||x<1||y<1||x>4096||y>4096)return false;p=stbi_load_from_memory(b.data(),int(b.size()),&x,&y,&c,4);w=x;h=y;}
 if(!p||!w||!h||w>4096||h>4096){if(p){if(dds)hwModernGraphicsFreeDecodedDds(p);else stbi_image_free(p);}return false;}
 // Keep aspect ratio in a transparent square; cap decoded badge residency.
 r.width=r.height=256;r.pixels.assign(256*256*4,0);unsigned ow=256,oh=256;if(w>h)oh=std::max(1u,256*h/w);else ow=std::max(1u,256*w/h);
 for(unsigned y=0;y<oh;y++)for(unsigned x=0;x<ow;x++){unsigned sx=std::min(w-1,x*w/ow),sy=std::min(h-1,y*h/oh);std::memcpy(&r.pixels[(((y+(256-oh)/2)*256)+(x+(256-ow)/2))*4],p+(sy*w+sx)*4,4);}
 if(dds)hwModernGraphicsFreeDecodedDds(p);else stbi_image_free(p);return true;}
std::string importBytes(const std::vector<unsigned char>&b,const std::string&label,const std::string&extension){if(b.empty()||b.size()>maxFile)return {};auto id=digest(b);if(id.empty())return {};
 {std::lock_guard lock(guard);auto existing=records.find(id);if(existing!=records.end()){
   if(label.find("Workshop")!=std::string::npos&&existing->second->label.find("Workshop")==std::string::npos){existing->second->label=(existing->second->label+" + "+label.substr(label.find("Workshop"))).substr(0,240);std::ofstream note(root/(id+".name"),std::ios::binary);note<<existing->second->label;}return id;}}
 Record check;if(!decode(b,check))return {};auto item=std::make_unique<Record>();item->id=id;item->label=label.substr(0,160);std::replace(item->label.begin(),item->label.end(),'\n',' ');std::replace(item->label.begin(),item->label.end(),'\r',' ');item->path=root/(id+extension);
 fs::create_directories(root);auto temp=item->path;temp+=".pending";{std::ofstream f(temp,std::ios::binary);f.write((const char*)b.data(),b.size());if(!f)return {};}std::error_code ec;fs::rename(temp,item->path,ec);if(ec){fs::remove(temp,ec);if(!fs::exists(item->path))return {};}
 {std::ofstream f(root/(id+".name"),std::ios::binary);f<<item->label;}
 std::lock_guard lock(guard);if(!records.count(id)){catalog.push_back(item.get());records[id]=std::move(item);}return id;}
bool badgeName(const std::string&name){auto n=lower(name);auto ext=lower(fs::path(n).extension().string());return (ext==".png"||ext==".dds"||ext==".tga")&&(n.find("badges/")!=std::string::npos||n.find("badge/")!=std::string::npos);}
uint32_t u32(const unsigned char*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const unsigned char*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
uint32_t crc(const std::vector<unsigned char>&b){uint32_t c=~0u;for(auto v:b){c^=v;for(int i=0;i<8;i++)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
void archive(const fs::path&path,const std::string&source){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return;uint64_t fileSize=uint64_t(f.tellg());std::array<unsigned char,180>h{};f.seekg(0);if(!f.read((char*)h.data(),h.size())||std::memcmp(h.data(),"_ARCHIVE\2\0\0\0",12))return;
 uint32_t size=u32(h.data()+172),base=u32(h.data()+176);if(size<24||size>32*1024*1024||uint64_t(base)<180ull+size||base>fileSize)return;std::vector<unsigned char>m(size);if(!f.read((char*)m.data(),size))return;
 auto table=[&](unsigned offset,unsigned count,unsigned stride){return offset>=24&&uint64_t(offset)+uint64_t(count)*stride<=size;};unsigned folders=u32(m.data()+6),nf=u16(m.data()+10),files=u32(m.data()+12),n=u16(m.data()+16),strings=u32(m.data()+18);
 if(!table(folders,nf,12)||!table(files,n,17)||strings<24||strings>=size)return;
 auto text=[&](unsigned offset)->std::string{uint64_t start=uint64_t(strings)+offset;if(start>=size)return {};size_t end=size_t(start);while(end<size&&end-start<4096&&m[end])end++;if(end==size||end-start>=4096)return {};return std::string((char*)m.data()+start,end-start);};
 std::vector<std::string> owners(n);for(unsigned i=0;i<nf;i++){auto p=m.data()+folders+i*12;unsigned first=u16(p+8),last=u16(p+10);if(first>last||last>n)return;auto folder=text(u32(p));for(unsigned j=first;j<last;j++)owners[j]=folder;}
 for(unsigned i=0;i<n&&!stopping;i++){auto p=m.data()+files+i*17;auto name=owners[i]+"/"+text(u32(p));std::replace(name.begin(),name.end(),'\\','/');if(!badgeName(name))continue;unsigned flags=p[4],offset=u32(p+5),stored=u32(p+9),expanded=u32(p+13);uint64_t start=uint64_t(base)+offset;
  if(offset<264||!stored||stored>maxFile||!expanded||expanded>maxFile||start+stored>fileSize||(flags!=0&&flags!=0x10&&flags!=0x20))continue;
  std::vector<unsigned char>b(stored);unsigned char expected[4];f.clear();f.seekg(start-4);if(!f.read((char*)expected,4)||!f.read((char*)b.data(),stored))continue;
  if(flags){std::vector<unsigned char>out(expanded);if(stbi_zlib_decode_buffer((char*)out.data(),int(expanded),(const char*)b.data(),int(stored))!=int(expanded))continue;b=std::move(out);}if(b.size()!=expanded||crc(b)!=u32(expected))continue;
  importBytes(b,fs::path(name).stem().string()+" / "+source,lower(fs::path(name).extension().string()));
 }
}
std::vector<fs::path> steamLibraries(){std::vector<fs::path> result;wchar_t buffer[4096];DWORD bytes=sizeof(buffer);if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,buffer,&bytes)==ERROR_SUCCESS)result.emplace_back(buffer);
 if(result.empty()){const wchar_t*p=_wgetenv(L"ProgramFiles(x86)");if(p)result.push_back(fs::path(p)/"Steam");}
 auto initial=result;for(const auto&steam:initial){auto b=read(steam/"steamapps/libraryfolders.vdf",4*1024*1024);std::string v(b.begin(),b.end());std::regex pattern("\\\"path\\\"\\s*\\\"([^\\\"]+)\\\"");for(auto i=std::sregex_iterator(v.begin(),v.end(),pattern);i!=std::sregex_iterator();++i){auto value=(*i)[1].str();for(size_t p=0;(p=value.find("\\\\",p))!=std::string::npos;)value.replace(p,2,"\\");result.emplace_back(value);}}
 std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;}
void scanDirectory(const fs::path&directory,const std::string&source,bool archives){std::error_code ec;if(!fs::is_directory(directory,ec))return;size_t seen=0;fs::recursive_directory_iterator it(directory,fs::directory_options::skip_permission_denied,ec),end;
 for(;it!=end&&!stopping&&seen++<200000;it.increment(ec)){if(ec){ec.clear();continue;}if(it.depth()>12){it.disable_recursion_pending();continue;}if(it->is_symlink(ec)){it.disable_recursion_pending();continue;}if(!it->is_regular_file(ec))continue;auto p=it->path();auto name=p.generic_u8string();auto ext=lower(p.extension().string());try{if(badgeName(name))importBytes(read(p),p.stem().u8string()+" / "+source,ext);else if(archives&&ext==".big")archive(p,source+" / "+p.stem().u8string());}catch(const std::exception&){} }
}
}
extern "C" void hwBadgeInit(void){std::lock_guard lock(guard);if(initialized)return;initialized=true;std::atexit(hwBadgeShutdown);char*b=SDL_GetBasePath();root=(b?fs::u8path(b):fs::current_path())/"Badges";if(const char*overrideRoot=std::getenv("OPENHW_BADGE_ROOT"))root=fs::u8path(overrideRoot);if(b)SDL_free(b);fs::create_directories(root);std::error_code ec;
 for(auto&entry:fs::directory_iterator(root,ec)){auto p=entry.path();auto ext=lower(p.extension().string());auto id=p.stem().string();if(id.size()!=64||(ext!=".png"&&ext!=".dds"&&ext!=".tga")||!std::all_of(id.begin(),id.end(),[](char c){return std::isxdigit((unsigned char)c)!=0;}))continue;auto item=std::make_unique<Record>();item->id=id;item->path=p;auto b=read(root/(id+".name"),1024);item->label=b.empty()?id.substr(0,12):std::string(b.begin(),b.end());catalog.push_back(item.get());records[id]=std::move(item);}
 auto bsel=read(root/"selected.txt",128);selected.assign(bsel.begin(),bsel.end());if(!records.count(selected))selected.clear();status=std::to_string(catalog.size())+" badges in permanent library";
}
extern "C" void hwBadgeShutdown(void){stopping=true;if(scan.joinable())scan.join();}
extern "C" void hwBadgeDiscover(void){hwBadgeInit();if(scanning.exchange(true))return;if(scan.joinable())scan.join();stopping=false;{std::lock_guard lock(guard);status="Discovering Remastered and installed Workshop badges...";}scan=std::thread([]{try{for(const auto&library:steamLibraries()){scanDirectory(library/"steamapps/common/Homeworld","Remastered",true);scanDirectory(library/"steamapps/workshop/content/244160","Workshop",true);}if(const char*p=std::getenv("HW_REMASTERED_INSTALL"))scanDirectory(fs::u8path(p),"Remastered",true);}catch(const std::exception&){}std::lock_guard lock(guard);status=std::to_string(catalog.size())+" badges / stored permanently in Badges";scanning=false;});}
extern "C" int hwBadgeCount(void){hwBadgeInit();std::lock_guard lock(guard);return int(catalog.size());}
extern "C" void hwBadgeInfo(int i,char*id,size_t cap,char*name,size_t ncap){std::lock_guard lock(guard);if(i<0||size_t(i)>=catalog.size()){copy(id,cap,"");copy(name,ncap,"");return;}copy(id,cap,catalog[i]->id);copy(name,ncap,catalog[i]->label);}
extern "C" void hwBadgeStatus(char*out,size_t cap){std::lock_guard lock(guard);copy(out,cap,status);}
extern "C" int hwBadgeImportFile(const char*path,char*out,size_t cap){hwBadgeInit();try{auto p=fs::u8path(path);auto ext=lower(p.extension().string());if(ext!=".png"&&ext!=".dds")return 0;auto id=importBytes(read(p),p.stem().u8string()+" / Custom",ext);copy(out,cap,id);return !id.empty();}catch(const std::exception&){return 0;}}
extern "C" int hwBadgeCustomImage(void){wchar_t path[32768]={};OPENFILENAMEW d={};d.lStructSize=sizeof(d);d.lpstrFilter=L"Badge images (PNG / DDS)\0*.png;*.dds\0PNG images\0*.png\0DDS images\0*.dds\0\0";d.lpstrFile=path;d.nMaxFile=32768;d.lpstrTitle=L"Import badge into OpenHW permanent library";d.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_ALLOWMULTISELECT;d.hwndOwner=GetActiveWindow();if(!GetOpenFileNameW(&d))return 0;char id[65];int imported=0;const wchar_t*next=path+wcslen(path)+1;std::vector<fs::path> chosen;if(*next){while(*next){chosen.push_back(fs::path(path)/next);next+=wcslen(next)+1;}}else chosen.push_back(fs::path(path));for(const auto&item:chosen){auto name=item.u8string();if(hwBadgeImportFile(name.c_str(),id,sizeof(id))){++imported;hwBadgeSelect(id,0);}}if(!imported){std::lock_guard lock(guard);status="Cannot import image: use a valid PNG or DDS up to 4096 x 4096 / 16 MiB";return 0;}return 1;}
extern "C" void hwBadgeSelect(const char*id,int persist){hwBadgeInit();std::lock_guard lock(guard);selected=id&&records.count(id)?id:"";if(persist){auto p=root/"selected.txt.pending";{std::ofstream f(p,std::ios::binary);f<<selected;}MoveFileExW(p.c_str(),(root/"selected.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}}
extern "C" void hwBadgeSelected(char*out,size_t cap){hwBadgeInit();std::lock_guard lock(guard);copy(out,cap,selected);}
extern "C" const unsigned char*hwBadgePixels(const char*id,unsigned*w,unsigned*h){hwBadgeInit();std::lock_guard lock(guard);auto it=records.find(id?id:"");if(it==records.end())return nullptr;auto&r=*it->second;if(r.pixels.empty()&&!decode(read(r.path),r))return nullptr;
 size_t resident=0;for(auto&v:records)if(!v.second->pixels.empty())resident++;
 if(resident>64)for(auto&v:records){auto&old=*v.second;if(&old!=&r&&old.id!=selected&&!old.path.empty()&&!old.pixels.empty()&&std::find(remote.begin(),remote.end(),&old)==remote.end()){old.pixels.clear();old.pixels.shrink_to_fit();if(--resident<=64)break;}}
 if(w)*w=r.width;if(h)*h=r.height;return r.pixels.data();}
extern "C" void hwBadgeSetRemote(unsigned slot,const unsigned char*p,unsigned w,unsigned h){if(slot>=8)return;if(!p){remote[slot]=nullptr;return;}if(w!=96||h!=96)return;hwBadgeInit();std::vector<unsigned char> bytes(p,p+w*h*4);auto id="remote-"+digest(bytes);std::lock_guard lock(guard);auto it=records.find(id);if(it==records.end()){auto r=std::make_unique<Record>();r->id=id;r->width=w;r->height=h;r->pixels=std::move(bytes);it=records.emplace(id,std::move(r)).first;}remote[slot]=it->second.get();}
extern "C" void hwBadgeClearRemote(void){remote.fill(nullptr);}
extern "C" const unsigned char*hwBadgePlayerPixels(unsigned slot,int local,unsigned*w,unsigned*h,const void**identity){if(local){char id[65];hwBadgeSelected(id,sizeof(id));auto p=hwBadgePixels(id,w,h);if(p){std::lock_guard lock(guard);*identity=records[id].get();}return p;}if(slot>=8||!remote[slot])return nullptr;auto&r=*remote[slot];*w=r.width;*h=r.height;*identity=&r;return r.pixels.data();}

extern "C" int hwBadgeProbeMain(int argc,char **argv){
 if(argc<5)return 2;_putenv_s("OPENHW_BADGE_ROOT",argv[2]);hwBadgeInit();char a[65],b[65],again[65],before[65];hwBadgeSelected(before,sizeof(before));unsigned w,h;
 if(!hwBadgeImportFile(argv[3],a,sizeof(a))||!hwBadgeImportFile(argv[4],b,sizeof(b)))return 3;
 if(argc>5&&std::string(argv[5])=="load"&&std::strcmp(before,a))return 7;
 int count=hwBadgeCount();if(!hwBadgeImportFile(argv[3],again,sizeof(again))||std::strcmp(a,again)||hwBadgeCount()!=count)return 4;
 const unsigned char*rgba=hwBadgePixels(a,&w,&h);if(!rgba||w!=256||h!=256)return 5;
 unsigned sampleAlpha=rgba[(128*256+128)*4+3];const unsigned char*other=hwBadgePixels(b,&w,&h);if(!other||w!=256||h!=256)return 8;unsigned ddsAlpha=other[(128*256+128)*4+3];
 hwBadgeSelect(a,1);if(argc>5&&std::string(argv[5])=="discover"){hwBadgeDiscover();for(unsigned i=0;i<9000&&scanning;i++)std::this_thread::sleep_for(std::chrono::milliseconds(10));if(scanning){hwBadgeShutdown();return 6;}}
 std::ofstream report(root/"probe-report.json");report<<"{\"count\":"<<hwBadgeCount()<<",\"pngAlpha\":"<<sampleAlpha<<",\"ddsAlpha\":"<<ddsAlpha<<",\"selected\":\""<<a<<"\",\"labels\":[";
 for(int i=0;i<hwBadgeCount();i++){char id[65],label[256];hwBadgeInfo(i,id,sizeof(id),label,sizeof(label));std::string safe=label;for(char&c:safe)if(c=='\"'||c=='\\'||(unsigned char)c<32)c=' ';if(i)report<<",";report<<"\""<<safe<<"\"";}report<<"]}";report.close();
 hwBadgeShutdown();std::fprintf(stderr,"Badge library probe passed: %d permanent records; duplicate import, PNG/DDS decode and persisted choice checked.\n",hwBadgeCount());return 0;
}
