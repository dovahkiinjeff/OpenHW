#ifndef HW_CAMPAIGN_MISSION_PATHS_H
#define HW_CAMPAIGN_MISSION_PATHS_H
#include "MissionEnum.h"
#include <stdio.h>
#include <stddef.h>

#define SP_MISSION_PATH_CAPACITY 256

static __inline int hwCampaignMissionPaths(MissionEnum mission, int raiderRetreat,
    char *directory, size_t directoryCapacity, char *file, size_t fileCapacity)
{
    int directoryLength, fileLength;
#ifdef _WIN32
    const char *separator = "\\";
#else
    const char *separator = "/";
#endif
    if (!directory || !file || !directoryCapacity || !fileCapacity) return 0;
    if (hwIsCataclysmMission(mission))
    {
        int number = hwCataclysmMissionNumber(mission);
        directoryLength = snprintf(directory, directoryCapacity,
            "Cataclysm%sSinglePlayer%sMission%02d%s", separator, separator,number,separator);
        fileLength = snprintf(file, fileCapacity, "Mission%02d.mission",number);
    }
    else if (raiderRetreat && mission == MISSION_5B_TURANIC_RAIDER_PLANETOID)
    {
        directoryLength = snprintf(directory, directoryCapacity,
            "SinglePlayer%sMission05_OEM%s", separator, separator);
        fileLength = snprintf(file, fileCapacity, "Mission05_OEM.mission");
    }
    else
    {
        directoryLength = snprintf(directory, directoryCapacity,
            "SinglePlayer%sMission%02d%s", separator, (int)mission, separator);
        fileLength = snprintf(file, fileCapacity, "Mission%02d.mission", (int)mission);
    }
    if (directoryLength < 0 || (size_t)directoryLength >= directoryCapacity ||
        fileLength < 0 || (size_t)fileLength >= fileCapacity)
    {
        directory[0] = file[0] = 0;
        return 0;
    }
    return 1;
}
#endif
