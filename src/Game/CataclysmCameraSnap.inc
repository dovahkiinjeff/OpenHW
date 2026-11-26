/* Owned 0042e140/0042e180 and the snap branch of 0042d510. The byte at
   CameraCommand+0x69 is a one-shot pose snap, NOT Camera.ignoreZoom (+0x68).
   Keep it out of the host's saved Camera/CameraCommand ABI. */
void cataCampaignCameraRequestSnap(real32 distance)
{
    sdword bits;memcpy(&bits,&distance,sizeof(bits));
    if(distance!=0)savedSet("CataCameraSnapDistance",bits);
    savedSet("CataCameraSnapPending",1);
}
bool32 cataCampaignCameraConsumeSnap(CameraCommand *command)
{
    AIVar *pending,*distanceVar;sdword bits;real32 distance;Camera *desired;
    if(command!=&universe.mainCameraCommand || !singlePlayerGame ||
       !hwIsCataclysmMission(spGetCurrentMission()))return FALSE;
    pending=aivarFind("CataCameraSnapPending");
    if(!pending || !aivarValueGet(pending) || !command->currentCameraStack)return FALSE;
    aivarValueSet(pending,0);desired=&command->currentCameraStack->remembercam;
    distanceVar=aivarFind("CataCameraSnapDistance");bits=distanceVar?aivarValueGet(distanceVar):0;
    memcpy(&distance,&bits,sizeof(distance));
    command->actualcamera.lookatpoint=desired->lookatpoint;
    command->actualcamera.angle=desired->angle;command->actualcamera.declination=desired->declination;
    if(distance>0){desired->distance=distance;command->zoomInCloseAsPossible=FALSE;aivarValueSet(distanceVar,0);}
    return TRUE;
}
