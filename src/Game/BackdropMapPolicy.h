#ifndef HW_BACKDROP_MAP_POLICY_H
#define HW_BACKDROP_MAP_POLICY_H
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Retail skirmish maps name the same EZ environments used by the campaign.
   The Classic default is the EZ05 geometry/star composition with a different
   legacy master tint. Its matching remastered environment is therefore 05. */
static int hwBackdropAssetForName(const char *path)
{
    const char *name = path, *p;
    char lower[32]; size_t n;
    int mission;
    if (path == NULL) return 0;
    for (p = path; *p; ++p) if (*p == '/' || *p == '\\') name = p + 1;
    n = strlen(name); if (n >= sizeof(lower)) return 0;
    for (size_t i=0;i<=n;++i) lower[i]=(char)tolower((unsigned char)name[i]);
    if (strcmp(lower,"default.btg")==0 || strcmp(lower,"default")==0) return 5;
    if ((n != 4 && n != 8) || lower[0]!='e' || lower[1]!='z' ||
        !isdigit((unsigned char)lower[2]) || !isdigit((unsigned char)lower[3]) ||
        (n==8 && strcmp(lower+4,".btg")!=0)) return 0;
    mission=(lower[2]-'0')*10+lower[3]-'0';
    if (mission==3) return 1;
    return mission>=1 && mission<=16 ? mission : 0;
}

static int hwBackdropContext(int story, int mission, const char *map, const char *background)
{
    unsigned int hash=2166136261u;
    const char *p;
    if (story) return mission>=1 && mission<=16 ? mission : 0;
    if ((!map || !*map) && (!background || !*background)) return 0;
    p=map && *map ? map : background;
    for (;*p;++p)
    {
        unsigned char c=(unsigned char)*p;
        if(c=='\\')c='/';
        hash^=(unsigned int)tolower(c);hash*=16777619u;
    }
    return (int)((hash & 0x3fffffffu) | 0x40000000u);
}

static void hwBackdropLightingName(char *out, size_t capacity, int context)
{
    if(context>=0x40000000)
        snprintf(out,capacity,"Multiplayer-%08x.rtxlight",(unsigned int)context);
    else snprintf(out,capacity,"Mission%02d.rtxlight",context);
}
static void hwBackdropMapName(char *out, size_t capacity, int context)
{
    if(context>=0x40000000)
        snprintf(out,capacity,"Multiplayer-%08x.rtxmap",(unsigned int)context);
    else snprintf(out,capacity,"Mission%02d.rtxmap",context);
}
#endif
