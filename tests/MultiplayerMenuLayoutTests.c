#include "FEFlow.h"
#include "uiscale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int MAIN_WindowWidth,MAIN_WindowHeight,checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
#include "../src/Game/MultiplayerMenuLayout.inc"
static void checkScreen(fescreen *screen)
{
    static const int sizes[][2]={{1280,720},{1920,1080},{2560,1440},{3440,1440},{3840,2160}};
    size_t n;int percent,i;
    for(n=0;n<sizeof(sizes)/sizeof(sizes[0]);++n)for(percent=75;percent<=150;percent+=25)
    {
        rectangle panel;
        MAIN_WindowWidth=sizes[n][0];MAIN_WindowHeight=sizes[n][1];
        uiScaleConfigure(MAIN_WindowWidth,MAIN_WindowHeight,percent);
        panel.x0=MAIN_WindowWidth*23/100;panel.x1=MAIN_WindowWidth-uiScaleSize(22);
        panel.y0=MAIN_WindowHeight/11;
        if(panel.y0<uiScaleSize(76))panel.y0=uiScaleSize(76);
        if(panel.y0>uiScaleSize(132))panel.y0=uiScaleSize(132);
        panel.y1=MAIN_WindowHeight-uiScaleSize(22);
        muiNetworkPrepare(screen,panel);
        for(i=1;i<screen->nAtoms;++i)
        {
            featom *a=&screen->atoms[i];
            if(bitTest(a->flags,FAF_Hidden))continue;
            if(!(a->x>=panel.x0 && a->y>=panel.y0 &&
                a->x+a->width<=panel.x1+1 && a->y+a->height<=panel.y1+1))
                fprintf(stderr,"%s atom %d (%s) at %d,%d %dx%d, output %dx%d UI %d%%\n",screen->name,i,a->name?a->name:"text",a->x,a->y,a->width,a->height,MAIN_WindowWidth,MAIN_WindowHeight,percent);
            CHECK(a->x>=panel.x0 && a->y>=panel.y0);
            CHECK(a->x+a->width<=panel.x1+1 && a->y+a->height<=panel.y1+1);
            CHECK(a->width>0 && a->height>0);
            if(a->name && strcmp(a->name,"MG_NumberOfComputers")==0)
            {
                int j,found=0;
                for(j=1;j<screen->nAtoms;++j)
                {
                    featom *button=&screen->atoms[j];
                    if(!button->name)continue;
                    if(strcmp(button->name,"MG_RemoveCPUOpponent")==0)
                    {
                        CHECK(button->y==a->y && button->height==a->height && button->width==a->width);
                        CHECK(button->x+button->width<a->x);++found;
                    }
                    if(strcmp(button->name,"MG_AddCPUOpponent")==0)
                    {
                        CHECK(button->y==a->y && button->height==a->height && button->width==a->width);
                        CHECK(button->x>a->x+a->width);++found;
                    }
                }
                CHECK(found==2);
            }
        }
    }
}
static unsigned int u32(const unsigned char *p){unsigned int v;memcpy(&v,p,4);return v;}
static unsigned short u16(const unsigned char *p){unsigned short v;memcpy(&v,p,2);return v;}
/* Optional local retail FIB exercises every actual page without bundling it. */
static void checkFib(const char *path)
{
    FILE *f=fopen(path,"rb");long length;unsigned char *bytes;int k,total=0;
    CHECK(f!=NULL);fseek(f,0,SEEK_END);length=ftell(f);rewind(f);
    bytes=malloc(length);CHECK(bytes!=NULL && fread(bytes,1,length,f)==(size_t)length);fclose(f);
    for(k=0;k<u16(bytes+10);++k)
    {
        const unsigned char *s=bytes+12+k*20;
        fescreen screen={0};int i;
        screen.name=(char *)bytes+u32(s);screen.nAtoms=u16(s+10);
        if(!muiNetworkPage(screen.name))continue;
        screen.atoms=calloc(screen.nAtoms,sizeof(featom));CHECK(screen.atoms!=NULL);
        for(i=0;i<screen.nAtoms;++i)
        {
            const unsigned char *disk=bytes+u32(s+16)+i*76;
            featom *a=&screen.atoms[i];
            a->name=u32(disk)?(char *)bytes+u32(disk):NULL;a->flags=u32(disk+4);a->type=disk[12];
            a->loadedX=(short)u16(disk+24);a->loadedY=(short)u16(disk+28);
            a->loadedWidth=(short)u16(disk+32)-a->loadedX;
            a->loadedHeight=(short)u16(disk+36)-a->loadedY;
            if(u32(disk+40) && a->type!=FA_RadioButton){a->type=FA_StaticText;a->pData=bytes+u32(disk+40);}
        }
        checkScreen(&screen);++total;free(screen.atoms);
    }
    free(bytes);CHECK(total>=30);printf("%d actual multiplayer pages checked at 5 resolutions and 4 UI scales.\n",total);
}
int main(int argc,char **argv)
{
    featom atoms[4]={0};fescreen screen={0};
    screen.name="LAN_Login";screen.nAtoms=4;screen.atoms=atoms;
    /* The decorative root extends far offscreen; useful controls must fit. */
    atoms[0].loadedY=1058;atoms[0].loadedWidth=640;atoms[0].loadedHeight=90;
    atoms[1].type=FA_TextEntry;atoms[1].name="MG_NameEntry";
    atoms[1].loadedX=238;atoms[1].loadedY=151;atoms[1].loadedWidth=214;atoms[1].loadedHeight=32;
    atoms[2].type=FA_Button;atoms[2].name="MG_LaunchLAN";
    atoms[2].loadedX=374;atoms[2].loadedY=264;atoms[2].loadedWidth=130;atoms[2].loadedHeight=32;
    atoms[3].type=FA_DecorativeRegion;atoms[3].loadedX=-200;atoms[3].loadedY=1000;atoms[3].loadedWidth=2000;atoms[3].loadedHeight=500;
    checkScreen(&screen);CHECK(bitTest(atoms[3].flags,FAF_Hidden));
    CHECK(muiNetworkPage("Skirmish_Colour") && muiNetworkPage("Resource_Options_View"));
    CHECK(!muiNetworkPage("Construction_manager"));
    if(argc>1)checkFib(argv[1]);
    printf("Multiplayer layout: %d checks passed.\n",checks);return 0;
}
