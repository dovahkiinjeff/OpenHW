/* Shared production scheduler; also exercised without a renderer. */
/* Account for commands emitted into multiplayer packets but not applied yet. */
static struct { Ship *factory; ShipType type; udword finish; } aifBuildReservations[256];
static sdword aifBuildReservationCount;

/* Stable list order breaks ties identically on clients and the server. */
Ship *aifChooseFactory(Player *player, ShipType type, ShipRace race)
{
    Node *node, *queued;
    Ship *best = NULL;
    udword bestFinish = 0xffffffffu;
    for (node = universe.ShipList.head; node; node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        udword finish = universe.univUpdateCounter;
        if (ship->playerowner != player ||
            (ship->flags & (SOF_Dead | SOF_Disabled | SOF_Hyperspace | SOF_Crazy)) ||
            !ship->staticinfo->canBuildShips || cmFactoryBuildRace(ship) != race ||
            !cmCanBuildShipType(ship, type, FALSE)) continue;
        /* Each type has its own construction lane in the legacy simulation. */
        for (queued = universe.mainCommandLayer.todolist.head; queued; queued = queued->next)
        {
            CommandToDo *command = (CommandToDo *)listGetStructOfNode(queued);
            if (command->ordertype.order == COMMAND_BUILDING_SHIP &&
                command->buildingship.creator == ship && command->buildingship.shipType == type &&
                command->buildingship.frameAtWhichToCreate > finish)
                finish = command->buildingship.frameAtWhichToCreate;
        }
        {
            sdword reservation;
            for (reservation = 0; reservation < aifBuildReservationCount; ++reservation)
                if (aifBuildReservations[reservation].factory == ship && aifBuildReservations[reservation].type == type)
                    finish = max(finish, aifBuildReservations[reservation].finish);
        }
        if (!best || finish < bestFinish) { best = ship; bestFinish = finish; }
    }
    return best;
}

static sdword aifExpansionReserve(Player *player)
{
    AIPlayer *ai = player->aiPlayer;
    ShipStaticInfo *info;
    Node *node;
    sdword output, target;
    if (singlePlayerGame || !ai || !player->PlayerMothership || ai->airNumRCollectors < 3 ||
        !aiuCanBuildShipType(Carrier, FALSE) || !aiuUnitCapCanBuildShip(ai, Carrier, 1) ||
        (bitTest(tpGameCreated.flag, MG_ResearchEnabled) && !rmCanBuildShip(player, Carrier))) return 0;
    info = GetShipStaticInfo(Carrier, player->PlayerMothership->shiprace);
    output = player->resourceUnits + ai->NumRUsSpentOnAttman + ai->NumRUsSpentOnDefman + ai->NumRUsSpentOnResourceman;
    target = output / max(1, info->buildCost * aiStrategy(player)->carrierInvestment);
    if (player->shiptotals[Carrier] >= target) return 0;
    for (node = universe.mainCommandLayer.todolist.head; node; node = node->next) {
        CommandToDo *command = (CommandToDo *)listGetStructOfNode(node);
        if (command->ordertype.order == COMMAND_BUILDING_SHIP && command->buildingship.playerIndex == player->playerIndex &&
            command->buildingship.shipType == Carrier) return 0;
    }
    return info->buildCost + 500;
}

static bool32 BuildShip(ShipStaticInfo *info, Player *player, sdword *spent,
                       sdword *available, Ship *creator, bool32 scripted)
{
    sdword cost = aiProductionValue(player, info->buildCost);
    if (!singlePlayerGame && !scripted)
        creator = aifChooseFactory(player, info->shiptype, info->shiprace);
    else if (!creator) creator = player->PlayerMothership;
    if (!creator || *available < cost) return FALSE;
    if (!singlePlayerGame && !scripted)
    {
        Node *node;
        sdword i;
        udword finish = universe.univUpdateCounter;
        for (node = universe.mainCommandLayer.todolist.head; node; node = node->next)
        {
            CommandToDo *command = (CommandToDo *)listGetStructOfNode(node);
            if (command->ordertype.order == COMMAND_BUILDING_SHIP &&
                command->buildingship.creator == creator && command->buildingship.shipType == info->shiptype)
                finish = max(finish, command->buildingship.frameAtWhichToCreate);
        }
        for (i = 0; i < aifBuildReservationCount; ++i)
            if (aifBuildReservations[i].factory == creator && aifBuildReservations[i].type == info->shiptype) break;
        if (i == aifBuildReservationCount)
        {
            /* Retain the unissued request if a pathological uncapped match
               exceeds this many active factory/type lanes in one update. */
            if (i == 256) return FALSE;
            ++aifBuildReservationCount;
            aifBuildReservations[i].factory = creator;
            aifBuildReservations[i].type = info->shiptype;
            aifBuildReservations[i].finish = finish;
        }
        aifBuildReservations[i].finish = max(finish, aifBuildReservations[i].finish) +
            aiProductionValue(player, info->buildTime);
    }
    clWrapBuildShip(&universe.mainCommandLayer, info->shiptype, info->shiprace,
                    player->playerIndex, creator);
    if (info->shiptype == ResourceCollector) aiCurrentAIPlayer->NumRCollectorsBeingBuilt++;
    else if (info->shiptype == ResourceController) aiCurrentAIPlayer->NumRControllersBeingBuilt++;
    else if (info->shiptype == AdvanceSupportFrigate) aiCurrentAIPlayer->NumASFBeingBuilt++;
    else if (info->shiptype == ResearchShip) aiCurrentAIPlayer->NumResearchShipsBeingBuilt++;
    *spent += cost;
    *available -= cost;
    return TRUE;
}

void aifBuildRequestedShips(RequestShips *scriptrequest, RequestShips *attrequest,
                            RequestShips *defrequest, RequestShips *resourcerequest)
{
    Player *player = aiCurrentAIPlayer->player;
    RequestShips *requests[4] = {scriptrequest, attrequest, defrequest, resourcerequest};
    sdword *spent[4] = {&aiCurrentAIPlayer->NumRUsSpentOnScriptman,
        &aiCurrentAIPlayer->NumRUsSpentOnAttman, &aiCurrentAIPlayer->NumRUsSpentOnDefman,
        &aiCurrentAIPlayer->NumRUsSpentOnResourceman};
    real32 weights[4] = {1.0f, ATTMAN_BUILDPRIORITY_RATIO, DEFMAN_BUILDPRIORITY_RATIO, RESMAN_BUILDPRIORITY_RATIO};
    sdword available = player->resourceUnits, i, pass;
    sdword expansionReserve = aifExpansionReserve(player);
    aifBuildReservationCount = 0;
    if (!player->PlayerMothership) return;
    /* Preserve script precedence and the managers' configured spending shares;
       never divide by zero on a new match or consume a failed build request. */
    for (pass = 0; pass < 4; ++pass)
    {
        sdword chosen = -1;
        real32 best = REALlyBig;
        for (i = 0; i < 4; ++i)
        {
            real32 score;
            if (!requests[i] || requests[i]->num_ships <= 0) continue;
            score = (i == 0) ? -2.0f : (requests[i]->priority & REQUESTSHIPS_HIPRI) ? -1.0f :
                (real32)*spent[i] / max(weights[i], 0.001f);
            if (chosen == -1 || score < best) { chosen = i; best = score; }
        }
        if (chosen >= 0)
        {
            RequestShips *request = requests[chosen];
            ShipRace race = player->PlayerMothership->shiprace;
            ShipStaticInfo *info;
            requests[chosen] = NULL;
            if (!(RacesAllowedForGivenShip[request->shiptype] & RaceToRaceBits(race))) continue;
            info = GetShipStaticInfo(request->shiptype, race);
            if (!bitTest(info->staticheader.infoFlags, IF_InfoLoaded)) continue;
            /* Spend toward expansion, but always allow collectors, labs, script
               commands and urgent defense to draw on the full bank. */
            {
            sdword held = (chosen == 1 || (chosen == 2 && !(request->priority & REQUESTSHIPS_HIPRI))) ?
                min(available, expansionReserve) : 0;
            available -= held;
            while (request->num_ships > 0 && BuildShip(info, player, spent[chosen], &available,
                                                     request->creator, chosen == 0))
                --request->num_ships;
            available += held;
            }
        }
    }
    aifBuildReservationCount = 0;
}

