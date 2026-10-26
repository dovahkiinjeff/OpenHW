/* Compile once for each supported ship-table length. Offsets and padding are
   supplied by the compiler, never guessed from save bytes. */
#include "ShipDefs.h"
#ifdef SAVE_LAYOUT_SHIP_COUNT
#undef TOTAL_NUM_SHIPS
#define TOTAL_NUM_SHIPS SAVE_LAYOUT_SHIP_COUNT
#endif
#include "Universe.h"
#include "AIPlayer.h"
#include "SaveGameLayout.h"
#include <stddef.h>

#define FIELD(T, M) {offsetof(T, M), sizeof(((T *)0)->M), NULL, 0}
#define CHILD(T, M, R, N) {offsetof(T, M), sizeof(((T *)0)->M), &R, N}

static const SaveLayoutField fieldsPlayerStats[] = {
    FIELD(PlayerStats, timeOfDeath),
    FIELD(PlayerStats, typeOfPlayerDeath),
    FIELD(PlayerStats, totalResourceUnits),
    FIELD(PlayerStats, totalResourceUnitsCollected),
    FIELD(PlayerStats, totalResourceUnitsSpent),
    FIELD(PlayerStats, totalResourceUnitsGiven),
    FIELD(PlayerStats, totalResourceUnitsRecieved),
    FIELD(PlayerStats, totalRegeneratedResourceUnits),
    FIELD(PlayerStats, totalInjectedResources),
    FIELD(PlayerStats, totalResourceUnitsViaBounties),
    FIELD(PlayerStats, totalRUsInCurrentShips),
    FIELD(PlayerStats, totalShips),
    FIELD(PlayerStats, totalForEachShip),
    FIELD(PlayerStats, totalForEachClass),
    FIELD(PlayerStats, totalKills),
    FIELD(PlayerStats, totalRUsKilled),
    FIELD(PlayerStats, totalKillsForEachShip),
    FIELD(PlayerStats, totalKillsForEachClass),
    FIELD(PlayerStats, totalLosses),
    FIELD(PlayerStats, totalRUsLost),
    FIELD(PlayerStats, totalLossesForEachShip),
    FIELD(PlayerStats, totalLossesForEachClass),
    FIELD(PlayerStats, pointsOfDamageDoneToWho),
};
static const SaveLayoutRecord recordPlayerStats = {
    sizeof(PlayerStats), sizeof(fieldsPlayerStats) / sizeof(fieldsPlayerStats[0]), fieldsPlayerStats
};

static const SaveLayoutField fieldsGameStats[] = {
    FIELD(GameStats, totalResourcesInGame),
    FIELD(GameStats, totalShipsInGame),
    FIELD(GameStats, totalKills),
    FIELD(GameStats, totalRUsInAllShips),
    FIELD(GameStats, totalRUsKilled),
    FIELD(GameStats, totalRUsLost),
    FIELD(GameStats, totalResourceUnitsCollected),
    FIELD(GameStats, updatedRUValuesTime),
    FIELD(GameStats, universeRUWorth),
    FIELD(GameStats, startingResources),
    CHILD(GameStats, playerStats, recordPlayerStats, MAX_MULTIPLAYER_PLAYERS + 1),
};
static const SaveLayoutRecord recordGameStats = {
    sizeof(GameStats), sizeof(fieldsGameStats) / sizeof(fieldsGameStats[0]), fieldsGameStats
};

static const SaveLayoutField fieldsPlayer[] = {
    FIELD(Player, PlayerMothership),
    FIELD(Player, resourceUnits),
    FIELD(Player, race),
    FIELD(Player, playerState),
    FIELD(Player, aiPlayer),
    FIELD(Player, autoLaunch),
    FIELD(Player, playerIndex),
    FIELD(Player, sensorLevel),
    FIELD(Player, bounty),
    FIELD(Player, timeMoShipAttacked),
    FIELD(Player, totalships),
    FIELD(Player, shiptotals),
    FIELD(Player, classtotals),
    FIELD(Player, researchinfo),
    FIELD(Player, Allies),
    FIELD(Player, AllianceProposals),
    FIELD(Player, AllianceProposalsTimeout),
    FIELD(Player, initialShipCost),
};
static const SaveLayoutRecord recordPlayer = {
    sizeof(Player), sizeof(fieldsPlayer) / sizeof(fieldsPlayer[0]), fieldsPlayer
};

static const SaveLayoutField fieldsUniverse[] = {
    FIELD(Universe, mainCommandLayer),
    FIELD(Universe, mainCameraCommand),
    FIELD(Universe, RenderList),
    FIELD(Universe, MinorRenderList),
    FIELD(Universe, dontUpdateRenderList),
    FIELD(Universe, phystimeelapsed),
    FIELD(Universe, totaltimeelapsed),
    FIELD(Universe, radius),
    FIELD(Universe, SpaceObjList),
    FIELD(Universe, MinorSpaceObjList),
    FIELD(Universe, effectList),
    FIELD(Universe, ShipList),
    FIELD(Universe, BulletList),
    FIELD(Universe, ResourceList),
    FIELD(Universe, DerelictList),
    FIELD(Universe, ImpactableList),
    FIELD(Universe, MissileList),
    FIELD(Universe, MineFormationList),
    FIELD(Universe, DeleteResourceList),
    FIELD(Universe, DeleteDerelictList),
    FIELD(Universe, DeleteShipList),
    FIELD(Universe, DeleteMissileList),
    FIELD(Universe, collBlobList),
    FIELD(Universe, RetreatList),
    FIELD(Universe, AttackMemory),
    FIELD(Universe, ResourceVolumeList),
    FIELD(Universe, HousekeepShipList),
    FIELD(Universe, star3dinfo),
    FIELD(Universe, univUpdateCounter),
    FIELD(Universe, backgroundColor),
    FIELD(Universe, resourceNumber),
    FIELD(Universe, derelictNumber),
    FIELD(Universe, shipNumber),
    FIELD(Universe, missileNumber),
    FIELD(Universe, wintime),
    FIELD(Universe, quittime),
    FIELD(Universe, lasttimeadded),
    FIELD(Universe, shipMaxUnits),
    FIELD(Universe, aiplayerEnemy),
    FIELD(Universe, aiplayerProcessing),
    FIELD(Universe, CapitalShipCaptured),
    FIELD(Universe, PlayerWhoWon),
    FIELD(Universe, bounties),
    FIELD(Universe, bountySize),
    CHILD(Universe, gameStats, recordGameStats, 1),
    FIELD(Universe, DerelictTech),
    FIELD(Universe, crateTimer),
    FIELD(Universe, numCratesInWorld),
    FIELD(Universe, collUpdateAllBlobs),
    FIELD(Universe, numPlayers),
    FIELD(Universe, curPlayerIndex),
    FIELD(Universe, curPlayerPtr),
    CHILD(Universe, players, recordPlayer, MAX_MULTIPLAYER_PLAYERS + 1),
    FIELD(Universe, world),
};
static const SaveLayoutRecord recordUniverse = {
    sizeof(Universe), sizeof(fieldsUniverse) / sizeof(fieldsUniverse[0]), fieldsUniverse
};

static const SaveLayoutField fieldsAIPlayer[] = {
    FIELD(AIPlayer, player),
    FIELD(AIPlayer, aiplayerDifficultyLevel),
    FIELD(AIPlayer, ResourceFeatures),
    FIELD(AIPlayer, AttackFeatures),
    FIELD(AIPlayer, DefenseFeatures),
    FIELD(AIPlayer, aiplayerUpdateRate),
    FIELD(AIPlayer, aiplayerFrameCount),
    FIELD(AIPlayer, enemyPlayerCount),
    FIELD(AIPlayer, primaryEnemyPlayer),
    FIELD(AIPlayer, newships),
    FIELD(AIPlayer, enemyShipsIAmAwareOf),
    FIELD(AIPlayer, primaryEnemyShipsIAmAwareOf),
    FIELD(AIPlayer, NumRUDockPoints),
    FIELD(AIPlayer, NumRCollectorsBeingBuilt),
    FIELD(AIPlayer, NumRControllersBeingBuilt),
    FIELD(AIPlayer, NumASFBeingBuilt),
    FIELD(AIPlayer, NumResearchShipsBeingBuilt),
    FIELD(AIPlayer, ScriptCreator),
    FIELD(AIPlayer, AICreator),
    FIELD(AIPlayer, NumRUsSpentOnResourceman),
    FIELD(AIPlayer, NumRUsSpentOnAttman),
    FIELD(AIPlayer, NumRUsSpentOnDefman),
    FIELD(AIPlayer, NumRUsSpentOnScriptman),
    FIELD(AIPlayer, ResourceManRequestShips),
    FIELD(AIPlayer, AttackManRequestShipsQ),
    FIELD(AIPlayer, DefenseManRequestShipsQ),
    FIELD(AIPlayer, ScriptManRequestShipsQ),
    FIELD(AIPlayer, AttackManTeamsWaitingForShipsQ),
    FIELD(AIPlayer, DefenseManTeamsWaitingForShipsQ),
    FIELD(AIPlayer, ScriptManTeamsWaitingForShipsQ),
    FIELD(AIPlayer, AttackManShipsBeingBuilt),
    FIELD(AIPlayer, DefenseManShipsBeingBuilt),
    FIELD(AIPlayer, ScriptManShipsBeingBuilt),
    FIELD(AIPlayer, TechnologyDeficit),
    FIELD(AIPlayer, ResourcersToBuild),
    FIELD(AIPlayer, ResearchersToBuild),
    FIELD(AIPlayer, ResearchDelay),
    FIELD(AIPlayer, firstTurn),
    FIELD(AIPlayer, recalculateAllies),
    FIELD(AIPlayer, aifHyperSavings),
    FIELD(AIPlayer, aifHyperSkimming),
    FIELD(AIPlayer, aifLastRUCount),
    FIELD(AIPlayer, airResourceReserves),
    FIELD(AIPlayer, airResourceCollectors),
    FIELD(AIPlayer, airEasilyAccesibleRUsInWorld),
    FIELD(AIPlayer, airNumRCollectors),
    FIELD(AIPlayer, airNumRControllers),
    FIELD(AIPlayer, airNumASF),
    FIELD(AIPlayer, airNumResearchShips),
    FIELD(AIPlayer, numSupportTeams),
    FIELD(AIPlayer, supportTeam),
    FIELD(AIPlayer, harassTeam),
    FIELD(AIPlayer, reconTeam),
    FIELD(AIPlayer, numReconTeams),
    FIELD(AIPlayer, attackTeam),
    FIELD(AIPlayer, numAttackTeams),
    FIELD(AIPlayer, haveAttackedMothership),
    FIELD(AIPlayer, attackVarLabel),
    FIELD(AIPlayer, aiaAttackProbability),
    FIELD(AIPlayer, numLeaders),
    FIELD(AIPlayer, aiaArmada),
    FIELD(AIPlayer, Targets),
    FIELD(AIPlayer, aiaTimeout),
    FIELD(AIPlayer, shipsattackingmothership),
    FIELD(AIPlayer, MothershipUnderAttack),
    FIELD(AIPlayer, AlertStatus),
    FIELD(AIPlayer, NumProxSensorsRequested),
    FIELD(AIPlayer, aidProximitySensors),
    FIELD(AIPlayer, aidDefenseTargets),
    FIELD(AIPlayer, aidInvadingShips),
    FIELD(AIPlayer, aidDistressShips),
    FIELD(AIPlayer, sphereofinfluence),
    FIELD(AIPlayer, mothershipdefteam),
    FIELD(AIPlayer, numGuardTeams),
    FIELD(AIPlayer, guardTeams),
    FIELD(AIPlayer, teams),
    FIELD(AIPlayer, teamsAllocated),
    FIELD(AIPlayer, teamsUsed),
};
static const SaveLayoutRecord recordAIPlayer = {
    sizeof(AIPlayer), sizeof(fieldsAIPlayer) / sizeof(fieldsAIPlayer[0]), fieldsAIPlayer
};

static const SaveGameLayout layout = { TOTAL_NUM_SHIPS, &recordUniverse, &recordAIPlayer };
const SaveGameLayout *SAVE_LAYOUT_FUNCTION(void) { return &layout; }
#undef FIELD
#undef CHILD
