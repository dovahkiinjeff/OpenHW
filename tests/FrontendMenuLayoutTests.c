#include "FEFlow.h"
#include "uiscale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MIN2(a,b) ((a)<(b)?(a):(b))
#define MAX2(a,b) ((a)>(b)?(a):(b))
#define MUI_COLOR_TAB_COUNT 6
#define MUI_GAMEPLAY_ROW_COUNT 13
static sdword MAIN_WindowWidth,MAIN_WindowHeight,checks;
typedef struct ModernUILayout {
    rectangle canvas,rail,content,preview,tabs[6],rows[13],cancel,apply;
} ModernUILayout;
static rectangle muiRect(sdword x0,sdword y0,sdword x1,sdword y1)
{rectangle r={x0,y0,x1,y1};return r;}
#include "../src/Game/FrontendMenuLayout.inc"
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line %d, %dx%d scale %g: %s\n",__LINE__,MAIN_WindowWidth,MAIN_WindowHeight,(double)uiScaleGet(),#x);exit(1);}}while(0)
static void within(rectangle r,rectangle p)
{CHECK(r.x1>r.x0 && r.y1>r.y0);CHECK(r.x0>=p.x0 && r.y0>=p.y0 && r.x1<=p.x1 && r.y1<=p.y1);}
static int overlaps(rectangle a,rectangle b)
{return a.x0<b.x1 && b.x0<a.x1 && a.y0<b.y1 && b.y0<a.y1;}
int main(void)
{
    static const int sizes[][2]={{1280,720},{1920,1080},{2560,1440},{3440,1440},{3840,2160}};
    unsigned n;int percent,side,i,j;
    for(n=0;n<sizeof(sizes)/sizeof(*sizes);++n)for(percent=75;percent<=150;percent+=25) {
        MuiStoryLayout story;ModernUILayout colors;rectangle screen;
        MAIN_WindowWidth=sizes[n][0];MAIN_WindowHeight=sizes[n][1];
        uiScaleConfigure(MAIN_WindowWidth,MAIN_WindowHeight,percent);screen=muiRect(0,0,MAIN_WindowWidth,MAIN_WindowHeight);
        for(side=0;side<2;++side) {
            rectangle panel=muiFrontendPanel(side);within(panel,screen);
            for(i=0;i<6;++i) {
                rectangle item=muiFrontendItem(side,i,6);within(item,panel);
                for(j=0;j<i;++j)CHECK(!overlaps(item,muiFrontendItem(side,j,6)));
            }
        }
        muiBuildStoryLayout(&story);within(story.panel,screen);
        CHECK(abs(story.panel.x0-(MAIN_WindowWidth-story.panel.x1))<=1);
        CHECK(abs(story.panel.y0-(MAIN_WindowHeight-story.panel.y1))<=1);
        {rectangle items[]={story.preview,story.races[0],story.races[1],story.colors,story.load,story.back,story.start};
         for(i=0;i<7;++i){within(items[i],story.panel);for(j=0;j<i;++j)CHECK(!overlaps(items[i],items[j]));}}
        within(story.thrust,story.preview);
        muiBuildFleetColorLayout(&colors);within(colors.rail,screen);within(colors.content,screen);within(colors.preview,screen);
        CHECK(!overlaps(colors.content,colors.preview) && !overlaps(colors.rail,colors.content));
        CHECK(colors.preview.x1-colors.preview.x0>=MAIN_WindowWidth/3);
        within(colors.apply,colors.content);within(colors.cancel,colors.content);
        CHECK(!overlaps(colors.apply,colors.cancel));
        for(i=0;i<6;++i)within(colors.tabs[i],colors.rail);
    }
    printf("PASS: %d layout bounds and separation checks at five resolutions and four UI scales.\n",checks);
    return 0;
}
