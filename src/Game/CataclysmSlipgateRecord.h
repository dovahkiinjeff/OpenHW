#ifndef OPENHW_CATACLYSM_SLIPGATE_RECORD_H
#define OPENHW_CATACLYSM_SLIPGATE_RECORD_H
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef struct CataSlipgateRecord {float x,y,z,yaw;int index,total;char source[64];} CataSlipgateRecord;
/* Original scene grammar: ,x,y,z,index/total,yaw,shiptype. */
static int cataParseSlipgateRecord(const char *field,CataSlipgateRecord *out)
{
    char text[512],trailing;size_t i;if(!field || !out || strlen(field)>=sizeof(text))return 0;
    strcpy(text,field);for(i=0;text[i];i++)if(text[i]==',')text[i]=' ';
    memset(out,0,sizeof(*out));
    if(sscanf(text,"%f %f %f %d/%d %f %63s %c",&out->x,&out->y,&out->z,&out->index,&out->total,&out->yaw,out->source,&trailing)!=7)return 0;
    return isfinite(out->x)&&isfinite(out->y)&&isfinite(out->z)&&isfinite(out->yaw)&&
        out->index>0&&out->total>0&&out->index<=out->total&&out->total<=65535&&
        !strcmp(out->source,"dSlipgateLarge");
}
#endif
