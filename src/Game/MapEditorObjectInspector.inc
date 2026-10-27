/* Every displayed property owns its adjustment; gizmo mode/axis is separate. */
static bool32 mmeAdjustObjectInspector(MMETrackedObject *track,sdword row,sdword direction,
                                       real32 fine,real32 coarse)
{
    if(track==NULL || track->object==NULL || row<0 || row>6)return FALSE;
    if(row<3)
    {
        vector *position=mmeObjectPosition(track->object);
        real32 step=100.0f*coarse*fine*(real32)direction;
        if(position==NULL)return FALSE;
        if(row==0)position->x+=step;
        else if(row==1)position->y+=step;
        else position->z+=step;
        mmeStopObjectMotion(track->object);
        univUpdateObjRotInfo((SpaceObjRot *)track->object);
    }
    else if(row<6)mmeRotateTrack(track,row-3,5.0f*fine*(real32)direction);
    else
    {
        real32 scale;
        if(track->objtype!=OBJ_DerelictType && track->objtype!=OBJ_AsteroidType &&
           track->objtype!=OBJ_DustType && track->objtype!=OBJ_GasType &&
           track->objtype!=OBJ_NebulaType)return FALSE;
        scale=mmeClampF(mmeObjectScale(track->object)+.1f*fine*(real32)direction,.05f,50.0f);
        mmeSetObjectScale(track->object,scale);track->scale=scale;
    }
    track->dirty=TRUE;
    mmeStatus("OBJECT PROPERTY LIVE / UNSAVED");
    return TRUE;
}
