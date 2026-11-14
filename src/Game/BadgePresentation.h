#ifndef OPENHW_BADGE_PRESENTATION_H
#define OPENHW_BADGE_PRESENTATION_H
typedef struct HWBadgeImageRect {int x0,y0,x1,y1;} HWBadgeImageRect;
/* Badge textures are transparent squares. Fit the entire square, never crop. */
static HWBadgeImageRect hwBadgeFitSquare(int x0,int y0,int x1,int y1)
{
    HWBadgeImageRect r;int w=x1-x0,h=y1-y0,side=w<h?w:h;
    if(side<0)side=0;
    r.x0=x0+(w-side)/2;r.y0=y0+(h-side)/2;
    r.x1=r.x0+side;r.y1=r.y0+side;return r;
}
#endif
