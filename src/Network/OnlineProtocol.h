#ifndef OPENHW_ONLINE_PROTOCOL_H
#define OPENHW_ONLINE_PROTOCOL_H
#include <stdint.h>
/* Versioned, little-endian envelope. Never put process pointers in this file.
   The native command stream inside STEP/SYNC is accepted only for an exact
   content/build fingerprint and is separately checked before execution. */
#define OH_PROTOCOL 1
#define OH_MAX_MESSAGE 65536
#define OH_MAX_SYNC 48000
#define OH_MAX_PLAYERS 8
#define OH_NAME_BYTES 48
#define OH_RULE_COUNT 14
enum OHMessage {
    OH_HELLO=1, OH_CREATE=2, OH_JOIN=3, OH_LEAVE=4, OH_CONFIGURE=5,
    OH_ADD_AI=6, OH_REMOVE_SLOT=7, OH_READY=8, OH_START=9, OH_LOADED=10,
    OH_COMMAND=11, OH_PAUSE=12, OH_WELCOME=100, OH_LOBBY=101, OH_ERROR=102,
    OH_LOAD=103, OH_SYNC=104, OH_STOP=105, OH_PAUSED=106,
    OH_WORKER_INIT=200, OH_WORKER_READY=201, OH_WORKER_STEP=202,
    OH_WORKER_SYNC=203, OH_WORKER_ERROR=204, OH_WORKER_INSPECT=205
};
enum OHRule {
    OH_UNLIMITED_FUEL, OH_PAUSE_ORDERS, OH_SHIP_RECOIL, OH_RESOURCE_MULTIPLIER,
    OH_RANK_SYSTEM, OH_MISSION_START_LAUNCH, OH_RESOURCE_POPULATION,
    OH_UNLIMITED_RUS, OH_FOG_OF_WAR, OH_STARTING_RESOURCE_FLEET,
    OH_SUPER_SALVAGERS, OH_CAPTURE_BUILD_ALL, OH_DISABLE_UNIT_CAPS,
    OH_RULE_RESERVED
};
#pragma pack(push,1)
typedef struct OHHeader { char magic[4]; uint16_t version,type; uint32_t length; } OHHeader;
typedef struct OHHello { char name[48]; uint8_t fingerprint[32]; } OHHello;
/* kind: 0 empty, 1 human, 2 AI. Difficulty: 0 beginner, 1 intermediate,
   2 advanced. Race: 0 Kushan, 1 Taiidan. */
typedef struct OHSlot {
    uint8_t kind,race,difficulty,ready;
    uint8_t team,reserved[3]; /* 0 free-for-all, 1..8 shared alliance */
    uint32_t base,stripe,emission;
    char name[48];
} OHSlot;
typedef struct OHConfig {
    char map[64];
    uint16_t rules[OH_RULE_COUNT];
    uint8_t startingFleet,startingResources,bounty,reserved;
    uint32_t gameFlags,injectionInterval,injectionAmount,lumpTime,lumpAmount;
    OHSlot slots[OH_MAX_PLAYERS];
} OHConfig;
typedef struct OHLobby {
    char code[8];
    uint32_t revision;
    uint8_t ownSlot,host,state,reserved;
    OHConfig config;
} OHLobby;
typedef struct OHLoad {
    uint32_t seed;
    uint8_t ownSlot,humans,players,reserved;
    OHConfig config;
} OHLoad;
#pragma pack(pop)
#endif
