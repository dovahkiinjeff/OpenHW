#include "OpenHWMods.h"
#include "../Game/OpenHWArchive.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
namespace {
namespace fs=std::filesystem;
bool valid(const std::string& s){return !s.empty()&&s.size()<=64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}) && s!="CON"&&s!="NUL";}
std::string lower(std::string s){for(auto& c:s)c=(char)std::tolower((unsigned char)c);return s;}
void safe(const fs::path& p){fs::path at;for(const auto& part:p){at/=part;auto a=GetFileAttributesW(at.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Linked folders are not supported for mod writes");}}
void message(char* p,size_t n,const char* s){if(p&&n)snprintf(p,n,"%s",s);}
void write(const fs::path& p,const std::string& data){safe(p);fs::create_directories(p.parent_path());auto temp=p;temp+=L".tmp";safe(temp);std::ofstream o(temp,std::ios::binary|std::ios::trunc);o.write(data.data(),data.size());o.close();if(!o)throw std::runtime_error("Cannot write mod file");if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot install mod file");}
std::string load(const fs::path& p){std::ifstream i(p,std::ios::binary);if(!i)throw std::runtime_error("Missing mod dependency: "+p.string());return std::string(std::istreambuf_iterator<char>(i),{});}
bool relative(const std::string& p){return !p.empty()&&p.find(':')==p.npos&&p[0]!='/'&&p[0]!='\\'&&p.find("..") == p.npos;}
}
extern "C" int openhwModsRead(const char* root,OpenHWModEntry* rows,int cap,char* error,size_t n){try{
 if(!root||!rows||cap<1)throw std::runtime_error("Invalid mod list");std::set<std::string> seen;int count=0;
 auto add=[&](const std::string& name,int enabled){if(valid(name)&&seen.insert(lower(name)).second&&fs::is_directory(fs::path(root)/"mods"/name)){if(count>=cap)throw std::runtime_error("Too many mods");snprintf(rows[count].name,65,"%s",name.c_str());rows[count++].enabled=enabled;}};
 std::ifstream input(fs::path(root)/"mods/load-order.txt");std::string line;
 while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.rfind("#disabled=",0)==0)add(line.substr(10),0);else if(!line.empty()&&line[0]!='#')add(line,1);}
 std::vector<std::string> found;auto dir=fs::path(root)/"mods";if(fs::is_directory(dir))for(auto& e:fs::directory_iterator(dir))if(e.is_directory())found.push_back(e.path().filename().string());std::sort(found.begin(),found.end());for(auto& name:found)add(name,0);return count;
 }catch(const std::exception& e){message(error,n,e.what());return -1;}}
extern "C" int openhwModsSave(const char* root,const OpenHWModEntry* rows,int count,char* error,size_t n){try{
 if(!root||!rows||count<0||count>128)throw std::runtime_error("Invalid mod list");std::set<std::string> seen;
 std::string text="# Lower entries override higher entries. Restart to apply.\n";
 for(int i=0;i<count;i++){if(!valid(rows[i].name)||!seen.insert(lower(rows[i].name)).second)throw std::runtime_error("Invalid or duplicate mod name");text+=(rows[i].enabled?"":"#disabled=")+std::string(rows[i].name)+"\n";}
 write(fs::path(root)/"mods/load-order.txt",text);return 1;
 }catch(const std::exception& e){message(error,n,e.what());return 0;}}
extern "C" int openhwModPublish(const char* root,const char* name,int append,const char* shipPath,const char* definition,size_t length,int replace,char* error,size_t n){try{
 if(!root||!name||!valid(name)||!shipPath||!relative(shipPath)||!definition||!length||length>1024*1024)throw std::runtime_error("Invalid mod name or ship path");
 const fs::path base(root),dest=base/"mods"/name;safe(dest);
 if(append?!fs::is_directory(dest):fs::exists(dest))throw std::runtime_error(append?"Select an existing mod":"Mod already exists; use Append");
 std::map<std::string,std::string> files;std::set<std::string> folders;
 auto collect=[&](const std::string& folder){if(!relative(folder)||!folders.insert(lower(folder)).second)return;
  const auto directory=base/"Data"/folder;if(!fs::is_directory(directory))throw std::runtime_error("Missing dependency folder: "+folder);
  for(auto& entry:fs::recursive_directory_iterator(directory)){safe(entry.path());if(!entry.is_regular_file())continue;auto filename=entry.path().filename().string();if(filename.find(".bak")!=filename.npos||filename.find(".before")!=filename.npos||entry.path().extension()==".tmp")continue;auto rel=(fs::path(folder)/fs::relative(entry.path(),directory)).generic_string();char overridePath[2048];files[rel]=load(openhwModOverride(rel.c_str(),overridePath,sizeof(overridePath))?fs::path(overridePath):entry.path());}
 };
 collect(fs::path(shipPath).parent_path().generic_string());
 std::string text(definition,length);std::istringstream input(text);std::string line;
 while(std::getline(input,line)){if(line.rfind("StartShipHardPointConfig(",0)!=0)continue;auto end=line.rfind('"');if(end==line.npos)throw std::runtime_error("Invalid subsystem declaration");auto begin=line.rfind('"',end-1);auto type=line.substr(begin+1,end-begin-1);if(!valid(type))throw std::runtime_error("Invalid subsystem identity");collect("Subsystems/"+type);}
 // Explicit global references can share another subsystem's model/material/events.
 for(size_t pass=0;pass<32;pass++){auto previous=folders.size();std::vector<std::string> references;
  for(const auto& f:files)if(f.first.size()>=12&&f.first.find(".owsubsystem")!=f.first.npos){std::istringstream in(f.second);std::string word;while(in>>word)if(word.rfind("@/Subsystems/",0)==0){auto end=word.find('/',13);if(end!=word.npos)references.push_back(word.substr(2,end-2));}}
  for(const auto& folder:references)collect(folder);if(previous==folders.size())break;if(pass==31)throw std::runtime_error("Dependency graph exceeds limit");
 }
 files[shipPath]=text;
 bool conflict=false;for(auto& f:files)if(fs::exists(dest/f.first)&&load(dest/f.first)!=f.second)conflict=true;
 if(conflict&&!replace){message(error,n,"Existing mod files differ. Replace them with backups?");return 2;}
 const char* development=std::getenv("HW_DEVELOPMENT_ROOT");
 const auto backup=(development?fs::path(development):base/"Development")/"Backups"/("mod-"+std::string(name)+"-"+std::to_string(GetTickCount64()));
 for(auto& f:files){auto target=dest/f.first;safe(target);if(fs::exists(target)){if(load(target)==f.second)continue;auto old=backup/f.first;fs::create_directories(old.parent_path());fs::copy_file(target,old,fs::copy_options::none);}write(target,f.second);}
 message(error,n,"Mod saved. Enable it in Mod Manager; restart to apply.");return 1;
 }catch(const std::exception& e){message(error,n,e.what());return 0;}}
