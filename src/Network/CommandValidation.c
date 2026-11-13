#include "CommandValidation.h"
#include "CommandNetwork.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Select.h"
#include "ConsMgr.h"
#include "ResearchAPI.h"
#include "ShipPilotFrame.h"
#include "FormationSpreadPolicy.h"
#include <string.h>
#include <math.h>
#include <stddef.h>

static unsigned short word(const unsigned char *p){unsigned short n;memcpy(&n,p,2);return n;}
static int owned(ShipID id,unsigned player)
{
    Ship *ship=ShipIDtoShip(id,TRUE);
    return ship && ship->playerowner==&universe.players[player] && !(ship->flags&SOF_Dead);
}
static int selection(const unsigned char *p,size_t available,unsigned player,size_t *length)
{
    unsigned n,i;
    if(available<2)return 0;
    n=word(p);if(!n || n>1024 || n>(available-2)/sizeof(ShipID))return 0;
    *length=2+n*sizeof(ShipID);
    for(i=0;i<n;i++){ShipID id;memcpy(&id,p+2+i*sizeof(id),sizeof(id));if(!owned(id,player))return 0;}
    return 1;
}
static int targets(const unsigned char *p,size_t available,int allowEmpty,size_t *length)
{
    unsigned n,i;
    if(available<2)return 0;n=word(p);
    /* Derive the native variable-length layout from its ABI, not assumptions. */
    *length=sizeof(NetAttackSelection)-sizeof(TargetID)+(size_t)n*sizeof(TargetID);
    if((!n&&!allowEmpty)||n>1024 || *length>available)return 0;
    for(i=0;i<n;i++) {
        TargetID id;memcpy(&id,p+offsetof(NetAttackSelection,TargetID)+i*sizeof(id),sizeof(id));
        if(id.objtype>OBJ_DerelictType)return 0;
    }
    return 1;
}
static int finiteVector(const unsigned char *p)
{vector v;memcpy(&v,p,sizeof(v));return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z)&&fabsf(v.x)<1e9f&&fabsf(v.y)<1e9f&&fabsf(v.z)<1e9f;}

int hwValidatePlayerCommand(const void *bytes,size_t size,unsigned player)
{
    HWPacketHeader header;
    const unsigned char *p=(const unsigned char*)bytes,*end;
    unsigned type;size_t n=0,a=0,b=0;
    if(!bytes || player>=universe.numPlayers || size<sizeof(header)+2 || size>32768)return 0;
    end=p+size;
    memcpy(&header,p,sizeof(header));
    /* One authenticated order per packet. The worker alone merges sync frames. */
    if(header.type!=PACKETTYPE_COMMAND || header.from!=player || header.numberOfCommands!=1)return 0;
    p+=sizeof(header);type=word(p);p+=2;
    if((type>>8)!=player)return 0;type&=255;
#define HAVE(t) if((size_t)(end-p)<sizeof(t))return 0
#define SELECT_AT(t,member) do { size_t offset=offsetof(t,member); if(offset>(size_t)(end-p) || !selection(p+offset,end-p-offset,player,&a))return 0; n=sizeof(t)-sizeof(NetSelection)+a; } while(0)
    switch(type) {
    case COMMANDTYPE_MOVE:case COMMANDTYPE_MP_HYPERSPACE:
        HAVE(NetMoveCommand);if(!finiteVector(p)||!finiteVector(p+sizeof(vector)))return 0;
        SELECT_AT(NetMoveCommand,selection);break;
    case COMMANDTYPE_ATTACK:case COMMANDTYPE_SPECIAL:
        if(!targets(p,end-p,type==COMMANDTYPE_SPECIAL,&a) || !selection(p+a,end-p-a,player,&b))return 0;
        n=sizeof(NetAttackCommand)-sizeof(NetAttackSelection)-sizeof(NetSelection)+a+b;break;
    case COMMANDTYPE_PROTECT:
        if(!selection(p,end-p,player,&a)||!selection(p+a,end-p-a,player,&b))return 0;n=a+b;break;
    case COMMANDTYPE_FORMATION:
        HAVE(NetFormationCommand);if(word(p)>8 && word(p)!=100)return 0;
        SELECT_AT(NetFormationCommand,selection);break;
    case COMMANDTYPE_DOCK:
        HAVE(NetDockCommand);if(word(p+offsetof(NetDockCommand,dockType))&~1023u)return 0;
        SELECT_AT(NetDockCommand,selection);break;
    case COMMANDTYPE_LAUNCHMULTIPLE: {
        ShipID id;HAVE(NetLaunchMultipleCommand);memcpy(&id,p,sizeof(id));if(!owned(id,player))return 0;
        SELECT_AT(NetLaunchMultipleCommand,selection);break; }
    case COMMANDTYPE_MISC: {
        NetMiscCommand command;HAVE(NetMiscCommand);memcpy(&command,p,sizeof(command));
        if(command.miscCommand>MISCCOMMAND_FORMATION_SPREAD)return 0;
        if(command.miscCommand==MISCCOMMAND_TACTICS && command.miscData>2)return 0;
        if(command.miscCommand==MISCCOMMAND_FORMATION_SPREAD && command.miscData>FORMATION_SPREAD_MAX)return 0;
        SELECT_AT(NetMiscCommand,selection);break; }
    case COMMANDTYPE_COLLECTRESOURCE:
        HAVE(NetCollectResourceCommand);SELECT_AT(NetCollectResourceCommand,selection);break;
    case COMMANDTYPE_AUTOLAUNCH: {
        NetAutolaunchCommand command;HAVE(NetAutolaunchCommand);memcpy(&command,p,sizeof(command));
        if(command.playerIndex!=player || command.OnOff>31)return 0;n=sizeof(command);break; }
    case COMMANDTYPE_DETERMINISTICBUILD: {
        NetDeterministicBuildCommand command;Ship *factory;
        HAVE(NetDeterministicBuildCommand);memcpy(&command,p,sizeof(command));
        if(command.playerIndex!=player || command.subCommand<CMD_START || command.subCommand>CMD_CANCEL ||
           (command.numShips&0x3fff)>1000 || command.shipRace>R2 || command.shipType>=TOTAL_NUM_SHIPS || !owned(command.creatorID,player))return 0;
        factory=ShipIDtoShip(command.creatorID,TRUE);
        if(cmFactoryBuildRace(factory)!=command.shipRace || !cmCanBuildShipType(factory,command.shipType,TRUE))return 0;
        n=sizeof(command);break; }
    case COMMANDTYPE_RESEARCHINFO: {
        NetResearchCommand command;HAVE(NetResearchCommand);memcpy(&command,p,sizeof(command));
        if(command.playerindex!=player || command.ResearchMsgType>1 || command.labnum>=NUM_RESEARCHLABS || command.tech>=32)return 0;
        n=sizeof(command);break; }
    case COMMANDTYPE_RUTRANSFER: {
        NetRUTransferferCommand command;HAVE(NetRUTransferferCommand);memcpy(&command,p,sizeof(command));
        if(command.from!=player || command.to>=universe.numPlayers || command.resourceUnits<0 || command.resourceUnits>universe.players[player].resourceUnits || command.flags)return 0;
        n=sizeof(command);break; }
    case COMMANDTYPE_SHIP_PILOT: {
        ShipPilotFrame input;ShipID id;HAVE(ShipPilotFrame);memcpy(&input,p,sizeof(input));
        id.shipNumber=input.ship;if(!owned(id,player))return 0;n=sizeof(input);break; }
    case COMMANDTYPE_SHIP_APPEARANCE: {
        NetShipAppearanceCommand input;HAVE(NetShipAppearanceCommand);memcpy(&input,p,sizeof(input));
        if(input.reserved || input.appearance.mask>=64 || !owned(input.ship,player))return 0;
        n=sizeof(input);break; }
    /* Teams are fixed by the host. No raw spawns, captain transfer, file upload,
       dropped-player declarations or developer commands from clients. */
    default:return 0;
    }
#undef HAVE
#undef SELECT_AT
    return n==(size_t)(end-p);
}
