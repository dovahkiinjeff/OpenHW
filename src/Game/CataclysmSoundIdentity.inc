/* Retail Cataclysm ShipDefs.h IDs, not OpenHW/HW1 saved ship IDs. */
static sdword cataSoundShipObject(const Ship *ship)
{
    static const sdword somtaaw[]={0,1,4,18,14,16,6,15,27,24,20,28,29,30,32,23,21,22,19,122,114,2,5};
    static const struct {ShipType type;sdword kushan,taiidan;} shared[]={
        {AdvanceSupportFrigate,60,74},{AttackBomber,61,75},{Carrier,62,76},
        {CloakGenerator,63,77},{GravWellGenerator,64,79},{HeavyCorvette,65,-1},
        {HeavyCruiser,66,80},{HeavyDefender,67,81},{HeavyInterceptor,68,82},
        {IonCannonFrigate,69,83},{MultiGunCorvette,70,86},{ProximitySensor,71,87},
        {SalCapCorvette,72,89},{StandardDestroyer,73,90},{DFGFrigate,-1,78},
        {MinelayerCorvette,-1,84},{MissileDestroyer,-1,85},
        {ResourceCollector,-1,88},{StandardFrigate,-1,91}};
    unsigned i;
    if(IS_CATA_SHIP(ship->shiptype))return somtaaw[ship->shiptype-CATA_FIRST_SHIP];
    for(i=0;i<sizeof(shared)/sizeof(shared[0]);++i)
        if(shared[i].type==ship->shiptype)
            return ship->shiprace==R1?shared[i].kushan:ship->shiprace==R2?shared[i].taiidan:-1;
    return -1; /* An absent retail sound stays silent; never substitute HW1/RM. */
}
