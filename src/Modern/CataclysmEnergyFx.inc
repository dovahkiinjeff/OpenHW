/* Read original scripts through the owned archive. Namespace effect names so
   Cataclysm statics never resolve to a cached HW1 effect with the same name. */
namespace {
std::string cataEffectText(const std::string& name){
    int size;auto data=(const char*)hwCataDataView(("etg/"+name).c_str(),&size);
    if(!data)throw std::runtime_error("Missing owned effect: "+name);
    return scriptText(data,size);
}
std::string cataCodeLine(std::string line){
    auto at=line.find("//");if(at!=line.npos)line.erase(at);
    at=line.find(';');if(at!=line.npos)line.erase(at);return line;
}
std::string cataNamespaceEffects(const std::string& line){
    const std::regex effect("[A-Za-z0-9_]+\\.ebg",std::regex::icase);
    std::string result;size_t offset=0;
    for(std::sregex_iterator it(line.begin(),line.end(),effect),end;it!=end;++it){
        result+=line.substr(offset,it->position()-offset)+"Cata_"+key(it->str());
        offset=it->position()+it->length();
    }
    return result+line.substr(offset);
}
void cataEffectDependencies(const std::string& name,std::set<std::string>& names){
    if(!names.insert(key(name)).second)return;
    if(names.size()>64)throw std::runtime_error("Owned energy-effect dependency limit exceeded");
    std::istringstream lines(cataEffectText(name));std::string line;
    const std::regex effect("[A-Za-z0-9_]+\\.ebg",std::regex::icase);
    while(std::getline(lines,line)){
        line=cataCodeLine(line);
        for(std::sregex_iterator it(line.begin(),line.end(),effect),end;it!=end;++it)
            cataEffectDependencies(it->str(),names);
    }
}
}
extern "C" const char *hwCataEnergyFxScript(void){
    std::lock_guard<std::recursive_mutex> lock(mutex);
    try{
        const std::string cacheKey="@energy-fx-script";auto found=cache.find(cacheKey);
        if(found==cache.end()){
            std::set<std::string> dependencies;std::ostringstream bindings,out;
            const char *files[]={"race_kushan.script","race_taidan.script"};
            const char *races[]={"R1","R2"};
            for(unsigned race=0;race<2;++race){
                std::istringstream lines(cataEffectText(files[race]));std::string line;
                while(std::getline(lines,line)){
                    line=cataCodeLine(line);std::istringstream words(line);std::string command,params;
                    words>>command;std::getline(words,params);
                    if(command.rfind("set",0)!=0 || params.find("EnergyCannon")==params.npos)continue;
                    auto comma=params.find(',');if(comma==params.npos)throw std::runtime_error("Invalid owned energy mapping");
                    params=std::string(races[race])+params.substr(comma);
                    const std::regex effect("[A-Za-z0-9_]+\\.ebg",std::regex::icase);
                    for(std::sregex_iterator it(params.begin(),params.end(),effect),end;it!=end;++it)
                        cataEffectDependencies(it->str(),dependencies);
                    params=cataNamespaceEffects(params);
                    bindings<<command<<' '<<params<<'\n';
                }
            }
            if(dependencies.empty())throw std::runtime_error("Owned energy effect mappings are missing");
            cataEffectDependencies("repair_corvette.ebg",dependencies);
            cataEffectDependencies("workersalvage.ebg",dependencies);
            cataEffectDependencies("r1salvage.ebg",dependencies);
            cataEffectDependencies("salvage.ebg",dependencies);
            cataEffectDependencies("p1salvage.ebg",dependencies);
            for(const auto& name:dependencies)out<<"LoadEffect Cata_"<<name<<'\n';
            out<<bindings.str();auto value=out.str();
            found=cache.emplace(cacheKey,std::vector<unsigned char>(value.begin(),value.end())).first;
            found->second.push_back(0);
        }
        return (const char*)found->second.data();
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm energy FX] %s\n",e.what());return nullptr;}
}
extern "C" const void *hwCataCompatibleEffectView(const char *name,int *size){
    std::lock_guard<std::recursive_mutex> lock(mutex);if(size)*size=-1;
    try{
        auto path=key(name);if(path.rfind("cataclysm/",0)==0)path.erase(0,10);
        if(path=="etg/cata_energy.script"){
            const char *script=hwCataEnergyFxScript();if(script&&size)*size=(int)std::strlen(script);return script;
        }
        if(path.rfind("etg/cata_",0)!=0)return nullptr;
        auto source=path.substr(9);const std::string cacheKey="@effect/"+source;
        auto found=cache.find(cacheKey);
        if(found==cache.end()){
            std::istringstream lines(cataEffectText(source));std::ostringstream out;std::string line;
            const std::regex effect("[A-Za-z0-9_]+\\.ebg",std::regex::icase);
            const std::regex mesh("etg[\\\\/]",std::regex::icase);
            while(std::getline(lines,line)){
                if(line.rfind("#line",0)==0){out<<line<<'\n';continue;}
                line=cataCodeLine(line);
                // The owned script declares P1Salvage but calls it P1salvage.
                // OpenHW's ETG symbol table compares labels case-sensitively.
                constexpr char retailCall[]="setMeshAnimation(P1salvage,";
                if(source=="p1salvage.ebg")
                    if(auto use=line.find(retailCall);use!=line.npos)
                        line.replace(use,sizeof(retailCall)-1,"setMeshAnimation(P1Salvage,");
                line=cataNamespaceEffects(line);
                line=std::regex_replace(line,mesh,"Cataclysm\\ETG\\");
                out<<line<<'\n';
            }
            auto value=out.str();found=cache.emplace(cacheKey,std::vector<unsigned char>(value.begin(),value.end())).first;
        }
        if(size)*size=(int)found->second.size();return found->second.data();
    }catch(const std::exception& e){std::fprintf(stderr,"[Cataclysm energy FX] %s\n",e.what());return nullptr;}
}
