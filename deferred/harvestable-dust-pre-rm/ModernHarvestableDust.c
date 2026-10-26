#include "ModernHarvestableDust.h"
#include "Clouds.h"
#include "File.h"
#include "Universe.h"
#include "Sensors.h"
#include "SinglePlayer.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static HWModernVolumetricDustVolume style;
static bool32 loaded;
static const char *settings = "HarvestableDust.rtxdust";
#define MDUST_MAX_OVERRIDES 2048
typedef struct DustOverride
{
    sdword mission;
    udword resourceId;
    HWModernVolumetricDustVolume material;
} DustOverride;
static DustOverride overrides[MDUST_MAX_OVERRIDES];
static sdword overrideCount;
#define MDUST_MAX_PRESETS 32
static HWModernVolumetricDustVolume presets[MDUST_MAX_PRESETS];
static sdword presetCount;

/* Stable shuffled assignment: no camera, time, load order, or harvesting dependence. */
static HWModernVolumetricDustVolume *assignedStyle(udword resourceId)
{
    udword hash;
    sdword mission=max(0,(sdword)spGetCurrentMission());
    if(!presetCount || mission<1 || mission>16) return &style;
    hash=resourceId ^ ((udword)mission*0x9e3779b9u) ^ 0x7f4a7c15u;
    hash^=hash>>16; hash*=0x7feb352du;
    hash^=hash>>15; hash*=0x846ca68bu; hash^=hash>>16;
    return &presets[hash%(udword)presetCount];
}

static sdword overrideIndex(udword resourceId)
{
    sdword i,mission=max(0,(sdword)spGetCurrentMission());
    for(i=0;i<overrideCount;i++)
        if(overrides[i].mission==mission && overrides[i].resourceId==resourceId) return i;
    return -1;
}

HWModernVolumetricDustVolume *modernHarvestableDustStyleFor(udword resourceId,bool32 edit)
{
    sdword index;
    modernHarvestableDustStyle();
    index=overrideIndex(resourceId);
    if(index>=0) return &overrides[index].material;
    if(!edit) return assignedStyle(resourceId);
    if(overrideCount>=MDUST_MAX_OVERRIDES) return NULL;
    index=overrideCount++;
    overrides[index].mission=max(0,(sdword)spGetCurrentMission());
    overrides[index].resourceId=resourceId;
    overrides[index].material=*assignedStyle(resourceId);
    return &overrides[index].material;
}

bool32 modernHarvestableDustHasOverride(udword resourceId)
{
    modernHarvestableDustStyle();
    return overrideIndex(resourceId)>=0;
}

void modernHarvestableDustResetOverride(udword resourceId)
{
    sdword index;
    modernHarvestableDustStyle();
    index=overrideIndex(resourceId);
    if(index>=0) { --overrideCount; overrides[index]=overrides[overrideCount]; }
}

static bool32 parseStyle(const char *line,HWModernVolumetricDustVolume *target)
{
    float v[15]; unsigned int enabled; int i;
    if(sscanf(line,"STYLE %u %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
        &enabled,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],
        &v[8],&v[9],&v[10],&v[11],&v[12],&v[13],&v[14])!=16 || enabled>1) return FALSE;
    for(i=0;i<15;i++) if(!isfinite(v[i])) return FALSE;
    if(v[0]<0.25f || v[0]>4 || v[1]<0.25f || v[1]>4 || v[2]<0.25f || v[2]>4 ||
       v[3]<0 || v[3]>8 || v[4]<0 || v[4]>8 || v[5]<0 || v[5]>8 ||
       v[6]<-0.95f || v[6]>0.95f || v[7]<0.05f || v[7]>2 ||
       v[8]<0 || v[8]>8 || v[9]<0 || v[9]>8 || v[10]<0 || v[10]>8 ||
       v[11]<0.00001f || v[11]>0.02f || v[12]<0 || v[12]>2 ||
       v[13]<0 || v[13]>2 || v[14]!=0) return FALSE;
    target->enabled=enabled;
    for(i=0;i<3;i++) { target->size[i]=v[i]; target->color[i]=v[8+i]; }
    target->density=v[3]; target->scattering=v[4]; target->absorption=v[5];
    target->anisotropy=v[6]; target->coverage=v[7]; target->noiseScale=v[11];
    target->noiseDetail=v[12]; target->shapeVariation=v[13];
    return TRUE;
}

static void defaults(void)
{
    memset(&style,0,sizeof(style));
    style.enabled=1; style.shape=2;
    style.size[0]=1.15f; style.size[1]=0.9f; style.size[2]=1.05f;
    style.density=0.42f; style.scattering=0.8f; style.absorption=0.16f;
    style.anisotropy=0.25f; style.coverage=0.85f;
    style.color[0]=0.67f; style.color[1]=0.57f; style.color[2]=0.38f;
    style.noiseScale=0.0015f; style.noiseDetail=0.65f; style.shapeVariation=1.1f;
    style.flags=HW_MODERN_DUST_RECEIVE_MISSION_KEY | HW_MODERN_DUST_RECEIVE_LOCAL_LIGHTS |
                HW_MODERN_DUST_CAST_VOLUMETRIC_SHADOW;
}

void modernHarvestableDustReload(void)
{
    filehandle handle;
    char line[512];
    int version;
    defaults(); overrideCount=0; presetCount=0; loaded=TRUE;
    handle=fileOpen((char *)settings,FF_TextMode|FF_IgnoreBIG|FF_UserSettingsPath|FF_ReturnNULLOnFail);
    if (!handle)
    {
        char path[1024]; char *base=SDL_GetBasePath();
        if (!base) return;
        snprintf(path,sizeof(path),"%sDustClouds/%s",base,settings); SDL_free(base);
        handle=fileOpen(path,FF_TextMode|FF_IgnoreBIG|FF_IgnorePrepend|FF_ReturnNULLOnFail);
    }
    if (!handle) return;
    if (fgets(line,sizeof(line),fileStream(handle)) &&
        sscanf(line,"RTXHARVESTABLEDUST %d",&version)==1 && (version>=1 && version<=3) &&
        fgets(line,sizeof(line),fileStream(handle)) &&
        parseStyle(line,&style))
    {
        while(version>=2 && fgets(line,sizeof(line),fileStream(handle)))
        {
            DustOverride entry; int offset=0,i;
            if(version>=3 && strncmp(line,"PRESET ",7)==0)
            {
                HWModernVolumetricDustVolume material=style;
                if(presetCount<MDUST_MAX_PRESETS && parseStyle(line+7,&material))
                    presets[presetCount++]=material;
                continue;
            }
            entry.material=style;
            if(sscanf(line,"CLOUD %d %u %n",&entry.mission,&entry.resourceId,&offset)!=2 ||
               offset<=0 || entry.mission<0 || entry.mission>16 || !parseStyle(line+offset,&entry.material)) continue;
            for(i=0;i<overrideCount;i++)
                if(overrides[i].mission==entry.mission && overrides[i].resourceId==entry.resourceId) break;
            if(i<MDUST_MAX_OVERRIDES) { overrides[i]=entry; if(i==overrideCount) ++overrideCount; }
        }
    }
    else fprintf(stderr,"[HarvestableDust] Invalid style file; using defaults.\n");
    fileClose(handle);
}

HWModernVolumetricDustVolume *modernHarvestableDustStyle(void)
{
    if(!loaded) modernHarvestableDustReload();
    return &style;
}

static void writeMaterial(FILE *fp,const HWModernVolumetricDustVolume *s)
{
    fprintf(fp,"STYLE %u %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.8f %.6f %.6f 0\n",
        s->enabled,s->size[0],s->size[1],s->size[2],s->density,
        s->scattering,s->absorption,s->anisotropy,s->coverage,
        s->color[0],s->color[1],s->color[2],s->noiseScale,s->noiseDetail,s->shapeVariation);
}

static void writeStyle(FILE *fp)
{
    sdword i;
    fprintf(fp,"RTXHARVESTABLEDUST 3\n"); writeMaterial(fp,&style);
    for(i=0;i<presetCount;i++)
    {
        fprintf(fp,"PRESET "); writeMaterial(fp,&presets[i]);
    }
    for(i=0;i<overrideCount;i++)
    {
        fprintf(fp,"CLOUD %d %u ",overrides[i].mission,overrides[i].resourceId);
        writeMaterial(fp,&overrides[i].material);
    }
}

bool32 modernHarvestableDustSave(void)
{
    filehandle handle; FILE *fp; char root[1024],directory[1024],path[1024]; char *base;
    modernHarvestableDustStyle();
    handle=fileOpen((char *)settings,FF_WriteMode|FF_TextMode|FF_IgnoreBIG|FF_UserSettingsPath|FF_ReturnNULLOnFail);
    if(!handle) return FALSE;
    fp=fileStream(handle); if(!fp) { fileClose(handle); return FALSE; }
    writeStyle(fp); if(ferror(fp)) { fileClose(handle); return FALSE; } fileClose(handle);
    base=SDL_GetBasePath(); if(!base) return FALSE;
    snprintf(root,sizeof(root),"%sRTXExports",base);
    snprintf(directory,sizeof(directory),"%sRTXExports/Global",base);
    snprintf(path,sizeof(path),"%sRTXExports/Global/%s",base,settings); SDL_free(base);
    if(!fileMakeDirectory(root) || !fileMakeDirectory(directory)) return FALSE;
    fp=fopen(path,"wb"); if(!fp) return FALSE;
    writeStyle(fp); if(fclose(fp)!=0) return FALSE;
    return TRUE;
}

bool32 modernHarvestableDustActive(void)
{
    return hwModernGraphicsIsRaytracingSceneOpen() && !smSensorsActive;
}

bool32 modernHarvestableDustActiveFor(udword resourceId)
{
    return modernHarvestableDustActive() && modernHarvestableDustStyleFor(resourceId,FALSE)->enabled;
}

void modernHarvestableDustUpdate(void)
{
    HWModernVolumetricDustVolume volumes[256];
    unsigned int count=0;
    Node *node;
    if(modernHarvestableDustActive())
    for(node=universe.ResourceList.head;node && count<256;node=node->next)
    {
        DustCloud *dust=(DustCloud *)listGetStructOfNode(node);
        cloudSystem *system;
        HWModernVolumetricDustVolume *v;
        float radius;
        int axis;
        const HWModernVolumetricDustVolume *material;
        if(dust->objtype!=OBJ_DustType || (dust->flags & SOF_Hide) || dust->resourcevalue<=0 || !dust->stub) continue;
        system=(cloudSystem *)dust->stub;
        material=modernHarvestableDustStyleFor(dust->resourceID.resourceNumber,FALSE);
        if(!material->enabled) continue;
        radius=system->radius * system->healthFactor;
        if(radius<=0) continue;
        v=&volumes[count++]; *v=*material;
        v->shapeSeed=0x9e3779b9u ^ ((unsigned int)dust->resourceID.resourceNumber * 2654435761u);
        v->position[0]=dust->posinfo.position.x;
        v->position[1]=dust->posinfo.position.y;
        v->position[2]=dust->posinfo.position.z;
        for(axis=0;axis<3;axis++) v->size[axis]=radius*2.0f*material->size[axis];
        /* Resource identity anchors shape/noise; never use camera position or time. */
        v->rotationDegrees[1]=(float)(v->shapeSeed%360u);
    }
    hwModernGraphicsSetHarvestableDustVolumes(count?volumes:NULL,count);
}
