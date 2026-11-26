/* Keep both sets alive: mixer channels borrow bank memory. Switch only on
   the game thread, after joining the audio pass, and read owned files in place. */
typedef struct SEBankSet {
    BANK *gun,*ship,*effect,*ui;
    SFXLUT *gunEvents,*common,*shipEvents,*derelict,*explosion,*hit,*uiEvents;
} SEBankSet;
static SEBankSet seOriginalBanks,seCataBanks;
static bool32 seBanksCaptured,seCataBanksLoaded,seCataBanksActive;
static const char *seBankDirectory=SOUNDFXDIR;
#include "CataclysmSoundIdentity.inc"
#include "CataclysmUiSound.inc"
static void seCaptureBanks(SEBankSet *set)
{
    set->gun=GunBank;set->ship=ShipBank;set->effect=SpecialEffectBank;set->ui=UIBank;
    set->gunEvents=GunEventsLUT;set->common=ShipCmnEventsLUT;set->shipEvents=ShipEventsLUT;
    set->derelict=DerelictEventsLUT;set->explosion=SpecExpEventsLUT;set->hit=SpecHitEventsLUT;set->uiEvents=UIEventsLUT;
}
static void seUseBanks(const SEBankSet *set)
{
    GunBank=set->gun;ShipBank=set->ship;SpecialEffectBank=set->effect;UIBank=set->ui;
    GunEventsLUT=set->gunEvents;ShipCmnEventsLUT=set->common;ShipEventsLUT=set->shipEvents;
    DerelictEventsLUT=set->derelict;SpecExpEventsLUT=set->explosion;SpecHitEventsLUT=set->hit;UIEventsLUT=set->uiEvents;
}
static void seFreeBanks(SEBankSet *set)
{
    memFree(set->gun);memFree(set->ship);memFree(set->effect);memFree(set->ui);
    memFree(set->gunEvents);memFree(set->common);memFree(set->shipEvents);
    memFree(set->derelict);memFree(set->explosion);memFree(set->hit);memFree(set->uiEvents);
    memset(set,0,sizeof(*set));
}
bool32 soundEventUsesCataclysm(void)
{
    return singlePlayerGame && hwIsCataclysmMission(spGetCurrentMission());
}
void soundEventSelectGameBanks(void)
{
    bool32 wanted=soundEventUsesCataclysm();
    if(!soundeventinited || !enableSFX || wanted==seCataBanksActive)return;
    hwModernGraphicsWaitAudioWork();
    if(!seBanksCaptured){seCaptureBanks(&seOriginalBanks);seBanksCaptured=TRUE;}
    if(wanted && !seCataBanksLoaded){
        if(!hwCataDataReady())dbgFatalf(DBG_Loc,"Cataclysm sounds require the owned game data");
        seBankDirectory="Cataclysm\\SoundFX\\";
        SEloadbank();
        seCaptureBanks(&seCataBanks);seCataBanksLoaded=TRUE;
        seBankDirectory=SOUNDFXDIR;
    }
    seUseBanks(wanted?&seCataBanks:&seOriginalBanks);
    seCataBanksActive=wanted;
}
sdword SEshipSoundObject(const Ship *ship)
{
    return seCataBanksActive?cataSoundShipObject(ship):ship->shiptype;
}
sdword SEpatch(SFXLUT *lut,sdword object,sdword event)
{
    if(seCataBanksActive && lut==UIEventsLUT){
        sdword row=cataUiEventRow(event);if(row<0)return SOUND_ERR;
        event=UI_Flag|row;
    }
    sdword index=GetPatch(lut,object,event);
    return index<0?SOUND_ERR:lut->lookup[index];
}
