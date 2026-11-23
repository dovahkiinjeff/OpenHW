/* Technology closure, lab assignments and AI research policy. */
ResearchTopic *Researching(Player *player, TechnologyType tech)
{
    PlayerResearchInfo *research=&player->researchinfo;
    ResearchTopic      *topic;
    Node               *walk;

    walk = research->listoftopics.head;

    // walk through list of topics being researched
    while (walk != NULL)
    {
        topic = (ResearchTopic *)listGetStructOfNode(walk);
        if (topic->techresearch == tech)
            return(topic);
        walk = walk->next;
    }

    return(NULL);
}

void rmClearResearchlab(Player *player, sdword labnumber)
{
    ResearchLab   *lab = &player->researchinfo.researchlabs[labnumber];
    ResearchTopic *topic = player->researchinfo.researchlabs[labnumber].topic;

    if (lab->labstatus==LS_RESEARCHITEM)
    {
        if (topic->numlabsresearching==1)
        {
            lab->topic = NULL;
            listDeleteNode(&topic->link);
            lab->labstatus = LS_NORESEARCHITEM;
        }
        else
        {
            lab->topic = NULL;
            lab->labstatus = LS_NORESEARCHITEM;
            topic->numlabsresearching--;
        }
        if ((player == universe.curPlayerPtr)&&(rmGUIActive)) rmUpdateTechList();
    }
}

void rmAssignPlayersLabToResearch(Player *player, sdword labnumber, TechnologyType tech)
{
    ResearchTopic *topic;
    if ((topic=Researching(player, tech))!=NULL)
    {
        player->researchinfo.researchlabs[labnumber].labstatus = LS_RESEARCHITEM;
        player->researchinfo.researchlabs[labnumber].topic = topic;
        topic->numlabsresearching++;
    }
    else
    {
        topic = (ResearchTopic *)memAlloc(sizeof(ResearchTopic),"ResearchTopic",NonVolatile);
        listAddNode(&player->researchinfo.listoftopics, &topic->link, topic);
        player->researchinfo.researchlabs[labnumber].labstatus = LS_RESEARCHITEM;
        player->researchinfo.researchlabs[labnumber].topic = topic;
        topic->numlabsresearching = 1;
        topic->techresearch       = tech;
        topic->timeleft           = (real32)player->researchinfo.techstat->TimeToComplete[tech];
        if (aiIsExpert(player)) topic->timeleft *= 0.9f;
    }

    if ((player == universe.curPlayerPtr)&&(rmGUIActive)) rmUpdateTechList();
}

static udword rmExpandTechDependencies(Player *player, udword mask)
{
    sdword pass, tech;
    mask &= AllTechnology;
    for (pass = 0; pass < NumTechnologies; ++pass)
    {
        udword previous = mask;
        for (tech = 0; tech < NumTechnologies; ++tech)
            if (mask & TechToBit(tech))
                mask |= player->researchinfo.techstat->TechNeededToResearch[tech] & AllTechnology;
        if (mask == previous) break;
    }
    return mask;
}

udword rmAllTechRequiredforTech(Player *player, TechnologyType type)
{
    return rmExpandTechDependencies(player, player->researchinfo.techstat->TechNeededToResearch[type]);
}

udword rmAllTechRequredForShip(Player *player, ShipType type)
{
    udword mask = player->researchinfo.techstat->TechNeededToBuildShip[type];
    if (mask & RM_Disabled) return 0;
    return rmExpandTechDependencies(player, mask) & ~player->researchinfo.HasTechnology;
}

static udword rmExpertRushMask(Player *player)
{
    if (!aiIsExpert(player) || !player->researchinfo.CanDoResearch ||
        !player->researchinfo.techstat) return 0;
    return rmAllTechRequredForShip(player, HeavyCruiser) |
           rmAllTechRequredForShip(player, MissileDestroyer);
}

static bool32 rmExpertReservedLab(Player *player, sdword index)
{
    sdword lab, count = 0, rank = 0;
    if (!rmExpertRushMask(player)) return FALSE;
    for (lab = 0; lab < NUM_RESEARCHLABS; ++lab)
    {
        if (player->researchinfo.researchlabs[lab].labstatus == LS_NORESEARCHSHIP) continue;
        if (lab < index) ++rank;
        ++count;
    }
    return rank < (count + 1) / 2;
}

static sdword rmFindNormalAILab(Player *player)
{
    sdword lab;
    for (lab = 0; lab < NUM_RESEARCHLABS; ++lab)
        if (player->researchinfo.researchlabs[lab].labstatus == LS_NORESEARCHITEM &&
            !rmExpertReservedLab(player, lab)) return lab;
    return -1;
}

void rmUpdateExpertResearch(Player *player)
{
    udword needed = rmExpertRushMask(player);
    sdword lab, tech, chosen = -1;
    if (!needed) return;
    /* Shared prerequisites serve both rush targets. Continue existing progress
       rather than canceling/restarting topics every strategic update. */
    for (tech = 0; tech < NumTechnologies; ++tech)
        if ((needed & TechToBit(tech)) &&
            !(player->researchinfo.techstat->TechNeededToResearch[tech] & ~player->researchinfo.HasTechnology))
        {
            if (chosen == -1) chosen = tech;
            if (Researching(player, (TechnologyType)tech)) { chosen = tech; break; }
        }
    for (lab = 0; lab < NUM_RESEARCHLABS; ++lab)
    {
        ResearchLab *module = &player->researchinfo.researchlabs[lab];
        if (module->labstatus == LS_NORESEARCHSHIP || !rmExpertReservedLab(player, lab)) continue;
        if (module->labstatus == LS_RESEARCHITEM && module->topic &&
            (needed & TechToBit(module->topic->techresearch))) continue;
        if (chosen == -1) continue;
        if (module->labstatus == LS_RESEARCHITEM) rmClearResearchlab(player, lab);
        rmAssignPlayersLabToResearch(player, lab, (TechnologyType)chosen);
    }
}

sdword rmTechRequiredForShip(Player *player, ShipType type)
{
    sdword i, numtech=0;
    udword techneeded, temp;

    temp = player->researchinfo.techstat->TechNeededToBuildShip[type];
    techneeded = temp;

    for (i=0;i<NumTechnologies;i++)
    {
        if (bitTest(temp, TechToBit(i)))
        {
            techneeded |= rmAllTechRequiredforTech(player, i);
        }
    }

    techneeded &= (~player->researchinfo.HasTechnology);

    for (i=0;i<NumTechnologies;i++)
    {
        if (bitTest(techneeded, TechToBit(i)))
        {
            numtech++;
        }
    }

    return (numtech);
}

bool32 rmResearchTechForShip(struct Player *player, ShipType type)
{
    udword numtech, techneeded;
    sdword freelab;
    sdword techtoresearch = -1;
    sdword i;
    bool32 researching = FALSE;

    numtech = rmTechRequiredForShip(player, type);

    if (numtech > 0)
    {
        techneeded = rmAllTechRequredForShip(player, type);

        for (i=0;i<NumTechnologies;i++)
        {
            if (bitTest(techneeded, TechToBit((udword)i)))
            {
                if ((player->researchinfo.techstat->TechNeededToResearch[i]&(~player->researchinfo.HasTechnology)) == 0)
                {
                    techtoresearch = i;
                    freelab = rmFindNormalAILab(player);
                    if (freelab != -1)
                    {
                        rmAssignPlayersLabToResearch(player,freelab, techtoresearch);
                        researching = TRUE;
                    }
                }
            }
        }
    }

    if (techtoresearch != -1)
    {
        while ((freelab = rmFindNormalAILab(player))!=-1)
        {
            rmAssignPlayersLabToResearch(player,freelab, techtoresearch);
            researching = TRUE;
        }
    }
    return researching;
}

