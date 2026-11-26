#include "CataclysmScriptRuntime.h"
#include "CataclysmScriptIds.h"
#include <algorithm>
#include <map>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <vector>

namespace {
constexpr unsigned FleetIntel=0x0a8a880cu;
constexpr unsigned Objectives=0x098ddc43u;
constexpr unsigned KushanFrigates=0x0d788163u;
constexpr unsigned TaiidanBombers=0x04fcebd3u;
constexpr unsigned Cruiser=0x0518e0d2u;
struct Selection {int count{};};
struct Team {
    unsigned id{};
    Selection ships;
    CataScriptCallback fsmWatch{};
    CataScriptCallback stateWatch{};
    std::vector<unsigned> messages;
    bool launched=true;
    bool invulnerable{};
    int health=100,damage=100,speed=-1;
    int originalCount=2;
};
struct Timer {int duration{};int deadline{};};
std::map<unsigned,Team> teams;
std::map<std::string,int> variables;
std::map<unsigned,Selection> selections;
std::map<std::string,Timer> timers;
std::vector<unsigned> attackTeams;
std::vector<unsigned> workerAttackTeams;
Team *current{};unsigned scope{};int delivered{};
bool fleetVisible{};
bool carrierSighted{},somtaawNear{};
std::vector<std::pair<unsigned,unsigned>> sentMessages;
int now{}, speechEnds{}, objectivesCreated{};
int carrierBuilds{},shipsBuilt{};
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
CataScriptArg number(int value){CataScriptArg result{};result.kind=CATA_ARG_NUMBER;result.value.number=value;return result;}
CataScriptArg object(void *value){CataScriptArg result{};result.kind=CATA_ARG_OBJECT;result.value.object=value;return result;}
void run(Team *team,CataScriptCallback callback){auto saved=current;auto oldScope=scope;current=team;callback();current=saved;scope=oldScope;}
std::string key(unsigned id){char result[64];cataScriptVariableKey(scope,id,result,sizeof(result));return result;}
std::string timerKey(unsigned id){char result[64];cataScriptScopedKey(scope,id,result,sizeof(result));return result;}
unsigned popMessage(size_t index)
{
    auto& messages=current->messages;
    if(index>=messages.size()) return 0;
    const auto message=messages[index];
    messages[index]=messages.back();
    messages.pop_back();
    return message;
}
void tick(Team& team)
{
    if(team.fsmWatch) run(&team,team.fsmWatch);
    if(team.stateWatch) run(&team,team.stateWatch);
}
}
extern "C" void cataScriptFault(const char *message){throw std::runtime_error(message);}
extern "C" void *cataScriptCurrentTeam(){return current;}
extern "C" void *cataScriptCurrentShips(){return current?&current->ships:nullptr;}
extern "C" int cataScriptMessageCount(){return current?static_cast<int>(current->messages.size()):0;}
extern "C" void cataScriptScope(int mission){scope=mission?0:current?current->id:0;}
extern "C" const char *cataScriptText(uint32_t address){return address==0x0088b940u?"sWorker":address==0x0088b4b4u?"tHeavyInterceptor":"Focus";}
extern "C" const char *cataScriptLocalizedText(uint32_t){return "Text";}
extern "C" CataScriptArg cataScriptDispatch(unsigned api,const CataScriptArg *args,unsigned count)
{
    auto n=[&](unsigned i){require(i<count && args[i].kind==CATA_ARG_NUMBER,"Expected numeric argument");return (unsigned)args[i].value.number;};
    auto sel=[&](unsigned i){
        require(i<count && args[i].kind==CATA_ARG_OBJECT,"Expected ship selection");
        return static_cast<Selection *>(args[i].value.object);
    };
    switch(api){
    case 0x004907a0:{
        auto id=n(0);
        auto [it, inserted]=teams.try_emplace(id);
        if(inserted){it->second.id=id;it->second.ships.count=id==FleetIntel || id==Objectives?0:2;}
        return object(&it->second);}
    case 0x004907e0:{
        auto result=cataScriptDispatch(0x004907a0,args,count);
        return object(&static_cast<Team *>(result.value.object)->ships);}
    case 0x00490660:{require(count==4,"FSM arity");auto team=(Team *)args[3].value.object;
        team->fsmWatch=args[2].value.callback;team->stateWatch=nullptr;run(team,args[1].value.callback);return number(0);}
    case 0x00490640:require(current && count==3,"State outside team");current->stateWatch=args[2].value.callback;args[1].value.callback();return number(0);
    case 0x00491d30:case 0x00491d70:{
        auto variable=key(n(0));
        if(variable=="ShipsHaveArrived" && n(1))
            require(fleetVisible && selections[0xcff5860u].count && selections[0xca25860u].count,
                "Mission teams can observe arrival before the Kuun-Lan selections are ready");
        if(api==0x00491d30 || variables.count(variable)) variables[variable]=(int)n(1);
        return number(0);}
    case 0x00491db0:++variables[key(n(0))];return number(0);
    case 0x00491e30:return number(variables[key(n(0))]);
    case 0x00491ec0:timers.try_emplace(timerKey(n(0)));return number(0);
    case 0x00491f00:timers.at(timerKey(n(0))).duration=n(1);return number(0);
    case 0x00491f50:{
        auto& timer=timers.at(timerKey(n(0)));
        timer.deadline=now+timer.duration;return number(0);}
    case 0x00491f90:timers[timerKey(n(0))]={static_cast<int>(n(1)),now+static_cast<int>(n(1))};return number(0);
    case 0x00492060:{
        auto found=timers.find(timerKey(n(0)));
        return number(found!=timers.end() && now>=found->second.deadline);}
    case 0x004920a0:{auto found=timers.find(timerKey(n(0)));if(found==timers.end()||now<found->second.deadline)return number(0);timers.erase(found);return number(1);}
    case 0x00417ec0:return number(static_cast<int>(popMessage(n(1))));
    case 0x00417e10:case 0x00494a00:{
        auto* target=static_cast<Team *>(args[api==0x00417e10?1:0].value.object);
        auto message=n(api==0x00417e10?2:1);
        sentMessages.emplace_back(target->id,message);
        target->messages.push_back(message);return number(0);}
    case 0x00492120:
        for(size_t i=0;i<current->messages.size();++i)
            if(current->messages[i]==n(0)){popMessage(i);return number(1);}
        return number(0);
    case 0x00492190:
        if(current->ships.count && sel(0)->count) attackTeams.push_back(current->id);
        if(sel(0)==&selections[0x0c46fa23u] && sel(0)->count)workerAttackTeams.push_back(current->id);
        return number(0);
    case 0x004948a0:return number(current->launched);
    case 0x004947c0:current->launched=false;return number(0);
    case 0x00495690:
        require(args[2].kind==CATA_ARG_TEXT && std::string(args[2].value.text)=="sWorker","Raider lost its original worker target type");
        sel(0)->count=sel(1)->count;return number(sel(0)->count);
    case 0x00492910:return number(n(0));
    case 0x00492ce0:return number(current->ships.count);
    case 0x00492e90:sel(0)->count=somtaawNear?1:0;return number(sel(0)->count);
    case 0x004985f0:return number(sel(0)->count);
    case 0x004984c0:current->invulnerable=n(1)!=0;return number(0);
    case 0x00493980:current->damage=n(1);return number(0);
    case 0x00493aa0:current->health=n(0);return number(0);
    case 0x00492b20:return number(current->health);
    case 0x00493a10:current->speed=n(1);return number(0);
    case 0x00499570:return number(carrierSighted);
    case 0x00493e00:return number(current->originalCount);
    case 0x00499870:return number(1);
    case 0x00499c70:
        require(count==5&&args[1].kind==CATA_ARG_TEXT&&std::string(args[1].value.text)=="tHeavyInterceptor",
                "Carrier production lost the original Taiidan interceptor type");
        require(sel(3)==&teams.at(0x06d3ec02u).ships&&n(4)==1,"Replacement squad used the wrong carrier or owner");
        require(n(2)==unsigned(current->originalCount),"Carrier replacement changed the original squad size");
        sel(0)->count=sel(3)->count?int(n(2)):0;
        if(sel(0)->count){++carrierBuilds;shipsBuilt+=sel(0)->count;}
        current->launched=false;return number(0);
    case 0x00493cf0:{auto target=static_cast<Team*>(args[0].value.object);target->ships.count+=sel(1)->count;return number(0);}
    case 0x00492d10:sel(0)->count=0;return number(0);
    case 0x00492d20:return number(sel(0)->count);
    case 0x00492db0:sel(1)->count=2;return number(2);
    case 0x00493b90:return number(1);
    case 0x00493de0:return number(args[0].value.object==current);
    case 0x00490af0:case 0x00490960:case 0x00490aa0:case 0x004908c0:return object(nullptr);
    case 0x00495560:case 0x00495620:sel(0)->count=sel(1)->count;return number(sel(0)->count);
    case 0x00495bd0:sel(0)->count+=sel(1)->count;return number(sel(0)->count);
    case 0x00495c40:return number(sel(0)->count);
    case 0x00494e70:++objectivesCreated;return number(0);
    case 0x00496eb0:speechEnds=now+2;return number(0);
    case 0x00497b00:return number(now>=speechEnds);
    case 0x00496e40:return number(2000);
    case 0x00496720:require(count==1 && args[0].kind==CATA_ARG_TEXT,"Lost text argument");return number(0);
    case 0x00498230:fleetVisible=true;return number(0);
    case 0x00490bb0:return object(&selections[n(0)]);
    case 0x00498790:
        require(fleetVisible,"Invalid fleet handoff selection");
        sel(0)->count=1;return number(0);
    case 0x00497000:
        require(fleetVisible,"Fleet arrival was not scheduled before music");
        return number(0);
    case 0x00495eb0:case 0x00496190:case 0x004961b0:case 0x004961f0:case 0x00496210:case 0x00496250:
    case 0x00496870:case 0x00496ae0:case 0x00496490:
    case 0x00494f60:case 0x004964b0:case 0x00496cf0:case 0x00497710:case 0x004d6480:
    case 0x00492780:case 0x00492810:case 0x00492960:case 0x004929d0:
    case 0x004941a0:case 0x00494160:case 0x00494750:
    case 0x00495e50:case 0x00495e80:case 0x00495ea0:case 0x00496260:case 0x004963a0:
    case 0x004963e0:case 0x004964e0:case 0x00496530:case 0x00496bc0:case 0x00496c30:
    case 0x00496c80:case 0x00496d10:case 0x00496e20:case 0x00497290:case 0x00497580:
    case 0x00497630:case 0x004977a0:case 0x00497a90:case 0x00497aa0:case 0x00497ad0:
    case 0x00497af0:case 0x00497b80:case 0x00497ed0:case 0x004980c0:
    case 0x00498c70:case 0x0049a2a0:case 0x0049aae0:
        return number(0);
    case 0x70000001:require(count==1,"Resolver received later arguments");return object(&teams[n(0)]);
    case 0x70000002:require(count==2 && args[0].kind==CATA_ARG_OBJECT,"Lost resolver object");delivered=(int)n(1);return number(delivered);
    default:{char error[96];std::snprintf(error,sizeof(error),"Unexpected API in Mission 1 test: %08x",api);
        throw std::runtime_error(error);}
    }
}
int main()
{
    try{
        auto resolved=cataCall(0x70000001,1,7,1234);
        require((int)cataCall(0x70000002,2,resolved)==1234 && delivered==1234,"Hoisted message was lost");
        require(resolved.pending.empty(),"Consumed caller arguments were retained");
        require((int)cataCall(0x70000002,2,resolved,4321)==4321,"Stale arguments were reused");
        require((int)cataCall(0x70000002,2,cataCall(0x70000001,1,7,99))==99,"Nested resolver lost arguments");
        bool rejected=false;try{cataCall(0x70000002,2,resolved);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Incomplete recovered call was accepted");
        require(key(0x87defce4u)=="ShipsHaveArrived","Fleet arrival handshake was not mapped");
        require(key(0x858db5e4u)=="RecentlyLoaded","Load handshake was not mapped");
        teams.clear();
        cataMission01Init();
        require(teams.size()==2,"Mission init did not create its main and event teams");
        require(variables["PlayNislet"]==1,"Intro did not request the original NISlet");
        variables["ShipsHaveArrived"]=0;
        variables["RecentlyLoaded"]=0;
        variables["Nislet1Complete"]=1;
        tick(teams.at(FleetIntel));
        require(fleetVisible && variables["ShipsHaveArrived"]==0,"Intro completion falsely signaled finished hyperspace arrival");
        tick(teams.at(Objectives));
        scope=Objectives;
        require(variables[key(0x083f97b5u)]==0,"Objectives started before physical fleet arrival");
        scope=0;
        require(std::find(attackTeams.begin(),attackTeams.end(),KushanFrigates)!=attackTeams.end() &&
                std::find(attackTeams.begin(),attackTeams.end(),TaiidanBombers)!=attackTeams.end(),
                "Opening battle did not queue orders for both fleets");
        now=2;
        variables["ShipsHaveArrived"]=1; // engine completes the scheduled arrival
        run(nullptr,cataMission01Watch);
        require(selections[0x4a2a29u].count>0,"Mission watch could not refresh combat targets");

        /* Run the opening briefings until the reinforcement order is queued
           behind its delay. It must reach the fleet only after that timer. */
        auto& intel=teams.at(FleetIntel);
        auto& objectives=teams.at(Objectives);
        for(;now<40;++now){tick(intel);tick(objectives);}
        require(objectivesCreated==1,"Opening combat objective did not appear");
        const auto& reinforcementMessages=teams.at(0x0a388bd1u).messages;
        const auto orderedToLaunch=[&]{
            return std::find(reinforcementMessages.begin(),reinforcementMessages.end(),0x0cb5e56eu)
                !=reinforcementMessages.end();
        };
        scope=FleetIntel;
        const auto briefingTimer=timers.at(timerKey(CATA_MISSION1_BRIEFING_TIMER));
        scope=0;
        require(briefingTimer.duration==50 && briefingTimer.deadline>now,"Fleet Intel did not start its briefing timer");
        now=briefingTimer.deadline-1;
        tick(intel);
        require(!orderedToLaunch(),"Fleet Intel released the reinforcements before their delay");
        now=briefingTimer.deadline;
        tick(intel);
        tick(intel);
        require(orderedToLaunch(),"Fleet Intel did not release the reinforcements when the timer expired");
        auto& raider=teams.at(0x0a388bd1u);
        tick(raider);
        tick(raider);
        require(workerAttackTeams.empty(),"Raider attacked before launching");
        raider.launched=true;
        tick(raider);
        require(!workerAttackTeams.empty() && workerAttackTeams.back()==raider.id,
                "Launched raider did not prioritize workers");
        auto& guard=teams.at(0x0cccfbf3u);
        const auto before=attackTeams.size();
        tick(guard);
        require(attackTeams.size()>before,"In-space main battle fleet never refreshed its attack orders");
        auto& cruiser=teams.at(Cruiser);
        require(cruiser.invulnerable && cruiser.damage==10 && cruiser.health==40 && cruiser.speed==-1,
                "Cruiser must fight at 40% health with reduced damage and protection, without a speed hold");
        require(std::find(attackTeams.begin(),attackTeams.end(),Cruiser)!=attackTeams.end(),
                "Cruiser never received its opening attack order");
        somtaawNear=true;tick(cruiser);
        require(!cruiser.invulnerable,"Cruiser did not become vulnerable near Somtaaw ships");
        somtaawNear=false;tick(cruiser);
        require(cruiser.invulnerable,"Cruiser did not restore background battle protection");

        auto sent=[&](unsigned team,unsigned message){return std::count(sentMessages.begin(),sentMessages.end(),std::make_pair(team,message));};
        auto stage=[&]{auto old=scope;scope=Objectives;auto value=variables[key(0x083f97b5u)];scope=old;return value;};
        auto advance=[&](int seconds){for(int i=0;i<seconds;++i,++now){tick(objectives);tick(intel);tick(cruiser);}};
        require(stage()==1 && !sent(Cruiser,0x4b095u),"Final cruiser phase triggered before initial objectives");
        teams.at(TaiidanBombers).ships.count=0;
        advance(100);
        require(stage()==2,"Bomber defeat did not advance to carrier reconnaissance");
        require(!sent(Cruiser,0x4b095u),"Cruiser crippled before sensor-array defense");
        carrierSighted=true;advance(100);
        require(stage()==3,"Carrier sighting did not unlock the convoy stage");
        teams.at(0x0cbd4dc3u).ships.count=0;advance(100);
        require(stage()==4,"Convoy destruction did not trigger sensor-array attackers");
        for(auto team:{0x0b25c8d1u,0x06a0d9b1u,0x0ebc0bc1u})
            require(sent(team,0x852cf18u)==1,"A sensor-array attack group was not activated exactly once");
        teams.at(0x0b25c8d1u).ships.count=0;teams.at(0x06a0d9b1u).ships.count=0;
        advance(50);
        require(stage()==4 && !sent(Cruiser,0x4b095u),"Help request ignored the surviving sensor attack group");
        teams.at(0x0ebc0bc1u).ships.count=0;advance(100);
        require(stage()==5 && sent(Cruiser,0x4b095u)==1,"Sensor defense did not deliver the cruiser help request exactly once");
        require(!cruiser.invulnerable && cruiser.damage==50 && cruiser.speed==0 && cruiser.health==20,
                "Final cruiser phase did not apply the original crippled state");
        teams.at(Cruiser).ships.count=0;advance(40);
        require(stage()==6,"Cruiser destruction did not complete the ordered battle objectives");
        /* Retail's worker raiders are replaced after 45 seconds, up to three
           waves total. Rebuilt ships must finish launching before attacking. */
        const auto raiderAttackWatch=raider.stateWatch;
        for(int wave=2;wave<=3;++wave){
            raider.ships.count=0;tick(raider);const auto start=now;
            for(;now<start+45;++now)tick(raider);
            require(carrierBuilds==wave-2,"Carrier built a replacement wave before the original 45-second delay");
            tick(raider);
            require(carrierBuilds==wave-1&&shipsBuilt==(wave-1)*raider.originalCount,"Carrier did not build its replacement squad");
            tick(raider);require(raider.ships.count==raider.originalCount,"Rebuilt squad was not added back to its team");
            const auto attacks=workerAttackTeams.size();tick(raider);
            require(workerAttackTeams.size()==attacks,"Replacement attacked while still inside its carrier");
            raider.launched=true;tick(raider);
            require(workerAttackTeams.size()==attacks+1,"Launched replacement did not return to attacking workers");
        }
        raider.ships.count=0;tick(raider);now+=90;tick(raider);
        require(carrierBuilds==2,"Carrier exceeded the original three-wave worker-raid limit");
        raider.stateWatch=raiderAttackWatch;raider.ships.count=0;raider.launched=true;
        scope=raider.id;variables[key(0x8a928144u)]=1;scope=0;
        teams.at(0x06d3ec02u).ships.count=0;
        tick(raider);now+=45;tick(raider);tick(raider);
        require(carrierBuilds==2&&raider.ships.count==0,"A destroyed carrier still produced replacement ships");
        std::puts("Mission 1 passed objective progression, cruiser phases and carrier replacement waves, timing, launch gates and host destruction.");return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
