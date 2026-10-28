/* Reserve Shift+Ctrl+double-click for additive visible-type selection.
   Ordinary double-click must reach the original dock/support action. */
static bool32 mrSameTypeSelectionChord(void)
{
    return (keyIsHit(SHIFTKEY) || keyIsHit(RSHIFTKEY)) &&
           (keyIsHit(CONTROLKEY) || keyIsHit(RCONTROLKEY)) &&
           !keyIsHit(ALTKEY) && !keyIsHit(RALTKEY);
}
static bool32 mrSelectVisibleShipsOfSameType(Ship *clicked)
{
    Node *node;
    if(!mrSameTypeSelectionChord())return FALSE;

    if (clicked == NULL || clicked->objtype != OBJ_ShipType ||
        clicked->playerowner != universe.curPlayerPtr ||
        clicked->shiptype == Drone || !(clicked->flags & SOF_Selectable) ||
        (clicked->flags & (SOF_Dead|SOF_Hide|SOF_Hyperspace)))
        return FALSE;

    for (node = universe.RenderList.head; node != NULL; node = node->next)
    {
        Ship *candidate = (Ship *)listGetStructOfNode(node);
        sdword centerX, centerY, radius;
        if (candidate->objtype != OBJ_ShipType ||
            candidate->playerowner != clicked->playerowner ||
            candidate->shiptype != clicked->shiptype ||
            candidate->shiptype == Drone ||
            !(candidate->flags & SOF_Selectable) ||
            (candidate->flags & (SOF_Dead|SOF_Hide|SOF_Hyperspace)))
            continue;
        if (bitTest(candidate->flags, SOF_Slaveable) &&
            !bitTest(candidate->slaveinfo->flags, SF_MASTER))
            continue;
        if (candidate->collInfo.selCircleRadius <= 0.0f) continue;

        centerX = primGLToScreenX(candidate->collInfo.selCircleX);
        centerY = primGLToScreenY(candidate->collInfo.selCircleY);
        radius = ABS(primGLToScreenScaleX(candidate->collInfo.selCircleRadius));
        if (centerX + radius < 0 || centerX - radius >= MAIN_WindowWidth ||
            centerY + radius < 0 || centerY - radius >= MAIN_WindowHeight)
            continue;
        selSelectionAddSingleShip(&selSelected, candidate);
    }
    ioUpdateShipTotals();
    tutGameMessage("Game_DoubleClickSelect");
    return TRUE;
}

