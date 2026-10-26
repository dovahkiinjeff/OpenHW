/* Inspect the actual connected event bank, not an inferred source filename. */
#include "RemasteredPresentationEvents.h"
static void speEventsOpen(void){
 char root[256],path[512],ownerName[64]="Hull",error[256],*text=NULL,*row,*report;void*data=NULL;size_t used=0;sdword size;int owner=speSubsystemOwner(spe.selected);unsigned i;HwRmPresentationEvents*events;
 if(speFile.dirty){speStatus("SAVE OR DISCARD SHIP FILE CHANGES BEFORE OPENING EVENTS");return;}
 speFileOpen();
 snprintf(root,sizeof(root),"%s/%s/RMNative/",ShipRaceToStr(spe.doc.race),ShipTypeToStr(spe.doc.type));
 if(owner>=0){
  snprintf(ownerName,sizeof(ownerName),"%s",spe.doc.points[owner].name);
  snprintf(root,sizeof(root),"Subsystems/%s/",spe.doc.points[owner].subsystem);
  snprintf(path,sizeof(path),"%ssubsystem.owsubsystem",root);
  if(!fileExists(path,0)||fileSizeGet(path,0)>SHIP_TEXT_LIMIT){speStatus("SUBSYSTEM DEFINITION NOT FOUND");return;}
  size=fileLoadAlloc(path,&data,0);if(size<=0)return;text=malloc(size+1);if(text){memcpy(text,data,size);text[size]=0;}memFree(data);
 }else if(speFile.text.text)text=_strdup(speFile.text.text);
 if(!text)return;path[0]=0;
 for(row=strtok(text,"\r\n");row;row=strtok(NULL,"\r\n")){char slot[64],relative[256];
  if(sscanf(row,"presentation %63s %255s",slot,relative)==2&&!strcmp(slot,"Hull")){
   snprintf(path,sizeof(path),"%s%s",!strncmp(relative,"@/",2)?"":root,!strncmp(relative,"@/",2)?relative+2:relative);break;
  }
 }free(text);
 if(!path[0]){speStatus("NO EVENT FILE IS CONNECTED TO THIS COMPONENT");return;}
 if(!fileExists(path,0)||fileSizeGet(path,0)>16*1024*1024){speStatus("CONNECTED EVENT FILE NOT FOUND OR TOO LARGE");return;}
 size=fileLoadAlloc(path,&data,0);if(size<=0)return;
 events=hwRmPresentationEventsRead(data,size,error,sizeof(error));memFree(data);
 if(!events){speStatus(error);return;}
 report=malloc(SHIP_TEXT_LIMIT);if(!report){hwRmPresentationEventsFree(events);return;}
 used=snprintf(report,SHIP_TEXT_LIMIT,"Connected event file: %s\nComponent: %s\nRead-only decoded runtime data; this is not a new authoring format.\n\n",path,ownerName);
 for(i=0;i<hwRmPresentationTimelineCount(events)&&used<SHIP_TEXT_LIMIT-4096;i++){
  const HwRmPresentationTimeline*t=hwRmPresentationTimelineAt(events,i);
  used+=snprintf(report+used,SHIP_TEXT_LIMIT-used,"Timeline %u: %s\n  Parent: %d  Duration: %.9g  Loop: %u\n  Minimum: %.9g  Maximum: %.9g\n\n",i,t->name,t->parent,t->duration,t->loop,t->minimum,t->maximum);
 }
 for(i=0;i<hwRmPresentationEventCount(events)&&used<SHIP_TEXT_LIMIT-4096;i++){
  const HwRmPresentationEvent*e=hwRmPresentationEventAt(events,i);
  int n=snprintf(report+used,SHIP_TEXT_LIMIT-used,"Event %u / Timeline %u\n  Time: %.9g  Scale: %.9g  Flags: %u\n  Markers: %s\n  FX: %s\n  Sound: %s\n  Game event: %s\n  Orphan: %u (%s)  Transform: %u\n  Length mode: %u  Length: %.9g\n  Other: %u  Other marker: %s\n\n",i,e->timeline,e->time,e->scale,e->flags,e->markers,e->fx,e->sound,e->gameEvent,e->orphan,e->orphanText,e->transform,e->lengthMode,e->length,e->other,e->otherMarker);
  if(n<0||(size_t)n>=SHIP_TEXT_LIMIT-used){used=SHIP_TEXT_LIMIT-1;break;}used+=n;
 }
 hwRmPresentationEventsFree(events);
 if(speFileParse(report,used)){speEventView=1;speFile.ready=0;speFile.scroll=0;speFile.matches=0;snprintf(speFile.notice,sizeof(speFile.notice),"Connected events / read only / select and Ctrl+C to copy.");}
 free(report);
}
