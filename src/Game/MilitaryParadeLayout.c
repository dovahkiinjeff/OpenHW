#include "MilitaryParadeLayout.h"
#include "Universe.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif
static ParadeLayoutDocument layouts[2];
static bool32 loaded[2];
bool32 paradeLayoutIdentityValid(ShipRace race, ShipType type)
{
    return race<NUM_RACES && type<TOTAL_NUM_SHIPS &&
        type>=FirstShipTypeOfRace[race] && type<=LastShipTypeOfRace[race] &&
        (RacesAllowedForGivenShip[type] & RaceToRaceBits(race));
}
bool32 paradeLayoutValid(const ParadeShipLayout *e)
{
    int i;
    if(!e || (e->enabled!=0 && e->enabled!=1)) return FALSE;
    for(i=0;i<3;i++) if(!isfinite(e->position[i]) || fabsf(e->position[i])>100000 ||
        !isfinite(e->growth[i]) || fabsf(e->growth[i])>100000 ||
        !isfinite(e->rotation[i]) || fabsf(e->rotation[i])>360) return FALSE;
    return TRUE;
}
void paradeLayoutRotate(const real32 r[3],const real32 v[3],real32 out[3])
{
    float x=r[0]*.01745329252f,y=r[1]*.01745329252f,z=r[2]*.01745329252f;
    float a=v[0], b=cosf(x)*v[1]-sinf(x)*v[2], c=sinf(x)*v[1]+cosf(x)*v[2];
    float d=cosf(y)*a+sinf(y)*c, f=-sinf(y)*a+cosf(y)*c;
    out[0]=cosf(z)*d-sinf(z)*b;out[1]=sinf(z)*d+cosf(z)*b;out[2]=f;
}
static bool32 paradePath(ShipRace host,char *path,size_t bytes)
{
    char *base=SDL_GetBasePath();int n;
    if(!base || host>R2) {if(base)SDL_free(base);return FALSE;}
    n=snprintf(path,bytes,"%sParade-%s.cfg",base,host==R1?"Kushan":"Taiidan");
    SDL_free(base);return n>0 && (size_t)n<bytes;
}
bool32 paradeLayoutApply(ShipRace host,const ParadeLayoutDocument *doc)
{
    int r,t;
    if(host>R2 || !doc)return FALSE;
    for(r=0;r<NUM_RACES;r++)for(t=0;t<TOTAL_NUM_SHIPS;t++)
        if(!paradeLayoutValid(&doc->ships[r][t]) ||
           (doc->ships[r][t].enabled && !paradeLayoutIdentityValid(r,t))) return FALSE;
    layouts[host]=*doc;loaded[host]=TRUE;return TRUE;
}
static bool32 paradeReadPath(const char *path,ParadeLayoutDocument *out)
{
    char line[512],raceName[64],typeName[64];
    FILE *fp;ParadeLayoutDocument doc;int version;
    memset(&doc,0,sizeof(doc));fp=fopen(path,"rb");
    if(!fp)return FALSE;
    if(!fgets(line,sizeof(line),fp) || sscanf(line,"OPENHW_PARADE %d",&version)!=1 || version!=1) {fclose(fp);return FALSE;}
    while(fgets(line,sizeof(line),fp)) {
        ParadeShipLayout e={0};ShipRace r;ShipType t;
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\r')continue;
        if(sscanf(line,"%63s %63s %f %f %f %f %f %f %f %f %f",raceName,typeName,
            &e.position[0],&e.position[1],&e.position[2],&e.rotation[0],&e.rotation[1],&e.rotation[2],
            &e.growth[0],&e.growth[1],&e.growth[2])!=11) {fclose(fp);return FALSE;}
        r=StrToShipRace(raceName);t=StrToShipType(typeName);e.enabled=TRUE;
        if(!paradeLayoutIdentityValid(r,t) || !paradeLayoutValid(&e) || doc.ships[r][t].enabled) {fclose(fp);return FALSE;}
        doc.ships[r][t]=e;
    }
    if(ferror(fp)) {fclose(fp);return FALSE;}
    fclose(fp);*out=doc;return TRUE;
}
bool32 paradeLayoutReload(ShipRace host)
{
    char path[1024];ParadeLayoutDocument doc;FILE *fp;
    if(!paradePath(host,path,sizeof(path)))return FALSE;
    fp=fopen(path,"rb");if(!fp){memset(&layouts[host],0,sizeof(layouts[host]));loaded[host]=TRUE;return TRUE;}
    fclose(fp);
    return paradeReadPath(path,&doc) && paradeLayoutApply(host,&doc);
}
void paradeLayoutGet(ShipRace host,ParadeLayoutDocument *out)
{
    if(host>R2 || !out)return;
    if(!loaded[host]) {loaded[host]=TRUE;paradeLayoutReload(host);}
    *out=layouts[host];
}
bool32 paradeLayoutRuntime(ShipRace host,ShipRace race,ShipType type,sdword ordinal,
    ParadeShipLayout *entry,real32 position[3])
{
    int i;
    if(host>R2 || !paradeLayoutIdentityValid(race,type))return FALSE;
    if(!loaded[host]) {loaded[host]=TRUE;paradeLayoutReload(host);}
    *entry=layouts[host].ships[race][type];if(!entry->enabled)return FALSE;
    for(i=0;i<3;i++)position[i]=entry->position[i]+ordinal*entry->growth[i];
    return TRUE;
}
static bool32 paradeWritePath(ShipRace host,const char *path,const ParadeLayoutDocument *doc)
{
    char temp[1100],backup[1100];FILE *fp,*old,*copy;int r,t,ok=1;char block[8192];size_t n;
    if(host>R2 || !doc)return FALSE;
    for(r=0;r<NUM_RACES;r++)for(t=0;t<TOTAL_NUM_SHIPS;t++)
        if(!paradeLayoutValid(&doc->ships[r][t]) || (doc->ships[r][t].enabled && !paradeLayoutIdentityValid(r,t)))return FALSE;
    snprintf(temp,sizeof(temp),"%s.tmp",path);fp=fopen(temp,"wb");if(!fp)return FALSE;
    if(fprintf(fp,"OPENHW_PARADE 1\n")<0)ok=0;
    for(r=0;r<NUM_RACES;r++)for(t=0;t<TOTAL_NUM_SHIPS;t++) {
        const ParadeShipLayout *e=&doc->ships[r][t];if(!e->enabled)continue;
        if(fprintf(fp,"%s %s %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
            ShipRaceToStr(r),ShipTypeToStr(t),e->position[0],e->position[1],e->position[2],
            e->rotation[0],e->rotation[1],e->rotation[2],e->growth[0],e->growth[1],e->growth[2])<0)ok=0;
    }
    if(fclose(fp)!=0)ok=0;if(!ok){remove(temp);return FALSE;}
    old=fopen(path,"rb");
    if(old) {
        snprintf(backup,sizeof(backup),"%s.bak-%llu",path,(unsigned long long)SDL_GetPerformanceCounter());
        copy=fopen(backup,"wb");if(!copy){fclose(old);remove(temp);return FALSE;}
        while((n=fread(block,1,sizeof(block),old))>0)if(fwrite(block,1,n,copy)!=n)ok=0;
        if(ferror(old))ok=0;fclose(old);if(fclose(copy)!=0)ok=0;
        if(!ok){remove(temp);return FALSE;}
    }
#ifdef _WIN32
    if(!MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return FALSE;
#else
    if(rename(temp,path)!=0)return FALSE;
#endif
    return TRUE;
}
bool32 paradeLayoutSave(ShipRace host,const ParadeLayoutDocument *doc)
{
    char path[1024];
    return paradePath(host,path,sizeof(path)) && paradeWritePath(host,path,doc) && paradeLayoutApply(host,doc);
}
static bool32 paradeMultiplayerPath(ShipRace host,const char *name,char *path,size_t bytes)
{
    char *base;int i,n;
    if(host>R2 || !name || !name[0] || strlen(name)>32)return FALSE;
    for(i=0;name[i];i++)if(!isalnum((unsigned char)name[i]) && name[i]!='-' && name[i]!='_')return FALSE;
    base=SDL_GetBasePath();if(!base)return FALSE;
    n=snprintf(path,bytes,"%sMultiplayerParadePresets/Parade-%s-%s.cfg",base,host==R1?"Kushan":"Taiidan",name);
    SDL_free(base);return n>0 && (size_t)n<bytes;
}
bool32 paradeLayoutSaveMultiplayer(ShipRace host,const char *name,const ParadeLayoutDocument *doc)
{
    char path[1024],directory[1024],*separator;
    if(!paradeMultiplayerPath(host,name,path,sizeof(path)))return FALSE;
    snprintf(directory,sizeof(directory),"%s",path);separator=strrchr(directory,'/');
    if(!separator)return FALSE;*separator=0;
#ifdef _WIN32
    if(!CreateDirectoryA(directory,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;
#else
    /* An existing directory is handled by the subsequent atomic writer. */
    mkdir(directory,0755);
#endif
    /* Deliberately no apply/runtime call: these are authoring files only. */
    return paradeWritePath(host,path,doc);
}
bool32 paradeLayoutLoadMultiplayer(ShipRace host,const char *name,ParadeLayoutDocument *doc)
{
    char path[1024];return paradeMultiplayerPath(host,name,path,sizeof(path)) && paradeReadPath(path,doc);
}
