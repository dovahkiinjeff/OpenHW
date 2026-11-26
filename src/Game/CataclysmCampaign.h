#ifndef OPENHW_CATACLYSM_CAMPAIGN_H
#define OPENHW_CATACLYSM_CAMPAIGN_H
#include "KAS.h"
#include "ResearchAPI.h"
#include "CameraCommand.h"
#include "MissionEnum.h"
/* Mission 1 uses this otherwise unused HW1 slot in a separate research table;
   existing HW1 technology IDs and save layouts remain unchanged. */
#define CATA_LINK_TECH SalvageTech
#define CATA_REPAIR_TECH RepairTech
#define CATA_SALVAGE_TECH ConstructionTech
#define CATA_FIGHTER_DRIVE_TECH NewAlloys
#define CATA_MIMIC_TECH CloakGeneratorTech
#define CATA_AFTERBURNER_TECH AdvanceTacticalSupport
sdword cataCampaignTechSlot(const char *name);
const char *cataCampaignTechName(TechnologyType tech);
const char *cataCampaignTechTitle(TechnologyType tech);
const char *cataCampaignTechDescription(TechnologyType tech);
bool32 cataCampaignHasResearch(const char *name);
#define CATA_UPGRADE_REPAIR  0x00040000u
#define CATA_UPGRADE_SALVAGE 0x00080000u
#define CATA_UPGRADE_LINK    0x00000008u
void cataCampaignUpgradeActivate(const char *name);
bool32 cataCampaignShipHasUpgrade(struct Ship *ship,udword ability);
udword cataCampaignPendingUpgrade(const struct Ship *ship);
void cataCampaignOrderUpgrades(SelectCommand *selection);
void cataCampaignUpdateShipUpgrade(struct Ship *ship);
void cataCampaignDockedUpgrade(struct Ship *ship, const struct Ship *host);
void cataCampaignInitializeShipUpgrades(struct Ship *ship);
void cataCampaignCopyShipUpgrades(struct Ship *destination,struct Ship *source);
void cataCampaignCameraRequestSnap(real32 distance);
bool32 cataCampaignCameraConsumeSnap(CameraCommand *command);
void cataCampaignInitialize(void);
bool32 cataCampaignKeepHandoffVariable(const char *label);
void cataCampaignInitializeArrival(void);
bool32 cataCampaignCanLoadMission(MissionEnum mission);
void cataCampaignAfterLoad(void);
void cataCampaignResearchStatics(struct Player *player);
bool32 cataCampaignLinkAvailable(void);
bool32 cataCampaignShipCanLink(struct Ship *ship);
bool32 cataCampaignLockoutPreserved(void);
void cataCampaignResearchAllow(const char *name);
void cataCampaignResearchAcquire(const char *name);
void cataCampaignApplyUpgrades(GrowSelection *ships);
sdword cataCampaignShipsInSensorRange(GrowSelection *ships);
void cataCampaignSpeech(sdword event,sdword variable);
sdword cataCampaignSpeechEnded(void);
void cataCampaignRequestCheckpoint(sdword missionNumber,const char *title);
void cataCampaignWriteCheckpoint(void);
void cataCampaignClearCheckpoint(void);
void cataCampaignRestoreFleet(void);
void cataCampaignShipArrived(struct Ship *ship);
void cataCampaignSensorCenter(hvector *position);
sdword cataCampaignDifficulty(void);
void cataCampaignPreserveLockout(sdword preserve);
#endif
