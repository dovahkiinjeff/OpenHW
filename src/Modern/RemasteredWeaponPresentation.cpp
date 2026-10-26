#include "RemasteredWeaponPresentation.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Value {enum Kind {Text,Number,Boolean,Pair} kind;std::string text;float a=0,b=0;bool flag=false;};
struct Reader {
    std::string data;size_t at=0;
    void space(){for(;;){while(at<data.size() && (data[at]==' '||data[at]=='\t'||data[at]=='\r'||data[at]=='\n'))++at;
        if(at+1<data.size() && data[at]=='-' && data[at+1]=='-'){while(at<data.size() && data[at]!='\n')++at;}else return;}}
    bool eat(char c){space();if(at<data.size()&&data[at]==c){++at;return true;}return false;}
    void need(char c){if(!eat(c))throw std::runtime_error("invalid weaponfire data syntax");}
    std::string identifier(){space();size_t start=at;
        while(at<data.size() && ((data[at]>='a'&&data[at]<='z')||(data[at]>='A'&&data[at]<='Z')||data[at]=='_'||
              (at>start && data[at]>='0'&&data[at]<='9')))++at;
        if(at==start || at-start>255)throw std::runtime_error("invalid weaponfire identifier");return data.substr(start,at-start);}
    float number(){space();size_t start=at;
        if(at<data.size()&&data[at]=='-')++at;
        unsigned digits=0;while(at<data.size()&&data[at]>='0'&&data[at]<='9'){++at;++digits;}
        if(at<data.size()&&data[at]=='.'){++at;while(at<data.size()&&data[at]>='0'&&data[at]<='9'){++at;++digits;}}
        if(!digits)throw std::runtime_error("invalid weaponfire number");
        if(at<data.size()&&(data[at]=='e'||data[at]=='E')){++at;if(at<data.size()&&(data[at]=='+'||data[at]=='-'))++at;
            size_t exponent=at;while(at<data.size()&&data[at]>='0'&&data[at]<='9')++at;
            if(exponent==at)throw std::runtime_error("invalid weaponfire exponent");}
        float value=0;auto parsed=std::from_chars(data.data()+start,data.data()+at,value);
        if(parsed.ec!=std::errc{} || parsed.ptr!=data.data()+at || !std::isfinite(value))
            throw std::runtime_error("invalid/nonfinite weaponfire number");return value;}
    Value value(){space();if(at==data.size())throw std::runtime_error("missing weaponfire value");
        if(eat('"')){Value v{Value::Text};bool closed=false;
            while(at<data.size()){char c=data[at++];if(c=='"'){closed=true;break;}
                if(c=='\\'){if(at==data.size())break;c=data[at++];if(c!='\\'&&c!='"')throw std::runtime_error("unsupported weaponfire string escape");}
                if((unsigned char)c<32 || (unsigned char)c>126 || v.text.size()>=4096)throw std::runtime_error("invalid weaponfire string");v.text+=c;}
            if(!closed)throw std::runtime_error("unterminated weaponfire string");return v;}
        if(eat('{')){Value v{Value::Pair};v.a=number();if(!eat(',')&&!eat(';'))throw std::runtime_error("missing weaponfire clamp separator");
            v.b=number();if(!eat(','))eat(';');need('}');return v;}
        if(data.compare(at,4,"true")==0){at+=4;Value v{Value::Boolean};v.flag=true;return v;}
        if(data.compare(at,5,"false")==0){at+=5;return Value{Value::Boolean};}
        Value v{Value::Number};v.a=number();return v;}
};
struct OwnedSlot {std::string fx,sound;HwRmWeaponPresentationSlot view{};};
void message(char *error,size_t size,const char *text){if(error&&size)std::snprintf(error,size,"%s",text);}
}
struct HwRmWeaponPresentation {
    std::array<OwnedSlot,HW_RM_WEAPON_SLOT_COUNT> slots;
    std::string death,scar;
    std::vector<std::string> ignored;
};
extern "C" HwRmWeaponPresentation *hwRmWeaponPresentationRead(const void *bytes,size_t size,char *error,size_t capacity){
    try {
        if(!bytes || !size || size>1024*1024)throw std::runtime_error("invalid weaponfire storage");
        Reader r{std::string(static_cast<const char*>(bytes),size)};
        for(unsigned char c:r.data)if(c==0 || c>126 || (c<32&&c!='\r'&&c!='\n'&&c!='\t'))throw std::runtime_error("invalid weaponfire source character");
        std::map<std::string,Value> fields;
        for(;;){r.space();if(r.at==r.data.size())break;auto key=r.identifier();r.need('=');auto value=r.value();r.eat(';');
            if(fields.size()>=128 || !fields.emplace(key,std::move(value)).second)throw std::runtime_error("duplicate/excess weaponfire assignments");}
        auto out=std::make_unique<HwRmWeaponPresentation>();
        auto take=[&](const std::string& key,Value::Kind kind)->const Value* {
            auto found=fields.find(key);if(found==fields.end())return nullptr;
            if(found->second.kind!=kind)throw std::runtime_error("invalid weaponfire field type");return &found->second;};
        const char *names[]={"fire","bullet","hit","anticipate","miss","nopenetrate","blowthrough"};
        std::map<std::string,bool> consumed;
        for(unsigned i=0;i<HW_RM_WEAPON_SLOT_COUNT;++i){auto& slot=out->slots[i];std::string name=names[i];
            for(auto suffix:{"fx","_sfx"}){std::string key=name+suffix;consumed[key]=true;
                if(auto value=take(key,Value::Text))(suffix[0]=='f'?slot.fx:slot.sound)=value->text;}
            if(i==HW_RM_WEAPON_FIRE)continue;
            auto key=name+"_nlips";consumed[key]=true;
            if(auto value=take(key,Value::Number)){slot.view.nlips=value->a;slot.view.authoredOptions|=HW_RM_WEAPON_HAS_NLIPS;}
            key=name+"_clamp";consumed[key]=true;
            if(auto value=take(key,Value::Pair)){slot.view.clampMinimum=value->a;slot.view.clampMaximum=value->b;slot.view.authoredOptions|=HW_RM_WEAPON_HAS_CLAMP;}
            key=name+"_sm";consumed[key]=true;
            if(auto value=take(key,Value::Boolean)){slot.view.sm=value->flag;slot.view.authoredOptions|=HW_RM_WEAPON_HAS_SM;}
        }
        for(auto key:{"deathtype","scartype"}){consumed[key]=true;if(auto value=take(key,Value::Text))(key[0]=='d'?out->death:out->scar)=value->text;}
        for(const auto& field:fields)if(!consumed.count(field.first))out->ignored.push_back(field.first);
        for(auto& slot:out->slots){slot.view.fx=slot.fx.c_str();slot.view.sound=slot.sound.c_str();}
        message(error,capacity,"");return out.release();
    }catch(const std::exception& e){message(error,capacity,e.what());return nullptr;}
}
extern "C" void hwRmWeaponPresentationFree(HwRmWeaponPresentation *p){delete p;}
extern "C" const HwRmWeaponPresentationSlot *hwRmWeaponPresentationAt(const HwRmWeaponPresentation *p,unsigned i){return p&&i<HW_RM_WEAPON_SLOT_COUNT?&p->slots[i].view:nullptr;}
extern "C" const char *hwRmWeaponPresentationDeathType(const HwRmWeaponPresentation *p){return p?p->death.c_str():nullptr;}
extern "C" const char *hwRmWeaponPresentationScarType(const HwRmWeaponPresentation *p){return p?p->scar.c_str():nullptr;}
extern "C" unsigned hwRmWeaponPresentationIgnoredCount(const HwRmWeaponPresentation *p){return p?(unsigned)p->ignored.size():0;}
extern "C" const char *hwRmWeaponPresentationIgnoredAt(const HwRmWeaponPresentation *p,unsigned i){return p&&i<p->ignored.size()?p->ignored[i].c_str():nullptr;}
