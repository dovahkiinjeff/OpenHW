#ifndef OPENHW_REMASTERED_MUSIC_PATHS_H
#define OPENHW_REMASTERED_MUSIC_PATHS_H
/* Indexed by HW1 SoundMusic.h IDs, never RM mission/gameplay scripts.
   Unmapped logo/credits tracks stay explicit rather than guessing a sound. */
static const char *rmMusicPath(int track)
{
    static const char *const paths[] = {
        "ambient/a02_mission2and4.wav", "ambient/a04_mission5.wav",
        "ambient/a05_mission6and12.wav", "ambient/a06_mission7and8.wav",
        "ambient/a07_mission9.wav", "ambient/a08_mission10.wav",
        "ambient/a09_mission11and15.wav", "ambient/a11_mission13.wav",
        "ambient/a12_mission14.wav", "ambient/tutorial.wav",
        "battle/a01_mission1and16.wav", "battle/a03_mission3.wav",
        "battle/b01_turanicraiderslong.wav", "battle/b02_turanicraidersshort.wav",
        "battle/b03_swarmers.wav", "battle/b04_evilempire.wav", "battle/nislet01.wav",
        "nis/n01_r1opening.wav", "nis/n01_r2opening.wav", "nis/n02_p1intro.wav",
        "nis/n03_tradersintro.wav", "nis/n04_learnofsacking.wav", "nis/n05_p2intro.wav",
        "nis/n06_supernova.wav", "nis/n07_p3vstraders.wav", "nis/n08_awareness.wav",
        "nis/n09_headshot.wav", "nis/n10_miningfacility.wav", 0, 0, 0,
        "animatic/anim_00_opening.wav", "animatic/anim_01_02.wav",
        "animatic/anim_02_03.wav", "animatic/anim_03_04.wav", "animatic/anim_04_05.wav",
        "animatic/anim_05_06.wav", "animatic/anim_06_07.wav", "animatic/anim_07_08.wav",
        "animatic/anim_08_09.wav", "animatic/anim_09_10.wav", "animatic/anim_10_11.wav",
        "animatic/anim_11_12.wav", "animatic/anim_12_13.wav", "animatic/anim_13_14.wav",
        "animatic/anim_14_15.wav", "animatic/anim_15_16.wav", "animatic/anim_ending.wav"
    };
    return track >= 0 && track < (int)(sizeof(paths)/sizeof(paths[0])) ? paths[track] : 0;
}
#endif
