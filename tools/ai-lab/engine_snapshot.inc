/* Observational snapshots: no RNG calls or gameplay mutations. Private build only. */
#include "AITeam.h"
#include "CommandLayer.h"
#include "CommandDefs.h"
#include <math.h>
static double labFinite(double v){return isfinite(v)?v:0;}
static void aiLabSnapshot(uint32_t frame)
{
 unsigned p,i;Node *node;long long ru=0;
 for(node=universe.ResourceList.head;node;node=node->next)ru+=((Resource*)listGetStructOfNode(node))->resourcevalue;
 fprintf(stderr,"AI_LAB\t{\"kind\":\"world\",\"frame\":%u,\"time\":%.4f,\"resources\":%d,\"ru\":%lld}\n",frame,universe.totaltimeelapsed,universe.ResourceList.num,ru);
 for(p=0;p<universe.numPlayers;p++) {
  Player *pl=&universe.players[p];AIPlayer *ai=pl->aiPlayer;
  fprintf(stderr,"AI_LAB\t{\"kind\":\"player\",\"frame\":%u,\"player\":%u,\"alive\":%d,\"race\":%d,\"allies\":%u,\"ru\":%d,\"ships\":%d,\"tech\":%u,\"types\":[",frame,p,pl->playerState==PLAYER_ALIVE,pl->race,pl->Allies,pl->resourceUnits,pl->totalships,pl->researchinfo.HasTechnology);
  for(i=0;i<TOTAL_NUM_SHIPS;i++)fprintf(stderr,"%s%d",i?",":"",pl->shiptotals[i]);
  fprintf(stderr,"],\"labs\":[");
  for(i=0;i<NUM_RESEARCHLABS;i++) {
   ResearchLab *lab=&pl->researchinfo.researchlabs[i];
   fprintf(stderr,"%s[%d,%d,%.3f]",i?",":"",lab->labstatus,lab->topic?(int)lab->topic->techresearch:-1,lab->topic?labFinite(lab->topic->timeleft):0);
  }
  fprintf(stderr,"]");
  if(ai)fprintf(stderr,",\"difficulty\":%d,\"enemy\":%d,\"teams\":%d,\"new_ships\":%d,\"attack_requests\":%d,\"defense_requests\":%d,\"waiting_attack\":%d,\"waiting_defense\":%d,\"harvesters\":%d,\"researchers\":%d,\"hyper_savings\":%d,\"accessible_ru\":%d,\"spent\":[%d,%d,%d,%d]",ai->aiplayerDifficultyLevel,ai->primaryEnemyPlayer?(int)ai->primaryEnemyPlayer->playerIndex:-1,ai->teamsUsed,ai->newships.selection->numShips,ai->AttackManRequestShipsQ.num,ai->DefenseManRequestShipsQ.num,ai->AttackManTeamsWaitingForShipsQ.num,ai->DefenseManTeamsWaitingForShipsQ.num,ai->airNumRCollectors,ai->airNumResearchShips,ai->aifHyperSavings,ai->airEasilyAccesibleRUsInWorld,ai->NumRUsSpentOnAttman,ai->NumRUsSpentOnDefman,ai->NumRUsSpentOnResourceman,ai->NumRUsSpentOnScriptman);
  fprintf(stderr,"}\n");
  if(ai)for(i=0;i<(unsigned)ai->teamsUsed;i++) {
   unsigned s;AITeam *t=ai->teams[i];if(!t)continue;
   fprintf(stderr,"AI_LAB\t{\"kind\":\"team\",\"frame\":%u,\"player\":%u,\"id\":%u,\"type\":%d,\"flags\":%u,\"move\":%d,\"ships\":[",frame,p,i,t->teamType,t->teamFlags,t->curMove?(int)t->curMove->type:-1);
   for(s=0;s<(unsigned)t->shipList.selection->numShips;s++)fprintf(stderr,"%s%u",s?",":"",t->shipList.selection->ShipPtr[s]->shipID.shipNumber);
   fprintf(stderr,"]}\n");
  }
 }
 for(node=universe.ShipList.head;node;node=node->next) {
  Ship *s=(Ship*)listGetStructOfNode(node);CommandToDo *c=s->command;vector pos=s->posinfo.position;
  fprintf(stderr,"AI_LAB\t{\"kind\":\"ship\",\"frame\":%u,\"id\":%u,\"player\":%u,\"type\":%d,\"race\":%d,\"health\":%.2f,\"max_health\":%.2f,\"cargo\":%d,\"order\":%d,\"position_valid\":%d,\"position\":[%.2f,%.2f,%.2f]",frame,s->shipID.shipNumber,s->playerowner->playerIndex,(int)s->shiptype,(int)s->shiprace,labFinite(s->health),labFinite(s->staticinfo->maxhealth),s->resources,c?(int)c->ordertype.order:0,isfinite(pos.x)&&isfinite(pos.y)&&isfinite(pos.z),labFinite(pos.x),labFinite(pos.y),labFinite(pos.z));
  if(c && c->attack && c->ordertype.order==COMMAND_ATTACK) {
   fprintf(stderr,",\"targets\":[");
   for(i=0;i<(unsigned)c->attack->numTargets;i++) {
    SpaceObjRotImpTarg *target=c->attack->TargetPtr[i];
    fprintf(stderr,"%s%d",i?",":"",target && target->objtype==OBJ_ShipType?(int)((Ship*)target)->shipID.shipNumber:-1);
   }
   fprintf(stderr,"]");
  }
  fprintf(stderr,"}\n");
 }
 for(node=universe.mainCommandLayer.todolist.head;node;node=node->next) {
  CommandToDo *c=(CommandToDo*)listGetStructOfNode(node);
  if(c->ordertype.order==COMMAND_BUILDING_SHIP)fprintf(stderr,"AI_LAB\t{\"kind\":\"build\",\"frame\":%u,\"player\":%u,\"type\":%d,\"factory\":%d,\"finish_tick\":%u}\n",frame,c->buildingship.playerIndex,(int)c->buildingship.shipType,c->buildingship.creator?(int)c->buildingship.creator->shipID.shipNumber:-1,c->buildingship.frameAtWhichToCreate);
 }
 fprintf(stderr,"AI_LAB\t{\"kind\":\"snapshot_end\",\"frame\":%u}\n",frame);fflush(stderr);
}
