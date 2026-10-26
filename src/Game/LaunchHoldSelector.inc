static udword lmHoldAutoLaunchMask(const Ship *ship)
{
    Ship *primary, *carriers[4];
    sdword i;
    FillInCarrierMothershipInfo(universe.curPlayerPtr, &primary, carriers);
    if (ship == primary) return BIT0;
    /* Match Dock's first matching carrier slot, including a carrier acting
       as the player's primary ship. Never flip a second ship's launch bit. */
    for (i = 0; i < 4; ++i) if (ship == carriers[i]) return BIT1 << i;
    return 0;
}

static void lmHoldButtonRect(regionhandle region, sdword slot, rectangle *rect)
{
    rect->x0 = region->rect.x0 + uiScaleSize(slot * LM_HOLD_BUTTON_STEP);
    rect->y0 = region->rect.y0 + uiScaleSize(18);
    rect->x1 = rect->x0 + uiScaleSize(LM_HOLD_BUTTON_WIDTH);
    rect->y1 = rect->y0 + uiScaleSize(LM_HOLD_BUTTON_HEIGHT);
}

static void lmHoldNavRect(regionhandle region, bool32 next, rectangle *rect)
{
    rect->x0 = next ? region->rect.x1 - uiScaleSize(58) : region->rect.x0;
    rect->y0 = region->rect.y0 + uiScaleSize(80);
    rect->x1 = rect->x0 + uiScaleSize(58);
    rect->y1 = rect->y0 + uiScaleSize(22);
}

static void lmHoldPolicyRect(regionhandle region, rectangle *rect)
{
    rect->x0 = region->rect.x1 - uiScaleSize(112);
    rect->y0 = region->rect.y0 + uiScaleSize(108);
    rect->x1 = region->rect.x1;
    rect->y1 = rect->y0 + uiScaleSize(18);
}

static bool32 lmHoldHit(const rectangle *rect)
{
    return mouseCursorX() >= rect->x0 && mouseCursorX() < rect->x1 &&
        mouseCursorY() >= rect->y0 && mouseCursorY() < rect->y1;
}

static void lmHoldSelect(Ship *ship, bool32 followPage)
{
    sdword index = lmHoldIndexOf(ship);
    if (index < 0) return;
    if (ship != launchship || shipsin != ship->shipsInsideMe)
    {
        lmReLaunch(ship);
        lmUpdateShipView();
    }
    if (followPage) lmHoldPage = index / LM_HOLD_PAGE_SIZE;
    lmCurrentSelect = 0; /* Legacy FIB selectors are hidden in docked mode. */
    if (lmBaseRegion != NULL) regRecursiveSetDirty(lmBaseRegion);
}

static void lmHoldDraw(regionhandle region)
{
    sdword count = lmHoldCount(), slot, first;
    fonthandle oldfont;
    rectangle rect;
    char text[96];
    if (!lmActive) return;
    if (lmHoldIndexOf(launchship) < 0) lmHoldSelect(lmHoldAt(0), TRUE);
    lmHoldPage = lmHoldClampPage(count, lmHoldPage);
    first = lmHoldPage * LM_HOLD_PAGE_SIZE;
    oldfont = fontMakeCurrent(lmShipListFont);
    fontPrint(region->rect.x0, region->rect.y0, colRGB(222,162,62), "SHIP HOLDS");
    snprintf(text, sizeof(text), "%d-%d OF %d", count ? first + 1 : 0,
        min(count, first + LM_HOLD_PAGE_SIZE), count);
    fontPrint(region->rect.x1 - fontWidth(text), region->rect.y0, FEC_ListItemStandard, text);
    for (slot = 0; slot < LM_HOLD_PAGE_SIZE; ++slot)
    {
        sdword index = lmHoldSlotIndex(count, lmHoldPage, slot);
        Ship *ship = lmHoldAt(index);
        struct tagRegion imageRegion;
        bool32 selected;
        if (ship == NULL) continue;
        selected = ship == launchship;
        lmHoldButtonRect(region, slot, &rect);
        primRectSolid2(&rect, colRGB(12,23,30));
        primRectOutline2(&rect, selected ? 2 : 1,
            selected ? colRGB(222,162,62) : lmHoldHit(&rect) ? colRGB(135,156,159) : colRGB(62,84,94));
        memset(&imageRegion, 0, sizeof(imageRegion));
        imageRegion.rect = rect;
        imageRegion.rect.x0 += uiScaleSize(8);
        imageRegion.rect.x1 -= uiScaleSize(8);
        imageRegion.rect.y1 -= uiScaleSize(14);
        lmDrawShipImage(&imageRegion, ship->shiptype == Mothership ? 0 : 1);
        snprintf(text, sizeof(text), "%d: %d", index + 1, ship->shipsInsideMe->insideList.num);
        fontPrint(rect.x0 + (rect.x1 - rect.x0 - fontWidth(text)) / 2,
            rect.y1 - fontHeight(NULL) - uiScaleSize(1),
            selected ? colRGB(222,162,62) : FEC_ListItemStandard, text);
    }
    for (slot = 0; slot < 2; ++slot)
    {
        bool32 enabled = slot ? lmHoldPage + 1 < lmHoldPageCount(count) : lmHoldPage > 0;
        const char *label = slot ? "NEXT >" : "< PREV";
        lmHoldNavRect(region, slot != 0, &rect);
        primRectSolid2(&rect, colRGB(12,23,30));
        primRectOutline2(&rect, 1, enabled && lmHoldHit(&rect) ? colRGB(222,162,62) : colRGB(62,84,94));
        fontPrint(rect.x0 + (rect.x1 - rect.x0 - fontWidth((char *)label)) / 2,
            rect.y0 + (rect.y1 - rect.y0 - fontHeight(NULL)) / 2,
            enabled ? FEC_ListItemStandard : colRGB(74,82,84), (char *)label);
    }
    fontPrint(region->rect.x0 + uiScaleSize(76), region->rect.y0 + uiScaleSize(85),
        FEC_ListItemStandard, "WHEEL TO BROWSE");
    if (lmHoldIndexOf(launchship) >= 0)
    {
        udword mask = lmHoldAutoLaunchMask(launchship);
        snprintf(text, sizeof(text), "%d: %s", lmHoldIndexOf(launchship) + 1, ShipTypeToStr(launchship->shiptype));
        fontPrint(region->rect.x0, region->rect.y0 + uiScaleSize(110), colRGB(222,162,62), text);
        lmHoldPolicyRect(region, &rect);
        if (mask != 0)
        {
            rectangle box = rect;
            box.x1 = box.x0 + uiScaleSize(14); box.y1 = box.y0 + uiScaleSize(14);
            primRectOutline2(&box, 1, colRGB(135,156,159));
            if ((universe.curPlayerPtr->autoLaunch & mask) == 0)
            {
                box.x0 += uiScaleSize(3); box.y0 += uiScaleSize(3);
                box.x1 -= uiScaleSize(3); box.y1 -= uiScaleSize(3);
                primRectSolid2(&box, colRGB(222,162,62));
            }
            fontPrint(rect.x0 + uiScaleSize(20), rect.y0, FEC_ListItemStandard, "STAY DOCKED");
        }
        else fontPrint(rect.x0, rect.y0, FEC_ListItemStandard, "MANUAL LAUNCH");
    }
    fontMakeCurrent(oldfont);
}

static sdword lmHoldProcess(regionhandle region, smemsize ID, udword event, udword data)
{
    sdword count = lmHoldCount(), slot, direction = 0;
    rectangle rect;
    (void)ID; (void)data;
    lmHoldPage = lmHoldClampPage(count, lmHoldPage);
    if (event == RPE_WheelUp) direction = -1;
    else if (event == RPE_WheelDown) direction = 1;
    else if (event == RPE_PressLeft)
    {
        for (slot = 0; slot < LM_HOLD_PAGE_SIZE; ++slot)
        {
            lmHoldButtonRect(region, slot, &rect);
            if (lmHoldHit(&rect))
            {
                lmHoldSelect(lmHoldAt(lmHoldSlotIndex(count, lmHoldPage, slot)), FALSE);
                return 0;
            }
        }
        for (slot = 0; slot < 2; ++slot)
        {
            lmHoldNavRect(region, slot != 0, &rect);
            if (lmHoldHit(&rect)) direction = slot ? 1 : -1;
        }
        lmHoldPolicyRect(region, &rect);
        if (lmHoldHit(&rect) && lmHoldIndexOf(launchship) >= 0)
        {
            udword mask = lmHoldAutoLaunchMask(launchship);
            if (mask) clWrapAutoLaunch(universe.curPlayerPtr->autoLaunch ^ mask, universe.curPlayerIndex);
        }
    }
    if (direction) lmHoldPage = lmHoldClampPage(count, lmHoldPage + direction);
    regRecursiveSetDirty(lmBaseRegion);
    return 0;
}

static void lmHoldInstall(regionhandle parent)
{
    rectangle panel;
    lmHoldRegion = NULL;
    if (!mdManagerDockEnabled() || parent == NULL) return;
    mdGetPanelRect(&panel);
    lmHoldRegion = regChildAlloc(parent, 0, panel.x0 + uiScaleSize(12), panel.y0 + uiScaleSize(58),
        uiScaleSize(336), uiScaleSize(128), 0,
        RPE_PressLeft | RPE_WheelUp | RPE_WheelDown | RPE_DrawEveryFrame);
    regDrawFunctionSet(lmHoldRegion, lmHoldDraw);
    regFunctionSet(lmHoldRegion, (regionfunction)lmHoldProcess);
    regSiblingMoveToFront(lmHoldRegion);
}
