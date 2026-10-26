/* A contextual Dock order must retain the station clicked, rather than
   silently becoming a nearest-carrier order when its menu opens. Never
   dereference a cached object before finding it in the live ship list. */
static Ship *mrDockMenuTarget;
static uword mrDockMenuTargetID;
static fescreen *mrDockMenuScreen;

static Ship *mrLiveMissionDockTarget(const void *candidate, bool32 checkID, uword id)
{
    Node *node;
    if (!singlePlayerGame || candidate == NULL) return NULL;
    for (node=universe.ShipList.head;node!=NULL;node=node->next)
    {
        Ship *ship=(Ship *)listGetStructOfNode(node);
        if ((const void *)ship!=candidate) continue;
        if ((checkID && ship->shipID.shipNumber!=id) || ship->shiprace!=Traders ||
            ship->shiptype!=JunkYardHQ ||
            (ship->flags & (SOF_Dead|SOF_Disabled|SOF_Crazy|SOF_Hyperspace))) return NULL;
        return ship;
    }
    return NULL;
}

static void mrCaptureDockMenuTarget(void)
{
    mrDockMenuTarget=mrLiveMissionDockTarget(mouseCursorObjPtr,FALSE,0);
    mrDockMenuTargetID=mrDockMenuTarget?mrDockMenuTarget->shipID.shipNumber:0;
    mrDockMenuScreen=NULL;
}

static Ship *mrDockOrderTarget(featom *atom)
{
    if (atom!=NULL && mrDockMenuScreen!=NULL &&
        feTempMenuScreen==mrDockMenuScreen && feMenuLevel>0)
    {
        /* If the explicitly chosen station died, cancel this order instead
           of redirecting it to a mothership or another object at that address. */
        return mrLiveMissionDockTarget(mrDockMenuTarget,TRUE,mrDockMenuTargetID);
    }
    return mrLiveMissionDockTarget(mouseCursorObjPtr,FALSE,0);
}

static bool32 mrDockMenuHadStationTarget(featom *atom)
{
    return atom!=NULL && mrDockMenuTarget!=NULL && mrDockMenuScreen!=NULL &&
        feTempMenuScreen==mrDockMenuScreen && feMenuLevel>0;
}
