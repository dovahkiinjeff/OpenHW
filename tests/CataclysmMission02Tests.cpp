#include "CataclysmScriptRuntime.h"
#include <array>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <set>
namespace {
constexpr unsigned podTeam = 0x0893b502;
constexpr unsigned podArrival = 0x08970502;
constexpr unsigned gateSelection = 0x4c34;
constexpr unsigned salvageRaider = 0xdbdde91;
constexpr unsigned salvageRaidOther = 0xdbdde92;
constexpr unsigned salvageRaidCenter = 0x712bfb4;
constexpr unsigned testTeamCenter = 0xdead;
struct Team {unsigned id;CataScriptCallback init{},watch{},stateWatch{};};
std::map<unsigned,Team> teams;
std::map<unsigned,unsigned> selections;
std::map<unsigned,std::array<int,3>> positions;
std::map<std::pair<unsigned,unsigned>,int> variables;
std::vector<unsigned> destroyed,technologies;
std::vector<std::pair<unsigned,unsigned>> moduleTypes,moduleCounts;
std::map<unsigned,unsigned> timers;
Team *current;unsigned difficulty;bool arrived;unsigned checks;
unsigned podSignDraws,podDistanceDraws,gateMissileWaves;
int salvageRaidRoll;
unsigned salvageRaidSearches,salvageRaidOrders;
bool podLeft,podEntered,attackMessage;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
CataScriptArg number(int n){CataScriptArg a{};a.kind=CATA_ARG_NUMBER;a.value.number=n;return a;}
CataScriptArg object(void *p){CataScriptArg a{};a.kind=CATA_ARG_OBJECT;a.value.object=p;return a;}
void run(Team *team,CataScriptCallback callback){
    auto *previous=current;current=team;callback();current=previous;
}
}
extern "C" void cataScriptFault(const char *message){throw std::runtime_error(message);}
extern "C" void *cataScriptCurrentTeam(){return current;}
extern "C" void *cataScriptCurrentShips(){return current;}
extern "C" int cataScriptMessageCount(){return 0;}
extern "C" void cataScriptScope(int){}
extern "C" const char *cataScriptText(uint32_t address){
    static std::map<unsigned,std::string> text;
    auto [entry,_]=text.try_emplace(address,std::to_string(address));return entry->second.c_str();
}
extern "C" const char *cataScriptLocalizedText(uint32_t address){return cataScriptText(address);}
extern "C" CataScriptArg cataScriptDispatch(unsigned api,const CataScriptArg *args,unsigned count){
    auto n=[&](unsigned index){check(index<count&&args[index].kind==CATA_ARG_NUMBER,"numeric argument");return int(args[index].value.number);};
    auto x=[&](unsigned index){check(index<count&&args[index].kind==CATA_ARG_TEXT,"text argument");return unsigned(std::stoul(args[index].value.text));};
    auto o=[&](unsigned index){check(index<count&&args[index].kind==CATA_ARG_OBJECT,"object argument");return args[index].value.object;};
    auto local=[&](unsigned id)->int&{return variables[{current?current->id:0,id}];};
    switch(api){
    case 0x004907a0:{auto id=unsigned(n(0));auto [entry,_]=teams.try_emplace(id,Team{id});return object(&entry->second);}
    case 0x004907e0:case 0x00490bb0:{auto id=unsigned(n(0));selections[id]=id;return object(&selections[id]);}
    case 0x00490660:{check(count==4,"FSM arity");auto *team=static_cast<Team*>(o(3));
        check(args[1].kind==CATA_ARG_CALLBACK&&args[2].kind==CATA_ARG_CALLBACK,"typed callbacks");
        team->init=args[1].value.callback;team->watch=args[2].value.callback;
        run(team,team->init);return number(0);}
    case 0x00490640:
        check(current&&count==3&&args[1].kind==CATA_ARG_CALLBACK&&args[2].kind==CATA_ARG_CALLBACK,"state callbacks");
        current->stateWatch=args[2].value.callback;run(current,args[1].value.callback);break;
    case 0x00491d30:case 0x00491d70:local(n(0))=n(1);break;
    case 0x00491db0:++local(n(0));break;
    case 0x00491df0:--local(n(0));break;
    case 0x00491e30:return number(local(n(0)));
    case 0x00491e70:variables.erase({current->id,unsigned(n(0))});break;
    case 0x00493de0:return number(current==o(0));
    case 0x00492910:
        if(salvageRaidRoll>=0 && n(0)==0 && n(1)==9)return number(salvageRaidRoll);
        if(current&&current->id==podTeam){
            if(n(0)==-1&&n(1)==1)return number(0);
            if(n(0)==1&&n(1)==2)return number(++podSignDraws);
            if(n(0)==-6000&&n(1)==6000)return number(-321);
            if(n(0)==55000&&n(1)==65000)return number(++podDistanceDraws==1?59001:61002);
        }
        return number(n(0));
    case 0x00498dd0:positions[n(0)]={n(1),n(2),n(3)};break;
    case 0x00490820:case 0x004908c0:case 0x00490b30:return object(&positions[n(0)]);
    case 0x004909f0:return object(&positions[testTeamCenter]);
    case 0x00493800:if(current->id==podTeam)podLeft=true;break;
    case 0x00493600:
        if(current->id==podTeam){
            check(podLeft&&o(0)==&positions.at(podArrival),"pod hyperspace needs its complete arrival vector");
            check(positions.at(podArrival)==std::array<int,3>{61002,-59001,-321},"pod coordinates or retail RNG order changed");
            podEntered=true;
        }
        break;
    case 0x00492120:
        if(n(0)==0x588674&&attackMessage){attackMessage=false;return number(1);}
        break;
    case 0x00495a50:
        o(0);o(1);o(2);n(3);
        if(salvageRaidRoll>=0){
            check(o(0)==&selections.at(0x6896db5)&&o(1)==&selections.at(0x4a2a29),"Salvage raid searches the player fleet");
            const unsigned center=salvageRaidRoll<7?salvageRaidCenter:testTeamCenter;
            check(o(2)==&positions.at(center)&&n(3)==25000,"Both salvage raid searches need the retail radius");
            ++salvageRaidSearches;
        }
        return number(64);
    case 0x00492190:
        if(salvageRaidRoll>=0){
            check(o(0)==&selections.at(0x6896db5),"Salvage raid must attack its refreshed targets");
            ++salvageRaidOrders;
        }
        break;
    case 0x00492d20:return number(1);
    case 0x00492ae0:case 0x00498630:case 0x00494450:case 0x00494510:return number(0);
    case 0x00492960:case 0x00495c40:case 0x0049a100:case 0x004920e0:return number(0);
    case 0x004948a0:return number(1);
    case 0x00499c70:
        check(o(3)==&selections.at(gateSelection)&&n(4)==1,"raider reinforcements need the selected gates and Turanic owner");
        if(o(0)==&selections[0x379e5c2]){
            check(x(1)==0x0088b3c4&&n(2)>=0,"missile corvette reinforcement type and count");
            ++gateMissileWaves;
        }
        break;
    case 0x00493b90:return number(difficulty);
    case 0x00491f90:timers[n(0)]=n(1);return number(0);
    case 0x00498270:check(args[0].kind==CATA_ARG_OBJECT,"destroy selection");destroyed.push_back(*static_cast<unsigned*>(args[0].value.object));return number(0);
    case 0x00496cf0:technologies.push_back(x(0));return number(0);
    case 0x00497f10:moduleTypes.emplace_back(n(0),x(1));return number(0);
    case 0x00497f70:moduleCounts.emplace_back(n(0),n(1));return number(0);
    case 0x00498230:arrived=true;return number(0);
    case 0x00497000:case 0x00498790:case 0x0049aae0:case 0x00496e20:
    case 0x004980c0:case 0x00493f40:case 0x00490af0:case 0x004995e0:
    case 0x00497710:case 0x00491ec0:case 0x00496720:case 0x00493490:
    case 0x00495bd0:case 0x00498c10:case 0x00498c70:case 0x0049a460:
    case 0x00492780:case 0x004941a0:case 0x004991d0:case 0x004938f0:
    case 0x00493a10:case 0x00493aa0:case 0x004984c0:case 0x0049a2d0:
    case 0x00498f30:case 0x00497fd0:case 0x00495eb0:case 0x00494a00:
    case 0x00495640:case 0x00495690:case 0x00492d10:case 0x00493cf0:
    case 0x00492ce0:case 0x004927e0:return number(0);
    default:{char message[80];std::snprintf(message,sizeof(message),"Unexpected initializer API %08x",api);cataScriptFault(message);}
    }
    return number(0);
}
int main(){try{
    for(difficulty=0;difficulty<4;difficulty++){
        teams.clear();selections.clear();positions.clear();variables.clear();
        destroyed.clear();technologies.clear();moduleTypes.clear();moduleCounts.clear();timers.clear();
        arrived=podLeft=podEntered=false;podSignDraws=podDistanceDraws=0;
        salvageRaidRoll=-1;
        cataMission02Init();
        check(arrived,"Original M2 initialization must restore the fleet");
        check(podEntered,"The message pod must receive its hyperspace order during mission startup");
        check(technologies==std::vector<unsigned>({0x00890688,0x0089056c,0x0089069c,0x008906ac}),"Original fighter/link/salvage/repair unlocks changed");
        check(moduleCounts==std::vector<std::pair<unsigned,unsigned>>({{0,3},{1,1}}),"Original module slot counts changed");
        check(moduleTypes.size()==8,"Original module slot eligibility changed");
        const unsigned removed[]={5,3,2,1};
        check(destroyed.size()==removed[difficulty],"Original difficulty raid count changed");
        if(difficulty<3){const unsigned seconds[]={120,180,360};check(timers.at(0x4ce34)==seconds[difficulty],"Original escort delay changed");}
        check(teams.at(0x0a8a880c).init&&teams.at(0x098ddc43).watch,"Briefing/objective FSMs absent");
        if(difficulty==0){
            salvageRaidSearches=salvageRaidOrders=0;
            for(auto [id,roll]:{std::pair{salvageRaider,0},std::pair{salvageRaidOther,7}}){
                auto& raider=teams.at(id);
                salvageRaidRoll=roll;
                attackMessage=true;
                run(&raider,raider.stateWatch); // Hyperspace in and launch a first attack.
                run(&raider,raider.stateWatch); // Refresh targets when that attack ends.
            }
            check(salvageRaidSearches==2&&salvageRaidOrders==2,"Both salvage raids must finish retargeting");
            salvageRaidRoll=-1;
        }
        if(difficulty>=2){
            gateMissileWaves=0;
            for(unsigned id:{0x617a7d1u,0x617a7d2u,0x617a7d3u}){
                auto& raider=teams.at(id);
                check(raider.stateWatch,"Raider attack state missing");
                attackMessage=true;
                run(&raider,raider.stateWatch);
                check(!attackMessage,"Raider did not process its attack order");
            }
            check(gateMissileWaves==3,"Each raider must receive its missile-corvette wave from the gates");
        }
    }
    unsigned callbacks=0;std::set<const void*> unique;
    for(auto table=cataMission02FunctionTable();*table;++table){++callbacks;unique.insert(*table);}
    // Release linking may fold identical empty callbacks; the ordered table
    // must still retain every original callback label for save/load.
    check(callbacks==108,"Save callback table incomplete");
    std::printf("Cataclysm Mission 2 initializer: %u checks passed.\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
