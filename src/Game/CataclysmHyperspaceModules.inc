static bool32 spCanQueueScriptedHyperspace(const Ship *ship, bool32 arriving)
{
    /* Older saves may already contain an independently queued module. */
    return !cataShipIsAttachedComponent(ship) ||
        (arriving && bitTest(ship->flags, SOF_Hyperspace));
}

/* Carry installed modules with their host through the existing save and
   mission-loading path. They return to their native HOD mounts on arrival. */
static void spStowCataclysmModules(Ship *host)
{
    Node *node, *next;

    if (host->shiptype != CataMothership)
        return;

    for (node = universe.ShipList.head; node != NULL; node = next)
    {
        Ship *module = listGetStructOfNode(node);
        next = node->next;
        if (cataShipComponentHost(module) != host || bitTest(module->flags, SOF_Dead))
            continue;

        dockEnsureShipsInside(host);
        RemoveShipFromCommand(module);
        univRemoveShipFromOutside(module);
        dockPutShipInside(module, host);
    }
}

static void spRestoreCataclysmModules(Ship *host)
{
    Node *node, *next;

    if (host->shiptype != CataMothership || host->shipsInsideMe == NULL)
        return;

    for (node = host->shipsInsideMe->insideList.head; node != NULL; node = next)
    {
        Ship *module = ((InsideShip *)listGetStructOfNode(node))->ship;
        next = node->next;
        if (cataShipComponentHost(module) != host)
            continue;

        dockRemoveShipFromInside(module, host);
        PutShipOutsideAt(module, &host->posinfo.position, &host->posinfo.velocity, 0.0f);
    }
}
