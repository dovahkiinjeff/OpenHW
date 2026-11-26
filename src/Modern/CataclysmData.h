#ifndef OPENHW_CATACLYSM_DATA_H
#define OPENHW_CATACLYSM_DATA_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Owned installation stays read-only. Archive views live until close, which
   is only legal after all borrowed mesh/texture views have been released. */
int hwCataDataOpen(const char *directory,char *error,size_t errorSize);
int hwCataDataOpenConfigured(char *error,size_t errorSize);
int hwCataDataReady(void);
void hwCataDataClose(void);
const void *hwCataDataView(const char *name,int *size);
/* Engine-facing text view; raw owned definitions remain available above. */
const void *hwCataCompatibleShipView(const char *name,int *size);
float hwCataWeaponDamage(unsigned weaponClass,unsigned armorClass);
const char *hwCataOriginalText(unsigned originalDataAddress);
const char *hwCataLocalizedText(unsigned originalTableAddress,unsigned language);
/* Whitelisted external audio, resolved inside the owned directory. */
int hwCataAudioPath(const char *name,char *path,size_t capacity);
/* Resolve the numbered transition in the installed movie list. The movie
   path is absolute and the timed script path uses the Cataclysm archive. */
int hwCataAnimaticPaths(int sequence, char *movie, size_t movieCapacity,
                       char *script, size_t scriptCapacity);
typedef struct CataSpeechPhrase {
    unsigned checksum,bitrate,durationMilliseconds,count;
    unsigned offsets[32];
} CataSpeechPhrase;
int hwCataSpeechPhrase(unsigned event,int variable,CataSpeechPhrase *phrase,char *error,size_t capacity);
typedef unsigned (*CataSpeechRandom)(void);
int hwCataSpeechPhraseRandom(unsigned event,int variable,CataSpeechRandom random,CataSpeechPhrase *phrase,char *error,size_t capacity);
int hwCataResearchDuration(const char *technology);
int hwCataTechnologyIndex(const char *technology);
unsigned hwCataTechnologyAbilityMask(unsigned technology);
unsigned hwCataShipAllowedUpgradeMask(unsigned originalShip);
unsigned hwCataDataCount(void);
const char *hwCataDataName(unsigned index);
/* Visual requests only: Kushan models use OpenHW. Never use this for SHP,
   weapon, event, research or mission requests. */
int hwCataOpenHWVisualPath(const char *directory,const char *member,char *path,size_t capacity);
/* Extract the ship and LOD from a stock Taiidan hull mesh request. */
int hwCataTaiidanMeshIdentity(const char *path,char *ship,size_t capacity,unsigned *lod);
int hwCataRaiderMeshIdentity(const char *path,char *ship,size_t capacity,unsigned *lod);
/* Resolve animation from the actual owned SHP name, preserving race prefixes. */
int hwCataAnimationPath(const char *directory,const char *definition,char *path,size_t capacity);
/* Resolve a texture name for the registry, which appends the .LiF suffix. */
int hwCataNavLightTexturePath(const char *name,char *path,size_t capacity);
typedef struct CataStartingShip {char name[64];unsigned count;} CataStartingShip;
typedef struct CataStartingFleet {
    int resources;
    unsigned shipCount,technologyCount;
    CataStartingShip ships[32];
    char technologies[32][64];
} CataStartingFleet;
int hwCataMission1StartingFleet(CataStartingFleet *fleet,char *error,size_t capacity);
typedef struct CataShipEconomy {
    int supportRequired,supportProvided,typeLimit,totalLimit;
    float sensorRadius;
    int upgradeByDocking;
    float upgradeTimeModifier;
} CataShipEconomy;
int hwCataShipEconomy(const char *raceDirectory,const char *shipStem,CataShipEconomy *economy,char *error,size_t capacity);
typedef struct CataShipNavigation {int priority;float avoidBy;} CataShipNavigation;
int hwCataShipNavigation(const char *shipStem,CataShipNavigation *navigation,char *error,size_t capacity);
/* In-memory adapters only; owned effect scripts and dependencies stay in place. */
const char *hwCataEnergyFxScript(void);
const void *hwCataCompatibleEffectView(const char *name,int *size);
#ifdef __cplusplus
}
#endif
#endif
