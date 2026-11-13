/* Use the same masks, hue scalars and paint shading as the legacy palette
   upload. Only an edited ship's modern albedo calls this; authored emission,
   normal and material pages remain untouched. No GPU readback is involved. */
bool32 trColorizeSurfaceRgba(trhandle handle,const lifheader *lif,const char *looseName,
    color *pixels,sdword width,sdword height)
{
    texreg *reg;trcolorinfo colors;ubyte *effect0,*effect1,*shade=NULL;
    real32 scalar0=1.0f,scalar1=1.0f,baseScalar,stripeScalar;
    const char *separator;sdword race=-1;
    if(handle==TR_Invalid || trIndex(handle)>=TR_RegistrySize || !lif || !pixels ||
       width<=0 || height<=0 || (size_t)width*height>16777216u)return FALSE;
    reg=trStructureGet(handle);
    if(trPaletteIndex(handle)>=(udword)reg->nPalettes || trPending(trIndex(handle)))return FALSE;
    if(!(lif->flags&(TRF_TeamColor0|TRF_TeamColor1)))return TRUE;
    colors=trColorInfosPointer((sdword)trIndex(handle))[trPaletteIndex(handle)];
    baseScalar=colUdwordToReal(reg->baseScalar);stripeScalar=colUdwordToReal(reg->stripeScalar);
    separator=strpbrk(reg->fileName,"\\/");
    if(separator && (baseScalar!=0.0f || stripeScalar!=0.0f)) {
        char name[64];size_t length=(size_t)(separator-reg->fileName);
        if(length<sizeof(name)){memcpy(name,reg->fileName,length);name[length]=0;race=StrToShipRace(name);}
        if(race!=-1) {
            cpTeamEffectScalars(&scalar0,&scalar1,colors.base,colors.detail,race);
            scalar0=(scalar0-1.0f)*baseScalar+1.0f;scalar1=(scalar1-1.0f)*stripeScalar+1.0f;
        }
    }
    effect0=trModernLoosePngEffectMap(lif,looseName?looseName:"",width,height,0);
    effect1=trModernLoosePngEffectMap(lif,looseName?looseName:"",width,height,1);
    if(!effect0 || !effect1){if(effect0)memFree(effect0);if(effect1)memFree(effect1);return FALSE;}
    trBufferColorRGB(pixels,pixels,effect0,effect1,colors.base,colors.detail,
        width*height,lif->flags,scalar0,scalar1);
    memFree(effect0);memFree(effect1);
    if(looseName) {
        char name[PATH_MAX],*extension;snprintf(name,sizeof(name),"%s",looseName);
        extension=strrchr(name,'.');
        if(extension) {
            snprintf(extension,sizeof(name)-(size_t)(extension-name),"_paintShade.dds");
            trModernLoadLooseGrayPng(name,width,height,&shade);
        }
        if(shade) {
            sdword i;for(i=0;i<width*height;++i) {
                color c=pixels[i];udword s=shade[i];pixels[i]=colRGBA(
                    (colRed(c)*s+127)/255,(colGreen(c)*s+127)/255,(colBlue(c)*s+127)/255,colAlpha(c));
            }
            memFree(shade);
        }
    }
    return TRUE;
}
