/* A single menu model owns its palette entries, including engine-on pages.
   Nothing here edits team schemes or refreshes the global texture registry. */
meshdata *meshLoadMenuPreview(char *path,color base,color stripe)
{
    meshdata *mesh;
    bool32 saved=meshLoadingMenuPreview;
    trcolorinfo savedPaint=meshMenuPreviewPaint;
    meshLoadingMenuPreview=TRUE;
    meshMenuPreviewPaint.base=base;meshMenuPreviewPaint.detail=stripe;
    mesh=meshLoad(path);
    meshLoadingMenuPreview=saved;meshMenuPreviewPaint=savedPaint;
    if(mesh)meshMenuPreviewColors(mesh,base,stripe);
    return mesh;
}

static void meshMenuMaterialColors(materialentry *material,color base,color stripe)
{
    trcolorinfo colors={base,stripe};
    if(material->bTexturesRegistered && material->texture && material->texture[0]!=TR_Invalid) {
        trTextureColorsUpdate(material->texture[0],&colors);
        trPreparePreviewTexture(material->texture[0]);
    }
    if(material->flags&MDF_BaseColor)material->ambient=material->diffuse=base;
    if(material->flags&MDF_StripeColor)material->ambient=material->diffuse=stripe;
}

void meshMenuPreviewColors(meshdata *mesh,color base,color stripe)
{
    sdword i;unsigned bucket;
    if(!mesh)return;
    for(i=0;i<mesh->nLocalMaterials+mesh->nPublicMaterials;++i)
        meshMenuMaterialColors(&mesh->localMaterial[i],base,stripe);
    for(bucket=0;bucket<256;++bucket) {
        meshEngineMaterialPair *pair;
        for(pair=meshEngineMaterials[bucket];pair;pair=pair->next)
            if(pair->mesh==mesh)meshMenuMaterialColors(&pair->active,base,stripe);
    }
}

void meshRenderMenuPreview(meshdata *mesh,real32 engineBlend)
{
    uword flags[256];sdword i,count=mesh->nLocalMaterials+mesh->nPublicMaterials;
    real32 savedBlend=meshEngineBlend;
    bool32 savedBadgePreview=meshBadgePreview;meshBadgePreview=TRUE;
    const Ship *savedPaint=paintOwner;
#ifdef HW_ENABLE_D3D12_BACKEND
    uint64_t savedMotion=meshMotionOwner;
    meshMotionOwner=0;meshBadgeSetMotionOwner(0);
#endif
    /* Do not assign the synthetic preview ship a live fleet identity. */
    paintSetOwner(NULL);meshEngineBlend=engineBlend;
    if(count<=256)for(i=0;i<count;++i) {
        flags[i]=mesh->localMaterial[i].flags;
        mesh->localMaterial[i].flags&=~(MDF_BaseColor|MDF_StripeColor);
    }
    meshRender(mesh,0);
    if(count<=256)for(i=0;i<count;++i)mesh->localMaterial[i].flags=flags[i];
    meshEngineBlend=savedBlend;meshBadgePreview=savedBadgePreview;paintSetOwner(savedPaint);
#ifdef HW_ENABLE_D3D12_BACKEND
    meshMotionOwner=savedMotion;meshBadgeSetMotionOwner(savedMotion);hwglCaptureEngineIdle(-1.0f);
#endif
}

void meshFreeMenuPreview(meshdata *mesh)
{
    trhandle *handles;
    sdword count=0,i,capacity;
    unsigned bucket;
    if(!mesh)return;
    capacity=(mesh->nLocalMaterials+mesh->nPublicMaterials)*2;
    handles=capacity?(trhandle *)malloc(capacity*sizeof(trhandle)):NULL;
    for(i=0;handles && i<mesh->nLocalMaterials+mesh->nPublicMaterials;++i) {
        materialentry *material=&mesh->localMaterial[i];
        if(material->bTexturesRegistered && material->texture && material->texture[0]!=TR_Invalid)
            handles[count++]=material->texture[0];
    }
    for(bucket=0;handles && bucket<256;++bucket) {
        meshEngineMaterialPair *pair;
        for(pair=meshEngineMaterials[bucket];pair;pair=pair->next)
            if(pair->mesh==mesh && pair->handles[0]!=TR_Invalid && count<capacity)
                handles[count++]=pair->handles[0];
    }
    meshFree(mesh);
    for(i=0;i<count;++i)trTextureReleasePreview(handles[i]);
    free(handles);
}
