#include "CataclysmNeededFlags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {uint32_t input[25],output[22];unsigned next,applied;} Stream;
static uint32_t readFlag(void *context){Stream *s=context;CHECK(s->next<25);return s->input[s->next++];}
static void applyFlag(void *context,unsigned index,uint32_t flag){Stream *s=context;CHECK(index==s->applied++);s->output[index]=flag;}
int main(void)
{
    Stream stream={0};unsigned i;const uint32_t delimiter=0xf1a5deed;
    for(i=0;i<19;i++)stream.input[i]=0x0301+i*0x100;
    stream.input[19]=delimiter;stream.input[20]=0xfeed1234;
    CHECK(hwCataReadNeededFlags(21,delimiter,readFlag,applyFlag,&stream)==19);
    CHECK(stream.applied==19&&stream.next==20&&readFlag(&stream)==0xfeed1234);
    CHECK(stream.output[0]==0x0301&&stream.output[18]==0x1501&&stream.output[19]==0);
    memset(&stream,0,sizeof(stream));for(i=0;i<21;i++)stream.input[i]=0xff01;
    stream.input[21]=delimiter;stream.input[22]=0xfeed1234;
    CHECK(hwCataReadNeededFlags(21,delimiter,readFlag,applyFlag,&stream)==21);
    CHECK(stream.applied==21&&stream.next==22&&readFlag(&stream)==0xfeed1234);
    memset(&stream,0,sizeof(stream));stream.input[0]=delimiter;
    CHECK(hwCataReadNeededFlags(21,delimiter,readFlag,applyFlag,&stream)==0&&stream.applied==0&&stream.next==1);
    memset(&stream,0,sizeof(stream));
    CHECK(hwCataReadNeededFlags(21,delimiter,readFlag,applyFlag,&stream)==-1&&stream.next==22);
    printf("PASS: %d appended Cataclysm hull/load-stream compatibility checks\n",checks);return 0;
}
