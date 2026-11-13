/* Runtime side table keeps the Ship/save structure and multiplayer simulation
   layout unchanged. Only edited ships allocate private material instances. */
typedef struct PaintMesh {
    struct PaintMesh *next;
    meshdata *mesh;
    materialentry *materials;
    trhandle *handles;
    sdword count;
    color base,detail;
} PaintMesh;
typedef struct PaintShip {struct PaintShip *next;unsigned number,variant;const Ship *owner;ShipAppearance appearance;PaintMesh *meshes;} PaintShip;
static PaintShip *paintShips[65536];
static PaintShip *paintActive;
static const Ship *paintOwner;
static unsigned paintGeneration;
static void paintSetOwner(const void *owner) {paintOwner=(const Ship *)owner;}
const Ship *shipAppearanceAt(unsigned number,ShipAppearance *appearance)
{
    if(number>=65536 || !paintShips[number]) return NULL;
    *appearance=paintShips[number]->appearance;return paintShips[number]->owner;
}

static PaintShip *paintFind(const Ship *ship)
{
    PaintShip *entry;
    if(!ship || ship->objtype!=OBJ_ShipType) return NULL;
    entry=paintShips[ship->shipID.shipNumber];
    return entry && entry->owner==ship ? entry : NULL;
}
static unsigned paintSurfaceVariant(unsigned scheme)
{
    PaintShip *entry=paintFind(paintOwner);
    return entry && (entry->appearance.mask&3) ? entry->variant|scheme : scheme;
}
static trhandle paintMaterialHandle(const materialentry *material)
{
    PaintShip *ship;PaintMesh *mesh;
    for(ship=paintActive;ship;ship=ship->next)for(mesh=ship->meshes;mesh;mesh=mesh->next) {
        uintptr_t value=(uintptr_t)material,begin=(uintptr_t)mesh->materials;
        if(value>=begin && value<begin+(size_t)mesh->count*sizeof(materialentry))
            return material->bTexturesRegistered && material->texture ? material->texture[0] : TR_Invalid;
    }
    return TR_Invalid;
}
static materialentry *paintOriginalMaterial(materialentry *material)
{
    PaintShip *ship=paintFind(paintOwner);PaintMesh *mesh;
    if(ship)for(mesh=ship->meshes;mesh;mesh=mesh->next) {
        uintptr_t value=(uintptr_t)material,begin=(uintptr_t)mesh->materials;
        if(value>=begin && value<begin+(size_t)mesh->count*sizeof(materialentry))
            return &mesh->mesh->localMaterial[(value-begin)/sizeof(materialentry)];
    }
    return material;
}
color shipAppearanceColor(const Ship *ship,sdword channel,color fallback)
{
    PaintShip *entry=paintFind(ship);
    return entry && channel>=0 && channel<SHIP_PAINT_COUNT && (entry->appearance.mask&(1u<<channel)) ? entry->appearance.colors[channel] : fallback;
}
bool32 shipAppearanceHas(const Ship *ship,sdword channel)
{
    PaintShip *entry=paintFind(ship);
    return entry && channel>=0 && channel<SHIP_PAINT_COUNT && (entry->appearance.mask&(1u<<channel)) ? TRUE : FALSE;
}
void shipAppearanceGet(const Ship *ship,ShipAppearance *out)
{
    sdword scheme=ship?ship->colorScheme:0;
    PaintShip *entry=paintFind(ship);
    if(scheme<0 || scheme>=MAX_MULTIPLAYER_PLAYERS) scheme=0;
    memset(out,0,sizeof(*out));
    out->colors[0]=teColorSchemes[scheme].textureColor.base;
    out->colors[1]=teColorSchemes[scheme].textureColor.detail;
    out->colors[2]=teColorSchemes[scheme].trailColors[0];
    out->colors[3]=utyNavLightColor;out->colors[4]=utyResourceBeamColor;out->colors[5]=utyHyperspaceColor;
    if(entry) {sdword i;out->mask=entry->appearance.mask;for(i=0;i<SHIP_PAINT_COUNT;++i)if(out->mask&(1u<<i))out->colors[i]=entry->appearance.colors[i];}
}
static void paintFreeMesh(PaintMesh *entry)
{
    sdword i;
#ifdef HW_ENABLE_D3D12_BACKEND
    meshdata temporary=*entry->mesh;
    temporary.localMaterial=entry->materials;
    meshForgetModernSurfaceTextures(&temporary);
    meshSurfaceCacheForgetMaterials(entry->materials);
    for(i=0;i<entry->count;++i) hwModernGraphicsUnregisterSurfaceTexture(&entry->materials[i]);
#endif
    for(i=0;i<entry->count;++i) if(entry->handles[i*MAX_MULTIPLAYER_PLAYERS]!=TR_Invalid)
        trTextureUnregister(entry->handles[i*MAX_MULTIPLAYER_PLAYERS]);
    free(entry->handles);free(entry->materials);free(entry);
}
static void paintRemoveEntry(PaintShip *entry)
{
    PaintShip **at=&paintActive;
    const Ship *owner=entry->owner;
    while(*at && *at!=entry)at=&(*at)->next;
    if(*at)*at=entry->next;
    if(paintShips[entry->number]==entry)paintShips[entry->number]=NULL;
    while(entry->meshes) {PaintMesh *next=entry->meshes->next;paintFreeMesh(entry->meshes);entry->meshes=next;}
    free(entry);
    if(paintOwner==owner)paintOwner=NULL;
}
void shipAppearanceRetire(const void *ship)
{
    PaintShip *entry=paintFind((const Ship *)ship);
    if(entry)paintRemoveEntry(entry);
}
void shipAppearanceClear(void)
{
    while(paintActive)paintRemoveEntry(paintActive);
    paintOwner=NULL;
}
static void paintForgetMesh(meshdata *mesh)
{
    PaintShip *ship;
    for(ship=paintActive;ship;ship=ship->next) {
        PaintMesh **at=&ship->meshes;
        while(*at) {PaintMesh *entry=*at;if(entry->mesh==mesh){*at=entry->next;paintFreeMesh(entry);}else at=&entry->next;}
    }
}
bool32 shipAppearanceApply(Ship *ship,const ShipAppearance *appearance)
{
    PaintShip *entry;
    if(!ship || ship->objtype!=OBJ_ShipType || (ship->flags&SOF_Dead) || !appearance || appearance->mask>=64) return FALSE;
    if(paintShips[ship->shipID.shipNumber])paintRemoveEntry(paintShips[ship->shipID.shipNumber]);
    if(!appearance->mask) return TRUE;
    entry=(PaintShip *)calloc(1,sizeof(*entry));if(!entry)return FALSE;
    entry->owner=ship;entry->number=ship->shipID.shipNumber;entry->appearance=*appearance;paintShips[entry->number]=entry;
    paintGeneration=(paintGeneration+1)&0x3fffffu;
    if(!paintGeneration)paintGeneration=1;
    entry->variant=0x80000000u|(paintGeneration<<9);
    entry->next=paintActive;paintActive=entry;
    return TRUE;
}
static materialentry *meshAppearanceMaterials(meshdata *mesh,sdword scheme)
{
    PaintShip *ship=paintFind(paintOwner);
    PaintMesh *entry;
    trcolorinfo colors;
    sdword i,j;
    if(!ship || !(ship->appearance.mask&3)) return mesh->localMaterial;
    if(scheme<0 || scheme>=MAX_MULTIPLAYER_PLAYERS)scheme=0;
    colors=teColorSchemes[scheme].textureColor;
    colors.base=shipAppearanceColor(paintOwner,SHIP_PAINT_BASE,colors.base);
    colors.detail=shipAppearanceColor(paintOwner,SHIP_PAINT_STRIPE,colors.detail);
    for(entry=ship->meshes;entry;entry=entry->next) if(entry->mesh==mesh && entry->base==colors.base && entry->detail==colors.detail) return entry->materials;
    {PaintMesh **at=&ship->meshes;while(*at){PaintMesh *old=*at;if(old->mesh==mesh){*at=old->next;paintFreeMesh(old);}else at=&old->next;}}
    entry=(PaintMesh *)calloc(1,sizeof(*entry));if(!entry)return mesh->localMaterial;
    entry->count=mesh->nLocalMaterials+mesh->nPublicMaterials;entry->mesh=mesh;entry->base=colors.base;entry->detail=colors.detail;
    entry->materials=(materialentry *)calloc(entry->count,sizeof(materialentry));
    entry->handles=(trhandle *)malloc(entry->count*MAX_MULTIPLAYER_PLAYERS*sizeof(trhandle));
    if(!entry->materials || !entry->handles){free(entry->materials);free(entry->handles);free(entry);return mesh->localMaterial;}
    memcpy(entry->materials,mesh->localMaterial,entry->count*sizeof(materialentry));
    for(i=0;i<entry->count*MAX_MULTIPLAYER_PLAYERS;++i)entry->handles[i]=TR_Invalid;
    for(i=0;i<entry->count;++i) {
        materialentry *material=&entry->materials[i];
        trhandle handle=TR_Invalid;
        if(material->bTexturesRegistered && material->texture) {
            trhandle original=material->texture[scheme];
            if(original==TR_Invalid)for(j=0;j<MAX_MULTIPLAYER_PLAYERS;++j)if(material->texture[j]!=TR_Invalid){original=material->texture[j];break;}
            if(original!=TR_Invalid && trIndex(original)<TR_RegistrySize) {
                char path[PATH_MAX];
                snprintf(path,sizeof(path),"%s",trStructureGet(original)->fileName);
                handle=trTextureRegister(path,&colors,mesh);
            }
            for(j=0;j<MAX_MULTIPLAYER_PLAYERS;++j)entry->handles[i*MAX_MULTIPLAYER_PLAYERS+j]=handle;
            material->texture=&entry->handles[i*MAX_MULTIPLAYER_PLAYERS];
        }
        if(material->flags&MDF_BaseColor) {material->ambient=material->diffuse=colors.base;material->flags&=~MDF_BaseColor;}
        if(material->flags&MDF_StripeColor) {material->ambient=material->diffuse=colors.detail;material->flags&=~MDF_StripeColor;}
    }
    trRegistryRefresh();
    entry->next=ship->meshes;ship->meshes=entry;
#ifdef HW_ENABLE_D3D12_BACKEND
    for(i=0;i<entry->count;++i) {
        char path[PATH_MAX];materialentry *material=&entry->materials[i];
        meshRegisterModernEmissiveMaterial(mesh->fileName,material);
        if(material->textureNameSave) {meshTextureNameToPath(path,mesh->fileName,material->textureNameSave);meshQueueModernSurfaceTexture(material,path);}
        else meshQueueModernSurfaceTexture(material,NULL);
    }
#endif
    return entry->materials;
}
