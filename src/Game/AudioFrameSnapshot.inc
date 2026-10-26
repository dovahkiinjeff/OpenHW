/* Native ownership queries can lazily create model instances. Snapshot their
   answers on the game thread before the audio worker borrows the render list. */
static Ship **soundFrameHarvestOwners;
static size_t soundFrameHarvestCount,soundFrameHarvestCapacity;
static bool32 soundFrameHarvestSnapshot;
static bool32 soundEventFrameOwnsHarvestSound(Ship *ship)
{
    size_t i;
    if (!soundFrameHarvestSnapshot) return rmPresentationOwnsHarvestSound(ship);
    for(i=0;i<soundFrameHarvestCount;++i) if(soundFrameHarvestOwners[i]==ship) return TRUE;
    return FALSE;
}
static bool32 soundEventSnapshotHarvestOwners(void)
{
    Node *node; soundFrameHarvestCount=0;
    for(node=universe.RenderList.tail;node;node=node->prev) {
        SpaceObj *object=(SpaceObj *)listGetStructOfNode(node);Ship *ship;
        if(object->objtype!=OBJ_ShipType) continue;
        ship=(Ship *)object;
        if(ship->shiptype!=ResourceCollector || !ship->rceffect || !rmPresentationOwnsHarvestSound(ship)) continue;
        if(soundFrameHarvestCount==soundFrameHarvestCapacity) {
            size_t capacity=soundFrameHarvestCapacity?soundFrameHarvestCapacity*2:16;
            Ship **owners=(Ship **)realloc(soundFrameHarvestOwners,capacity*sizeof(*owners));
            if(!owners) return FALSE;
            soundFrameHarvestOwners=owners;soundFrameHarvestCapacity=capacity;
        }
        soundFrameHarvestOwners[soundFrameHarvestCount++]=ship;
    }
    soundFrameHarvestSnapshot=TRUE;return TRUE;
}

