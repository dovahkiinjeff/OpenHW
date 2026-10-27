/* Visible rows are views over stable catalog/track indices. */
static sdword mmeAssetRowAt(sdword row)
{
    sdword i;
    if (row == 0)
    {
        for(i=0;i<mmeAssetCount;++i)
            if(mmeAssets[i].kind==MME_ASSET_RTX_DUST_VOLUME)return i;
        return -1;
    }
    if(row==1)return -2; /* derelict dropdown */
    row-=2;
    for(i=0;i<mmeAssetCount;++i)
    {
        bool32 show=mme.derelictDropdownOpen ? mmeAssets[i].kind==MME_ASSET_DERELICT :
            mmeAssets[i].kind!=MME_ASSET_DERELICT && mmeAssets[i].kind!=MME_ASSET_RTX_DUST_VOLUME;
        if(show && row--==0)return i;
    }
    return -1;
}
static sdword mmeAssetRowCount(void)
{
    sdword count=2,i;
    for(i=0;i<mmeAssetCount;++i)
        if(mme.derelictDropdownOpen ? mmeAssets[i].kind==MME_ASSET_DERELICT :
           mmeAssets[i].kind!=MME_ASSET_DERELICT && mmeAssets[i].kind!=MME_ASSET_RTX_DUST_VOLUME)++count;
    return count;
}
static sdword mmeObjectRowAt(sdword row,bool32 placed)
{
    sdword i;
    for(i=0;i<mmeTrackedCount;++i)
        if((mmeTracked[i].created!=0)==(placed!=0) && (!placed || !mmeTracked[i].deleted))
            if(row--==0)return i;
    return -1;
}
static sdword mmeObjectRowCount(bool32 placed)
{
    sdword i,count=0;
    for(i=0;i<mmeTrackedCount;++i)
        if((mmeTracked[i].created!=0)==(placed!=0) && (!placed || !mmeTracked[i].deleted))++count;
    return count;
}
