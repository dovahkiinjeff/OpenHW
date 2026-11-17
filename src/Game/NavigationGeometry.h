#ifndef OPENHW_NAVIGATION_GEOMETRY_H
#define OPENHW_NAVIGATION_GEOMETRY_H
#include "NavigationOrders.h"
#include <math.h>
#include <string.h>
/* Three 800-unit frigate parade intervals per leg: a regular hexagonal
   track is at least 4800 units across (six parade intervals). */
#define NAV_MIN_TURN_RADIUS 1200.0f
static real32 navGeometryRadius(const SelectCommand *ships)
{
    real32 radius=NAV_MIN_TURN_RADIUS;sdword i;
    if(ships)for(i=0;i<ships->numShips;i++) {
        const ShipStaticInfo *s=ships->ShipPtr[i]->staticinfo;real32 hull,turn;
        if(!s)continue;
        hull=s->staticheader.staticCollInfo.avoidcollspheresize;
        if(isfinite(hull))radius=max(radius,3.0f*hull);
        if(isfinite(s->staticheader.maxvelocity) && isfinite(s->staticheader.maxrot) && s->staticheader.maxrot>0.001f) {
            turn=s->staticheader.maxvelocity/s->staticheader.maxrot;
            if(isfinite(turn))radius=max(radius,turn);
        }
    }
    return radius;
}
/* bad[i] refers to the authored edge i -> i+1 (wrapping for loops).
   A circular turn needs radius*tan(deflection/2) on each adjoining leg.
   Adjacent corners must fit on their shared leg, not just independently. */
static bool32 navGeometryCheck(const vector *points,unsigned count,bool32 loop,real32 radius,ubyte *bad)
{
    real32 length[NAV_MAX_POINTS]={0},cut[NAV_MAX_POINTS]={0};
    unsigned edges,i;bool32 valid=TRUE;
    if(bad)memset(bad,0,NAV_MAX_POINTS);
    if(!points || !count || count>NAV_MAX_POINTS || !isfinite(radius) || radius<=0)return FALSE;
    if(loop && count<3){if(bad)memset(bad,1,count);return FALSE;}
    edges=loop?count:count-1;
    for(i=0;i<edges;i++) {
        vector delta;vecSub(delta,points[(i+1)%count],points[i]);length[i]=sqrtf(vecMagnitudeSquared(delta));
        if(!isfinite(length[i]) || length[i]<2*radius){valid=FALSE;if(bad)bad[i]=1;}
    }
    for(i=loop?0:1;i<(loop?count:count-1);i++) {
        unsigned prev=(i+count-1)%count;vector a,b;real32 cosine;
        vecSub(a,points[i],points[prev]);vecSub(b,points[(i+1)%count],points[i]);
        if(length[prev]<=0 || length[i]<=0){cut[i]=1e30f;continue;}
        cosine=(a.x*b.x+a.y*b.y+a.z*b.z)/(length[prev]*length[i]);
        cosine=max(-1.0f,min(1.0f,cosine));
        cut[i]=cosine<=-0.9999f?1e30f:radius*sqrtf((1-cosine)/(1+cosine));
    }
    for(i=0;i<edges;i++)if(cut[i]+cut[(i+1)%count]>length[i]) {
        valid=FALSE;if(bad){bad[i]=1;bad[(i+count-1)%count]=1;if(i+1<edges || loop)bad[(i+1)%count]=1;}
    }
    return valid;
}
#endif
