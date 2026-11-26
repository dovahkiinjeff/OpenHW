bool32 soundEventWorldAudioAllowed(void)
{
    return !smSensorsActive && mrRenderMainScreen &&
        (!soundEventUsesCataclysm() || (!cmActive && !lmActive && !rmGUIActive));
}
static void soundEventPruneWorldLoops(void)
{
    Node *node;bool32 audible;
    if(!soundeventinited || !soundEventUsesCataclysm())return;
    audible=soundEventWorldAudioAllowed();
    /* Render-list-only updates cannot retire an ambient owner after it
       leaves that list. Sweep authoritative owners before the audio worker. */
    for(node=universe.ShipList.head;node;node=node->next){
        Ship *ship=listGetStructOfNode(node);
        if(!audible || !univSpaceObjInRenderList((SpaceObj*)ship))soundEventShipRemove(ship);
    }
    for(node=universe.DerelictList.head;node;node=node->next){
        Derelict *derelict=listGetStructOfNode(node);
        if(!audible || !univSpaceObjInRenderList((SpaceObj*)derelict))soundEventDerelictRemove(derelict);
    }
}
