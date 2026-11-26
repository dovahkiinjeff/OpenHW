#ifndef OPENHW_CAMPAIGN_COLOR_POLICY_H
#define OPENHW_CAMPAIGN_COLOR_POLICY_H
/* Cataclysm's stock palette must not inherit a stale HW1 scheme when no
   identity was chosen. Explicit player identities take precedence in both
   campaigns; NPC/script palette slots continue to use the mission palette. */
static int hwKeepMissionPlayerColors(int cataclysm,int colorsPicked,int index,
    int players,int match,int human)
{
    return index<players && (match||human) && (!cataclysm||colorsPicked);
}
#endif
