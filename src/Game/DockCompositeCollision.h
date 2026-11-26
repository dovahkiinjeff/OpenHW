#ifndef HW_DOCK_COMPOSITE_COLLISION_H
#define HW_DOCK_COMPOSITE_COLLISION_H
/* The Kuun-Lan's modules are collision objects sharing its docking hull.
   Apply the existing dock-with-host collision exemption to those parts too. */
static int dockCompositeContact(const Ship *visitor,const Ship *part)
{
    return visitor->dockingship && visitor->dockingship->shiptype==CataMothership &&
        part->clampInfo && part->clampInfo->host==(const SpaceObjRotImpTarg *)visitor->dockingship;
}
#endif
