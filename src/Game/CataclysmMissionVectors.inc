/* Original 00498f30 / 004991d0 and 00419ed0. The shared standoff sampler
   was checked against the recovered implementation: three AI-stream draws
   in [-10,10], normalization, radius and center; normal hemisphere mode. */
static void cataScriptClampVector(hvector *out,real32 boundScale)
{
    real32 x=smUniverseSizeX*boundScale,y=smUniverseSizeY*boundScale,z=smUniverseSizeZ*boundScale;
    out->x=max(-x,min(x,out->x));out->y=max(-y,min(y,out->y));out->z=max(-z,min(z,out->z));
}
static void cataScriptVectorBlend(hvector *out,const hvector *from,const hvector *to,sdword position,sdword total)
{
    real32 a,b;hvector f=*from,t=*to;
    if(total==0)cataScriptFault("Zero Cataclysm vector interpolation divisor");
    a=(real32)(total-position)/(real32)total;b=(real32)position/(real32)total;
    out->x=a*f.x+b*t.x;out->y=a*f.y+b*t.y;out->z=a*f.z+b*t.z;
    cataScriptClampVector(out,1.0f);
}
static void cataScriptVectorRandom(hvector *out,const hvector *center,sdword radius,real32 boundScale)
{
    vector origin={0,0,0},location=aiuGenerateRandomStandoffPoint(*(const vector *)center,(real32)radius,origin,RSP_NORMAL);
    out->x=location.x;out->y=location.y;out->z=location.z;out->w=1.0f;cataScriptClampVector(out,boundScale);
}
