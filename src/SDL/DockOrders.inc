void mrDockingOrders(char *string, featom *atom)
{
    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bDock)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }

    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    Ship *target=mrDockOrderTarget(atom);
    if (mrDockMenuHadStationTarget(atom) && target==NULL) return;
    makeShipsDockCapable((SelectCommand *)&selSelected); //filter ships that can't dock

    if (selSelected.numShips > 0)
    {
        soundEvent(NULL, UI_Click);
        clWrapDock(&universe.mainCommandLayer,(SelectCommand *)&selSelected,
                   target?DOCK_AT_SPECIFIC_SHIP:dockGetAppropriateTypeOfDocking((SelectCommand *)&selSelected),target);
    }
    else
    {
        soundEvent(NULL, UI_ClickCancel);
    }
#if MR_VERBOSE_LEVEL >= 1
    dbgMessage("Docking orders");
#endif
}
