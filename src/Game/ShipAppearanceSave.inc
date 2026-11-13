#define SHIP_PAINT_SAVE_MAGIC 0x5041494e
void shipAppearanceSave(void)
{
    unsigned i;sdword count=0,j;
    ShipAppearance appearance;const Ship *ship;
    for(i=0;i<65536;++i) if((ship=shipAppearanceAt(i,&appearance))!=NULL && SpaceObjRegistryGetID((SpaceObj *)ship)>=0)++count;
    SaveInfoNumber(SHIP_PAINT_SAVE_MAGIC);SaveInfoNumber(1);SaveInfoNumber(count);
    for(i=0;i<65536;++i) if((ship=shipAppearanceAt(i,&appearance))!=NULL) {
        sdword id=SpaceObjRegistryGetID((SpaceObj *)ship);
        if(id<0)continue;
        SaveInfoNumber(id);SaveInfoNumber((sdword)appearance.mask);
        for(j=0;j<SHIP_PAINT_COUNT;++j)SaveInfoNumber((sdword)appearance.colors[j]);
    }
}
void shipAppearanceLoad(void)
{
    sdword tag,count,i,j,version;
    if(!LoadInfoNumberOptional(&tag) || tag!=SHIP_PAINT_SAVE_MAGIC)return;
    version=LoadInfoNumber();count=LoadInfoNumber();
    if(version!=1 || count<0 || count>65536)return;
    for(i=0;i<count;++i) {
        sdword id=LoadInfoNumber();ShipAppearance appearance;
        appearance.mask=(udword)LoadInfoNumber();
        for(j=0;j<SHIP_PAINT_COUNT;++j)appearance.colors[j]=(color)LoadInfoNumber();
        if(id>=0 && id<SpaceObjRegistry.selection->numShips) {
            Ship *ship=SpaceObjRegistry.selection->ShipPtr[id];
            if(ship && ship->objtype==OBJ_ShipType)shipAppearanceApply(ship,&appearance);
        }
    }
}
