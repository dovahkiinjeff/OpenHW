/* Geometry only: keep FEMan's callbacks, list models, tab order and values. */
typedef struct MuiNetworkPage
{
    const char *name, *title, *description;
} MuiNetworkPage;

static const MuiNetworkPage muiNetworkPages[] = {
    {"Skirmish_Basic", "SKIRMISH / BATTLE SETUP", "Choose your opponents, victory rules and battlefield."},
    {"Skirmish_Advanced", "SKIRMISH / FLEET RULES", "Configure research, fleet limits and combat rules."},
    {"Skirmish_Resource", "SKIRMISH / RESOURCES", "Set starting resources, harvesting and reinforcements."},
    {"Skirmish_Colour", "SKIRMISH / FLEET COLOURS", "Choose your fleet and its hull and stripe colours."},
    {"Skirmish_Load", "SAVED SKIRMISHES", "Continue a saved battle against computer fleets."},
    {"Load_a_scenario", "SELECT BATTLEFIELD", "Choose a map and review its preview before confirming."},
    {"Player_Options", "FLEET COLOURS", "Customise your fleet's hull and stripe colours."},
    {"Recorded_Game", "RECORDED BATTLES", "Choose a recorded battle to replay."},
    {"Internet_Login", "ONLINE SERVICES", "Configure your player profile and connection."},
    {"LAN_Login", "LOCAL NETWORK", "Choose your player name and fleet colours."},
    {"LLAN_Login", "LOCAL NETWORK", "Choose your player name and fleet colours."},
    {"Channel_Chat", "PLAYER LOUNGE", "Chat with players and browse available battles."},
    {"LChannel_Chat", "LOCAL NETWORK LOUNGE", "Chat with players and browse local battles."},
    {"Available_Channels", "PLAYER CHANNELS", "Choose a channel or create one for your group."},
    {"Create_Channel", "CREATE CHANNEL", "Set the name and access rules for your channel."},
    {"Available_Games", "AVAILABLE BATTLES", "Choose a battle to join or create your own."},
    {"LAvailable_Games", "LOCAL BATTLES", "Choose a local battle to join or create your own."},
    {"Player_Wait", "BATTLE LOBBY", "Review the fleet roster and wait for the host."},
    {"LPlayer_Wait", "BATTLE LOBBY", "Review the fleet roster and wait for the host."},
    {"Captain_Wait", "HOST BATTLE", "Review the fleet roster and launch when ready."},
    {"LCaptain_Wait", "HOST BATTLE", "Review the fleet roster and launch when ready."},
    {"Basic_Options", "BATTLE SETUP", "Configure the battlefield, opponents and victory rules."},
    {"Advanced_Options", "FLEET RULES", "Configure research, fleet limits and combat rules."},
    {"Resource_Options", "RESOURCE RULES", "Configure harvesting and resource availability."},
    {"Game_Password", "JOIN PROTECTED BATTLE", "Enter the host's battle password."},
    {"LGame_Password", "JOIN PROTECTED BATTLE", "Enter the host's battle password."},
    {"Room_Password", "JOIN PROTECTED CHANNEL", "Enter the channel password."},
    {"Password_Change", "CHANGE PASSWORD", "Update your online account credentials."},
    {"New_Account", "PLAYER ACCOUNT", "Configure your online player account."},
    {"Connecting", "CONNECTING", "Waiting for the selected network service."},
    {"ChooseServer", "SELECT SERVER", "Choose an available game server."},
    {"Download_Patch", "SERVICE UPDATE", "Review the update offered by the network service."},
    {"Downloading_Patch", "SERVICE UPDATE", "Waiting for the network service update."}
};

static const MuiNetworkPage *muiNetworkPage(const char *name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(muiNetworkPages) / sizeof(muiNetworkPages[0]); ++i)
        if (strcmp(name, muiNetworkPages[i].name) == 0) return &muiNetworkPages[i];
    /* Host-edit and read-only variants use the same controls and shell. */
    if (strncmp(name, "Basic_Options_", 14) == 0) return muiNetworkPage("Basic_Options");
    if (strncmp(name, "Advanced_Options_", 17) == 0) return muiNetworkPage("Advanced_Options");
    if (strncmp(name, "Resource_Options_", 17) == 0) return muiNetworkPage("Resource_Options");
    return NULL;
}

static int muiNetworkTab(const char *name)
{
    if (name == NULL) return -1;
    if (strcmp(name, "Skirmish_Basic") == 0) return 0;
    if (strcmp(name, "Skirmish_Advanced") == 0) return 1;
    if (strcmp(name, "Skirmish_Resource") == 0) return 2;
    if (strcmp(name, "Skirmish_Colour") == 0) return 3;
    return -1;
}

static int muiNetworkTabAtom(const featom *atom)
{
    if (atom->type != FA_Button || atom->loadedX < 480 ||
        atom->loadedY < 30 || atom->loadedY > 190) return -1;
    return (atom->loadedY - 30) / 49;
}

static void muiNetworkSetRect(featom *atom, int x, int y, int w, int h)
{
    atom->x = (sword)x; atom->y = (sword)y;
    atom->width = (sword)(w > 0 ? w : 1);
    atom->height = (sword)(h > 0 ? h : 1);
}

static bool32 muiNetworkDecoration(const featom *atom)
{
    return atom->type == FA_DecorativeRegion ||
           atom->type == FA_OpaqueDecorativeRegion ||
           atom->type == FA_CutoutRegion ||
           (atom->type == 0 && atom->name == NULL && atom->pData == NULL);
}

static void muiNetworkPrepare(fescreen *screen, rectangle panel)
{
    int i, tab = muiNetworkTab(screen->name);
    int x0 = 32767, y0 = 32767, x1 = -32767, y1 = -32767;
    int pad = uiScaleSize(20), gap = uiScaleSize(8);
    rectangle body = {panel.x0 + pad, panel.y0 + pad,
                      panel.x1 - pad, panel.y1 - pad};
    real32 sx, sy, scale;
    for (i = 1; i < screen->nAtoms; ++i)
    {
        featom *a = &screen->atoms[i];
        if (muiNetworkDecoration(a) ||
            (a->type == FA_StaticText && a->loadedY < 32))
        {
            /* Hidden cutouts otherwise still punch holes in the modern panel. */
            if (a->type == FA_CutoutRegion) a->type = FA_DecorativeRegion;
            bitSet(a->flags, FAF_Hidden);
            continue;
        }
        if (a->loadedWidth <= 0 || a->loadedHeight <= 0) continue;
        if (a->loadedX < x0) x0 = a->loadedX;
        if (a->loadedY < y0) y0 = a->loadedY;
        if (a->loadedX + a->loadedWidth > x1) x1 = a->loadedX + a->loadedWidth;
        if (a->loadedY + a->loadedHeight > y1) y1 = a->loadedY + a->loadedHeight;
    }
    if (tab >= 0)
    {
        /* Replace the old right-hand button stack with four full-width tabs.
           Body controls keep their relationships, including colour previews. */
        x0 = 33; x1 = 445; y0 = 43; y1 = 406;
        body.y0 += uiScaleSize(58);
        body.y1 -= uiScaleSize(62);
    }
    if (x1 <= x0 || y1 <= y0) return;
    sx = (real32)(body.x1 - body.x0) / (x1 - x0);
    sy = (real32)(body.y1 - body.y0) / (y1 - y0);
    scale = sx < sy ? sx : sy;
    if (tab < 0 || tab == 3) sx = sy = scale;
    for (i = 1; i < screen->nAtoms; ++i)
    {
        featom *a = &screen->atoms[i];
        int slot = muiNetworkTabAtom(a);
        int cx = a->loadedX + a->loadedWidth / 2;
        int cy = a->loadedY + a->loadedHeight / 2;
        if (muiNetworkDecoration(a)) continue;
        if (tab >= 0 && a->type == FA_StaticText &&
            ((cx >= 480 && cy < 225) || cy >= 430 || a->loadedY < 32))
        {
            bitSet(a->flags, FAF_Hidden);
            continue;
        }
        if (tab >= 0 && slot >= 0)
        {
            int width = (panel.x1 - panel.x0 - pad * 2 - gap * 3) / 4;
            muiNetworkSetRect(a, panel.x0 + pad + slot * (width + gap),
                panel.y0 + pad, width, uiScaleSize(40));
        }
        else if (tab >= 0 && a->type == FA_Button && a->loadedY >= 430)
        {
            int width = uiScaleSize(160);
            muiNetworkSetRect(a, a->loadedX < 200 ? body.x0 : body.x1 - width,
                panel.y1 - pad - uiScaleSize(40), width, uiScaleSize(40));
        }
        else
        {
            int left = body.x0, top = body.y0;
            if (tab < 0 || tab == 3)
            {
                left += (int)((body.x1 - body.x0 - (x1 - x0) * scale) * .5f);
                top += (int)((body.y1 - body.y0 - (y1 - y0) * scale) * .5f);
            }
            muiNetworkSetRect(a, left + (int)((a->loadedX - x0) * sx + .5f),
                top + (int)((a->loadedY - y0) * sy + .5f),
                (int)(a->loadedWidth * sx + .5f),
                (int)(a->loadedHeight * sy + .5f));
        }
    }
    /* Legacy counters were tiny user regions painted with a black background
       and triangles whose height followed stretched width. Use square hit
       targets and a readable value cell, all sharing one baseline. */
    for (i = 1; i < screen->nAtoms; ++i)
    {
        featom *a = &screen->atoms[i];
        int j;
        if (a->name != NULL && strcmp(a->name, "MG_NumberOfComputers") == 0)
        {
            int center = a->x + a->width / 2;
            int height = (int)(20 * sy + .5f);
            int width = uiScaleSize(36), spacing = uiScaleSize(5);
            int top = a->y;
            if (height > uiScaleSize(36)) height = uiScaleSize(36);
            for (j = 1; j < screen->nAtoms; ++j)
            {
                featom *control = &screen->atoms[j];
                if (control->name == NULL) continue;
                if (strcmp(control->name, "MG_RemoveCPUOpponent") == 0)
                    muiNetworkSetRect(control, center - width * 3 / 2 - spacing,
                        top, width, height);
                else if (strcmp(control->name, "MG_AddCPUOpponent") == 0)
                    muiNetworkSetRect(control, center + width / 2 + spacing,
                        top, width, height);
            }
            muiNetworkSetRect(a, center - width / 2, top, width, height);
        }
        if (a->name != NULL && strcmp(a->name, "MG_GameType") == 0)
        {
            a->width = (sword)(123 * sx + .5f);
            for (j = 1; j < screen->nAtoms; ++j)
            {
                featom *label = &screen->atoms[j];
                int cx = label->loadedX + label->loadedWidth / 2;
                int cy = label->loadedY + label->loadedHeight / 2;
                if (label->type != FA_StaticText ||
                    cx < a->loadedX || cx >= a->loadedX + a->loadedWidth ||
                    cy < a->loadedY || cy >= a->loadedY + a->loadedHeight) continue;
                label->x = a->x + uiScaleSize(22);
                label->width = a->width - uiScaleSize(28);
                label->y = a->y + (a->height - label->height) / 2;
                label->flags = (label->flags & ~FAM_Justification) | FAM_JustCentre;
            }
        }
    }
    muiNetworkSetRect(&screen->atoms[0], 0, 0, MAIN_WindowWidth, MAIN_WindowHeight);
    bitSet(screen->atoms[0].flags, FAF_ContentsVisible);
}
