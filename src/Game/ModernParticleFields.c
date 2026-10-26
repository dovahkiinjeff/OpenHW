#include "../Modern/DiagnosticPolicy.h"
#include "ModernParticleFields.h"
#include "ParticleDensityField.h"
#include "Universe.h"
#include <stdio.h>

#define PF_MAX 2048
#define PF_MAX_POINTS 65536
typedef struct ParticleFieldStyle { HWModernHarvestableNebulaStyle style;int overridden; } ParticleFieldStyle;
static ParticleFieldStyle fields[PF_MAX];
static PDPoint *points;
static int pointCount,fieldCount,dirty=1,initialized;
static PDField densityField;
static HWModernVolumetricDustVolume *upload;
static HWModernHarvestableNebulaStyle defaultStyle;
static const Node *observedMinorHead;
static int observedMinorCount;
static int originalParticlesVisible=1;
static int originalRopeColorEnabled;
static float originalRopeColor[3]={.66f,.42f,.22f},originalRopeOpacity=1;
int modernOriginalRopeColor(float rgb[3]){memcpy(rgb,originalRopeColor,sizeof(originalRopeColor));return originalRopeColorEnabled;}
void modernOriginalRopeSetColor(const float rgb[3]){int i;for(i=0;i<3;i++)if(!isfinite(rgb[i])||rgb[i]<0||rgb[i]>1)return;memcpy(originalRopeColor,rgb,sizeof(originalRopeColor));originalRopeColorEnabled=1;}
void modernOriginalRopeResetColor(void){originalRopeColorEnabled=0;originalRopeColor[0]=.66f;originalRopeColor[1]=.42f;originalRopeColor[2]=.22f;}
float modernOriginalRopeOpacity(void){return originalRopeOpacity;}
void modernOriginalRopeSetOpacity(float opacity){if(isfinite(opacity)&&opacity>=0&&opacity<=1)originalRopeOpacity=opacity;}
void modernOriginalRopeReset(void){modernOriginalRopeResetColor();originalRopeOpacity=1;}
static float originalOpacity=168.0f/255.0f;
float modernParticleFieldsOriginalOpacity(void){return originalOpacity;}
void modernParticleFieldsSetOriginalOpacity(float opacity){if(isfinite(opacity)&&opacity>=0&&opacity<=1)originalOpacity=opacity;}
static int originalColorEnabled;
static float originalColor[3]={.66f,.48f,.28f};
int modernParticleFieldsOriginalColor(float rgb[3]){memcpy(rgb,originalColor,sizeof(originalColor));return originalColorEnabled;}
void modernParticleFieldsSetOriginalColor(const float rgb[3]){
    int i;for(i=0;i<3;i++)if(!isfinite(rgb[i])||rgb[i]<0||rgb[i]>1)return;
    memcpy(originalColor,rgb,sizeof(originalColor));originalColorEnabled=1;
}
void modernParticleFieldsResetOriginalColor(void){originalColorEnabled=0;originalColor[0]=.66f;originalColor[1]=.48f;originalColor[2]=.28f;}
int modernParticleFieldsOriginalVisible(void){return originalParticlesVisible;}
void modernParticleFieldsSetOriginalVisible(int visible){originalParticlesVisible=!!visible;}

void modernParticleFieldsResetStyles(void)
{
    int i;
    HWModernHarvestableNebulaStyle initial={1u,1.0f,0.34f,0.72f,0.82f,0.12f,
        {0.66f,0.48f,0.28f},0.00012f,0.72f,0.58f};
    originalParticlesVisible=1;
    originalOpacity=168.0f/255.0f;
    modernParticleFieldsResetOriginalColor();
    defaultStyle=initial;for(i=0;i<PF_MAX;i++)fields[i].overridden=0;dirty=1;
}
void modernParticleFieldsReset(void)
{
    modernOriginalRopeReset();
    free(points);points=NULL;free(upload);upload=NULL;pdFree(&densityField);
    pointCount=fieldCount=initialized=0;observedMinorHead=NULL;observedMinorCount=0;
    memset(fields,0,sizeof(fields));modernParticleFieldsResetStyles();
    hwModernGraphicsSetParticleFieldVolumes(NULL,0);
    hwModernGraphicsSetParticleFieldDensity(NULL,0);
}
void modernParticleFieldsDirty(void){dirty=1;}
int modernParticleFieldsCount(void){return fieldCount;}
HWModernHarvestableNebulaStyle *modernParticleFieldsStyle(int field,int edit)
{
    if(field<0 || field>=PF_MAX)return &defaultStyle;
    if(edit&&!fields[field].overridden){fields[field].style=defaultStyle;fields[field].overridden=1;dirty=1;}
    return fields[field].overridden?&fields[field].style:&defaultStyle;
}
void modernParticleFieldsResetOverride(int field)
{
    if(field>=0&&field<PF_MAX)fields[field].overridden=0;else modernParticleFieldsResetStyles();
    dirty=1;
}
static int collectParticles(void)
{
    Node *node;int k;
    if(!points)points=malloc(PF_MAX_POINTS*sizeof(*points));
    if(!points)return 0;
    pointCount=0;
    for(node=universe.MinorSpaceObjList.head;node;node=node->next){
        SpaceObj *object=(SpaceObj *)listGetStructOfNode(node);
        if(object->objtype!=OBJ_AsteroidType || ((Asteroid *)object)->asteroidtype!=Asteroid0)continue;
        if(pointCount==PF_MAX_POINTS){HW_DIAG_FPRINTF(stderr,"[ParticleDensity] Particle capacity reached: %d.\n",PF_MAX_POINTS);break;}
        points[pointCount].p[0]=object->posinfo.position.x;
        points[pointCount].p[1]=object->posinfo.position.y;
        points[pointCount].p[2]=object->posinfo.position.z;
        for(k=0;k<3;k++)if(!isfinite(points[pointCount].p[k])||fabsf(points[pointCount].p[k])>100000000.0f)break;
        if(k==3)pointCount++;
    }
    if(!pointCount)return 0;
    fieldCount=pdCluster(points,pointCount,PF_MAX);
    if(fieldCount<0){fieldCount=0;return 0;}
    if(fieldCount>PF_MAX){HW_DIAG_FPRINTF(stderr,"[ParticleDensity] Clump limit reached: %d.\n",PF_MAX);fieldCount=PF_MAX;}
    observedMinorHead=universe.MinorSpaceObjList.head;observedMinorCount=universe.MinorSpaceObjList.num;
    HW_DIAG_FPRINTF(stderr,"[ParticleDensity] %d actual Asteroid0 particles -> %d adaptive cloud formations.\n",pointCount,fieldCount);
    for(k=0;k<fieldCount;k++){
        int i,n=0;float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
        for(i=0;i<pointCount;i++)if(points[i].group==k){int axis;n++;for(axis=0;axis<3;axis++){lo[axis]=fminf(lo[axis],points[i].p[axis]);hi[axis]=fmaxf(hi[axis],points[i].p[axis]);}}
        HW_DIAG_FPRINTF(stderr,"[ParticleDensity] Cloud %d: %d particles; bounds (%.0f %.0f %.0f) - (%.0f %.0f %.0f).\n",k,n,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
    }
    return 1;
}
void modernParticleFieldsUpdate(void)
{
    float spread[PF_MAX],softness[PF_MAX];int i,k,count=0;
    if(initialized&&(observedMinorHead!=universe.MinorSpaceObjList.head||observedMinorCount!=universe.MinorSpaceObjList.num))initialized=0;
    if(!initialized){if(!collectParticles())return;initialized=1;dirty=1;}
    if(!dirty)return;
    for(i=0;i<fieldCount;i++){
        const HWModernHarvestableNebulaStyle *s=modernParticleFieldsStyle(i,0);
        spread[i]=defaultStyle.enabled&&s->enabled&&s->density>0?s->radiusScale:0;
        softness[i]=s->edgeFeather;
    }
    if(!pdBakeClouds(points,pointCount,spread,softness,fieldCount,&densityField)){
        HW_DIAG_FPRINTF(stderr,"[ParticleDensity] Bake incomplete (allocation or %d-brick capacity). Keeping previous renderer data.\n",PD_MAX_TILES);
        return;
    }
    free(upload);upload=malloc((size_t)(densityField.count?densityField.count:1)*sizeof(*upload));
    if(!upload)return;
    for(i=0;i<densityField.count;i++){
        const PDTile *tile=&densityField.tiles[i];
        const HWModernHarvestableNebulaStyle *s=modernParticleFieldsStyle(tile->group,0);
        HWModernVolumetricDustVolume *v;float *values;
        if(tile->peak<=0)continue;
        v=&upload[count++];memset(v,0,sizeof(*v));
        v->enabled=1;v->shape=HW_MODERN_DUST_PARTICLE_DENSITY;
        v->shapeSeed=(unsigned)i*PD_VALUES;
        v->flags=HW_MODERN_DUST_RECEIVE_MISSION_KEY|HW_MODERN_DUST_RECEIVE_LOCAL_LIGHTS;
        for(k=0;k<3;k++){v->position[k]=tile->minimum[k]+tile->span*.5f;v->size[k]=tile->span;v->color[k]=s->color[k];}
        /* Keep the renderer's conservative density bounds valid even where
           many kernels overlap. Grid samples are normalized by this peak. */
        v->density=s->density*tile->peak;v->coverage=s->edgeFeather;
        v->scattering=s->scattering;v->absorption=s->absorption;v->anisotropy=.18f;
        v->noiseScale=s->noiseScale;v->noiseDetail=s->noiseDetail;v->shapeVariation=s->shapeVariation;
        values=densityField.values+(size_t)i*PD_VALUES;
        for(k=0;k<PD_VALUES;k++)values[k]/=tile->peak;
    }
    hwModernGraphicsSetParticleFieldDensity(densityField.values,(unsigned)densityField.count*PD_VALUES);
    hwModernGraphicsSetParticleFieldVolumes(upload,(unsigned)count);dirty=0;
    HW_DIAG_FPRINTF(stderr,"[ParticleDensity] Baked %d sparse density bricks. Continuous formation support; logarithmic hotspot compression.\n",count);
}
void modernParticleFieldsWrite(FILE *f)
{
    int i;
    fprintf(f,"ORIGINAL_ROPE_STYLE %d %.9g %.9g %.9g %.9g\n",originalRopeColorEnabled,originalRopeColor[0],originalRopeColor[1],originalRopeColor[2],originalRopeOpacity);
    fprintf(f,"PARTICLE_ORIGINAL_VISIBLE %d\n",originalParticlesVisible);
    fprintf(f,"PARTICLE_ORIGINAL_OPACITY %.8f\n",originalOpacity);
    fprintf(f,"PARTICLE_ORIGINAL_COLOR %d %.6f %.6f %.6f\n",originalColorEnabled,originalColor[0],originalColor[1],originalColor[2]);
    fprintf(f,"# Particle-driven cloud formations; smooth support and compressed density peaks.\n");
    for(i=-1;i<fieldCount;i++)if(i<0||fields[i].overridden){
        const HWModernHarvestableNebulaStyle *s=modernParticleFieldsStyle(i,0);
        fprintf(f,"PARTICLE_DENSITY_STYLE %d %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",i,s->enabled,s->radiusScale,s->density,s->edgeFeather,s->scattering,s->absorption,s->color[0],s->color[1],s->color[2],s->noiseScale,s->noiseDetail,s->shapeVariation);
    }
}
int modernParticleFieldsRead(const char *line)
{
    HWModernHarvestableNebulaStyle s;int id,k;float v[11];const char *payload;
    if(!strncmp(line,"ORIGINAL_ROPE_STYLE ",20)){
        int enabled;float rgb[3],opacity;
        if(sscanf(line+20,"%d %f %f %f %f",&enabled,&rgb[0],&rgb[1],&rgb[2],&opacity)==5){
            for(k=0;k<3;k++)if(!isfinite(rgb[k])||rgb[k]<0||rgb[k]>1)return 1;
            if(!isfinite(opacity)||opacity<0||opacity>1||(enabled!=0&&enabled!=1))return 1;
            if(enabled)modernOriginalRopeSetColor(rgb);else modernOriginalRopeResetColor();
            modernOriginalRopeSetOpacity(opacity);
        }return 1;
    }
    if(!strncmp(line,"PARTICLE_ORIGINAL_OPACITY ",26)){
        float opacity;if(sscanf(line+26,"%f",&opacity)==1)modernParticleFieldsSetOriginalOpacity(opacity);
        return 1;
    }
    if(!strncmp(line,"PARTICLE_ORIGINAL_COLOR ",24)){
        int enabled;float rgb[3];
        if(sscanf(line+24,"%d %f %f %f",&enabled,&rgb[0],&rgb[1],&rgb[2])==4){
            if(enabled==1)modernParticleFieldsSetOriginalColor(rgb);
            else if(enabled==0)modernParticleFieldsResetOriginalColor();
        }return 1;
    }
    if(!strncmp(line,"PARTICLE_ORIGINAL_VISIBLE ",26)){
        int visible;if(sscanf(line+26,"%d",&visible)==1&&(visible==0||visible==1))modernParticleFieldsSetOriginalVisible(visible);
        return 1;
    }
    if(!strncmp(line,"PARTICLE_FIELD_REGION ",22))return 1; /* retired envelopes */
    if(!strncmp(line,"PARTICLE_DENSITY_STYLE ",22))payload=line+22;
    else if(!strncmp(line,"PARTICLE_FIELD ",15)){payload=line+15;if(atoi(payload)!=-1)return 1;}
    else return 0;
    if(sscanf(payload,"%d %u %f %f %f %f %f %f %f %f %f %f %f",&id,&s.enabled,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10])!=13||id < -1||id>=PF_MAX)return 1;
    for(k=0;k<11;k++)if(!isfinite(v[k])||v[k]<0||v[k]>8)return 1;
    if(v[0]>4||v[8]<.00001f||v[8]>.02f)return 1;
    s.enabled=!!s.enabled;s.radiusScale=v[0];s.density=v[1];s.edgeFeather=v[2];s.scattering=v[3];s.absorption=v[4];
    for(k=0;k<3;k++)s.color[k]=v[k+5];
    s.noiseScale=v[8];s.noiseDetail=v[9];s.shapeVariation=v[10];
    *modernParticleFieldsStyle(id,1)=s;dirty=1;return 1;
}
