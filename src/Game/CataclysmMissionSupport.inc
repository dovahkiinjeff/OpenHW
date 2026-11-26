/* Typed imports from the owned recovery. These helpers deliberately preserve
   the Cata distinctions from similarly named HW1 APIs: order attributes are
   combined across all ships, and attackers are not filtered by proximity. */
/* Original 004998c0 -> 00417260. Cata's numeric move 22 is
   FANCYGETSHIPS; the host enum uses 25. Never compare retail enum integers. */
static bool32 cataScriptTeamIdle(AITeam *team)
{
    return team && (!team->curMove || team->curMove->type==MOVE_DONE ||
        team->curMove->type==MOVE_GETSHIPS || team->curMove->type==MOVE_FANCYGETSHIPS);
}
/* 00492d60: preserve the actual object type/attribute tests, rather than the
   reconstructed "cloaked ships" label. This is a generic object selection. */
static sdword cataScriptFindMarkedAsteroids(GrowSelection *result)
{
    Node *node;if(!result)return 0;growSelectReset(result);
    for(node=universe.SpaceObjList.head;node;node=node->next){SpaceObj *object=listGetStructOfNode(node);
        if((object->objtype==OBJ_AsteroidType && (object->attributes&0x200u)) ||
           (object->objtype==OBJ_ShipType && cataDisguiseIsAsteroid((Ship*)object)))
            growSelectAddShip(result,(Ship*)object);}
    return result->selection->numShips;
}
/* Original 00493400 queries the first ship's inside/docked flag. The host's
   bit 3 means CircularGuard; its existing hide-without-hyperspace test is the
   matching inside-hangar representation used by kasfShipsSelectNotDocked. */
static bool32 cataScriptFirstShipDocked(GrowSelection *ships)
{
    Ship *ship;if(!ships || !ships->selection->numShips)return FALSE;
    ship=ships->selection->ShipPtr[0];return (ship->flags&(SOF_Hide|SOF_Hyperspace))==SOF_Hide;
}
static sdword cataScriptOrderAttributes(GrowSelection *ships)
{
    sdword i,attributes=0;
    if(ships)for(i=0;i<ships->selection->numShips;i++){
        Ship *ship=ships->selection->ShipPtr[i];
        if(ship->command)attributes|=ship->command->ordertype.attributes;
    }
    return attributes;
}
/* Original 00498630 / 00494450. Output may alias the input selection: retain
   a copy before clearing it, as the original callers use separate stores. */
static sdword cataScriptFindAttackers(GrowSelection *targets,GrowSelection *result)
{
    Node *node;GrowSelection copy;sdword i;
    if(!result)return 0;
    growSelectInit(&copy);
    if(targets)for(i=0;i<targets->selection->numShips;i++)
        growSelectAddShip(&copy,targets->selection->ShipPtr[i]);
    growSelectReset(result);
    if(copy.selection->numShips)for(node=universe.mainCommandLayer.todolist.head;node;node=node->next){
        CommandToDo *command=(CommandToDo *)listGetStructOfNode(node);
        if((command->ordertype.order==COMMAND_ATTACK ||
            (command->ordertype.attributes&(COMMAND_MASK_PASSIVE_ATTACKING|COMMAND_MASK_ATTACKING_AND_MOVING))) &&
            command->attack && AnyOfTheseShipsAreInSelection(copy.selection,(SelectCommand *)command->attack)){
            for(i=0;i<command->selection->numShips;i++)
                growSelectAddShipNoDuplication(result,command->selection->ShipPtr[i]);
        }
    }
    growSelectClose(&copy);return result->selection->numShips;
}
static void cataScriptVariableDestroy(uint32_t id)
{
    char key[32];
    cataScriptVariableKey(cataKasScope,id,key,sizeof(key));
    if(aivarFind(key))aivarDestroy(aivarFind(key));
}
static void cataScriptTimerDestroy(uint32_t id)
{
    char key[32];cataScriptScopedKey(cataKasScope,id,key,sizeof(key));timTimerDestroy(key);
}
static void cataScriptSelectionCopy(GrowSelection *copy,GrowSelection *source)
{
    sdword i;growSelectInit(copy);
    if(source)for(i=0;i<source->selection->numShips;i++)growSelectAddShip(copy,source->selection->ShipPtr[i]);
}
/* 004954d0 -> predicate 004954f0, arg1=0. This filters ownership and
   original script-control status, not salvage hull classes. Original hull
   9 is the Hive Frigate's Swarmer; never compare with host ShipType 9. */
static sdword cataScriptSelectPlayerCombatShips(GrowSelection *result,GrowSelection *source)
{
    GrowSelection input;sdword i;if(!result)return 0;
    cataScriptSelectionCopy(&input,source);growSelectReset(result);
    for(i=0;i<input.selection->numShips;i++){
        Ship *ship=input.selection->ShipPtr[i];
        if(ship->playerowner->playerIndex==0 &&
           !(ship->specialFlags2&SPECIAL_2_CataScriptControlled) && cataShipOriginalID(ship)!=9)
            growSelectAddShip(result,ship);
    }
    growSelectClose(&input);return result->selection->numShips;
}
/* Original 00495410/00495430: intersection with a supplied selection. */
static sdword cataScriptSelectMembers(GrowSelection *result,GrowSelection *source,GrowSelection *members)
{
    GrowSelection input,match;sdword i,j;
    if(!result)return 0;
    cataScriptSelectionCopy(&input,source);cataScriptSelectionCopy(&match,members);growSelectReset(result);
    for(i=0;i<input.selection->numShips;i++)for(j=0;j<match.selection->numShips;j++)
        if(input.selection->ShipPtr[i]==match.selection->ShipPtr[j]){
            growSelectAddShip(result,input.selection->ShipPtr[i]);break;
        }
    growSelectClose(&input);growSelectClose(&match);return result->selection->numShips;
}
/* Original 0049ad80 traverses shipsInsideMe. The reconstructed name
   "linked ships" does not mean Kuun-Lan subsystem attachments. */
static sdword cataScriptSelectInside(GrowSelection *result,GrowSelection *source)
{
    GrowSelection input;sdword i;Node *node;
    if(!result)return 0;cataScriptSelectionCopy(&input,source);growSelectReset(result);
    for(i=0;i<input.selection->numShips;i++){
        Ship *host=input.selection->ShipPtr[i];
        if(!host->shipsInsideMe)continue;
        for(node=host->shipsInsideMe->insideList.head;node;node=node->next)
            growSelectAddShipNoDuplication(result,((InsideShip*)listGetStructOfNode(node))->ship);
    }
    growSelectClose(&input);return result->selection->numShips;
}
/* 00495180/004951d0 selects actors physically attached to the first target;
   original iterator 004fc650 accepts the five salvager hull identities. */
static sdword cataScriptSelectSalvagers(GrowSelection *result,GrowSelection *source,GrowSelection *targets)
{
    GrowSelection input;Ship *target;sdword i;if(!result)return 0;
    target=targets && targets->selection->numShips?targets->selection->ShipPtr[0]:NULL;
    cataScriptSelectionCopy(&input,source);growSelectReset(result);
    if(target)for(i=0;i<input.selection->numShips;i++){
        Ship *ship=input.selection->ShipPtr[i];
        if((ship->shiptype==CataWorker || ship->shiptype==SalCapCorvette || ship->shiptype==P1StandardCorvette) &&
           ship->clampInfo && ship->clampInfo->host==(SpaceObjRotImpTarg*)target)growSelectAddShip(result,ship);
    }
    growSelectClose(&input);return result->selection->numShips;
}
/* Retail 00499c70 derives the race from the requested ship identity.
   OpenHW shares standard ship IDs across Kushan and Taiidan, whose original
   names retain the k/t prefix. Slipgates use R1 storage even for P1 launches. */
static ShipRace cataScriptReinforcementRace(ShipType type,const char *name,ShipRace hostRace)
{
    if(type>STD_LAST_SHIP)return GetValidRaceForShipType(type);
    if(name[0]=='k' || name[0]=='K')return R1;
    if(name[0]=='t' || name[0]=='T')return R2;
    return hostRace;
}

/* 004995e0 -> 0052a060 tests bit 14 of original ship static flags.
   0053b8c0 sets that bit for dSlipgateSmall/Medium/Large (112..114).
   The recovered SSTATF_BeastShip name is incorrect. */
static sdword cataScriptSelectGates(Volume *volume,GrowSelection *result)
{
    Node *node;if(!result)return 0;growSelectReset(result);if(!volume)return 0;
    for(node=universe.ShipList.head;node;node=node->next){Ship *ship=listGetStructOfNode(node);
        if(ship->shiptype==CataSlipgateLarge && !(ship->flags&(SOF_Dead|SOF_Hide|SOF_Hyperspace)) &&
           volPointInside(volume,&ship->posinfo.position))growSelectAddShip(result,ship);}
    return result->selection->numShips;
}
/* Original 0049a790 returns targets of passive-attack commands containing
   the input actors, unlike the inverse attacker query above. */
static sdword cataScriptOrderTargets(GrowSelection *actors,GrowSelection *result)
{
    GrowSelection input,found;Node *node;sdword i;
    if(!result)return 0;cataScriptSelectionCopy(&input,actors);growSelectInit(&found);
    if(input.selection->numShips)for(node=universe.mainCommandLayer.todolist.head;node;node=node->next){
        CommandToDo *command=(CommandToDo*)listGetStructOfNode(node);
        if(!(command->ordertype.attributes&COMMAND_MASK_PASSIVE_ATTACKING) || !command->attack ||
           !AnyOfTheseShipsAreInSelection(input.selection,command->selection))continue;
        for(i=0;i<command->attack->numTargets;i++)
            growSelectAddShipNoDuplication(&found,(Ship*)command->attack->TargetPtr[i]);
    }
    growSelectReset(result);
    for(i=0;i<found.selection->numShips;i++)growSelectAddShip(result,found.selection->ShipPtr[i]);
    growSelectClose(&found);growSelectClose(&input);return result->selection->numShips;
}
