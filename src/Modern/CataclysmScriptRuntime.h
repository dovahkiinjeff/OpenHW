#ifndef OPENHW_CATACLYSM_SCRIPT_RUNTIME_H
#define OPENHW_CATACLYSM_SCRIPT_RUNTIME_H
#include "CataclysmScriptBridge.h"
#include <vector>
#include <utility>
#include <cstdio>

/* Recovery can show a caller's later stack arguments on a preceding resolver
   call. Carry these arguments as typed values until the consuming API, rather
   than discarding them or reproducing a retail stack/32-bit memory layout. */
struct CataValue {
    CataScriptArg value{};
    std::vector<CataScriptArg> pending;
    CataValue(){value.kind=CATA_ARG_NUMBER;value.value.number=0;}
    CataValue(int n):CataValue(){value.value.number=n;}
    CataValue(unsigned n):CataValue(){value.value.number=n;}
    CataValue(const char *text){value.kind=CATA_ARG_TEXT;value.value.text=text;}
    CataValue(CataScriptCallback callback){value.kind=CATA_ARG_CALLBACK;value.value.callback=callback;}
    explicit CataValue(void *object){value.kind=CATA_ARG_OBJECT;value.value.object=object;}
    explicit CataValue(CataScriptArg arg):value(arg){}
    operator int() const {
        if(value.kind!=CATA_ARG_NUMBER){cataScriptFault("Object used as a numeric mission result");return 0;}
        return (int)value.value.number;
    }
};
inline void cataAppendArgs(std::vector<CataScriptArg>& args,CataValue& value){
    args.push_back(value.value);args.insert(args.end(),value.pending.begin(),value.pending.end());value.pending.clear();
}
inline void cataAppendArgs(std::vector<CataScriptArg>& args,const CataValue& value){
    args.push_back(value.value);args.insert(args.end(),value.pending.begin(),value.pending.end());
}
template<class T> inline void cataAppendArgs(std::vector<CataScriptArg>& args,T&& value){
    CataValue boxed(std::forward<T>(value));cataAppendArgs(args,boxed);
}
inline CataValue cataInvoke(unsigned api,unsigned arity,std::vector<CataScriptArg>& args){
    if(args.size()<arity){char error[128];std::snprintf(error,sizeof(error),"Mission API %08X needs %u arguments, got %u",api,arity,(unsigned)args.size());cataScriptFault(error);return {};}
    CataValue result(cataScriptDispatch(api,args.data(),arity));
    result.pending.assign(args.begin()+arity,args.end());return result;
}
template<class... T> inline CataValue cataCall(unsigned api,unsigned arity,T&&... values){
    std::vector<CataScriptArg> args;(cataAppendArgs(args,std::forward<T>(values)),...);return cataInvoke(api,arity,args);
}
inline CataValue cataTeam(){return CataValue(cataScriptCurrentTeam());}
inline CataValue cataShips(){return CataValue(cataScriptCurrentShips());}
#endif
