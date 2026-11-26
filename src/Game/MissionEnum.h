#ifndef HW_MISSION_ENUM_H
#define HW_MISSION_ENUM_H

/* Kept lightweight so render-only modules can identify mission-specific
   presentation without pulling the complete KAS/gameplay header graph. */
typedef enum MissionEnum
{
    MISSION_ENUM_NOT_INITIALISED = 0,
    MISSION_1_KHARAK_SYSTEM,
    MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM,
    MISSION_3_RETURN_TO_KHARAK,
    MISSION_4_GREAT_WASTELANDS_TRADERS,
    MISSION_5_GREAT_WASTELANDS_REVENGE,
    MISSION_6_DIAMOND_SHOALS,
    MISSION_7_THE_GARDENS_OF_KADESH,
    MISSION_8_THE_CATHEDRAL_OF_KADESH,
    MISSION_9_SEA_OF_LOST_SOULS,
    MISSION_10_SUPER_NOVA_STATION,
    MISSION_11_TENHAUSER_GATE,
    MISSION_12_GALACTIC_CORE,
    MISSION_13_THE_KAROS_GRAVEYARD,
    MISSION_14_BRIDGE_OF_SIGHS,
    MISSION_15_CHAPEL_PERILOUS,
    MISSION_16_HIIGARA,
    MISSION_5B_TURANIC_RAIDER_PLANETOID,
    MISSION_TUTORIAL,
    MISSION_CATA_1_HIIGARA,
    MISSION_CATA_2_OUTSKIRTS
} MissionEnum;

static __inline int hwIsCataclysmMission(MissionEnum mission)
{return mission==MISSION_CATA_1_HIIGARA || mission==MISSION_CATA_2_OUTSKIRTS;}

static __inline int hwCataclysmMissionNumber(MissionEnum mission)
{
    switch (mission) {
        case MISSION_CATA_1_HIIGARA: return 1;
        case MISSION_CATA_2_OUTSKIRTS: return 2;
        default: return 0;
    }
}

static __inline MissionEnum hwCataclysmAdjacentMission(MissionEnum mission, int direction)
{
    if (mission == MISSION_CATA_1_HIIGARA && direction > 0) return MISSION_CATA_2_OUTSKIRTS;
    if (mission == MISSION_CATA_2_OUTSKIRTS && direction < 0) return MISSION_CATA_1_HIIGARA;
    return MISSION_ENUM_NOT_INITIALISED;
}

#endif
