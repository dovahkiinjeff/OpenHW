#ifndef HW_SUPPORT_DOCK_BINDING_H
#define HW_SUPPORT_DOCK_BINDING_H
#include <ctype.h>
#include <stdio.h>
#include <string.h>
/* Explicit slot identities. RM's Kushan fighters run aft-to-forward; the
   Taiidan paths alternate sides, whereas Classic enumerates each side. */
static int supportDockPathName(int taiidan,const char *classic,char native[16])
{
    char name[24];size_t i,n;
    if(!classic || (n=strlen(classic))>=sizeof(name)) return 0;
    for(i=0;i<=n;++i) name[i]=(char)toupper((unsigned char)classic[i]);
    if(n==5 && !strncmp(name,"CORV",4) && name[4]>='0' && name[4]<='3') {
        sprintf(native,"Corv%c",name[4]);return 1;
    }
    if(!taiidan && n==7 && !strncmp(name,"FIGHT",5) &&
       (name[5]=='L'||name[5]=='R') && name[6]>='0' && name[6]<='4') {
        sprintf(native,"Figh%c%d",name[5],4-(name[6]-'0'));return 1;
    }
    if(taiidan && n==6 && !strncmp(name,"FIGHT",5) && name[5]>='0' && name[5]<='9') {
        int slot=name[5]-'0';
        sprintf(native,"Fight%d",slot<5?slot*2+1:(slot-5)*2+2);return 1;
    }
    return 0;
}
#endif
