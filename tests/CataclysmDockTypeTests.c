#include "ObjTypes.h"
#include "StringSupport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Unused nice-name conversion in the linked ObjTypes translation unit. */
char *MessageStrings[NumStrings];
static int checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void)
{
    /* Keep the installed HW1 save/geometry identities unchanged. */
    CHECK(DockingCone==0 && LatchPoint==1 && LaunchPoint==2);
    CHECK(StrToDockPointType("DockingCone")==DockingCone);
    CHECK(StrToDockPointType("LatchPoint")==LatchPoint);
    CHECK(StrToDockPointType("LaunchPoint")==LaunchPoint);
    CHECK(StrToDockPointType("dpt_salvagelatch")==DPT_SalvageLatch);
    CHECK(StrToDockPointType("not_a_dock_type")==-1);
#define X(name) CHECK(name>LaunchPoint);CHECK(StrToDockPointType(#name)==name);CHECK(!strcmp(DockPointTypeToStr(name),#name));
#include "CataclysmDockTypes.inc"
#undef X
    CHECK(DPT_Salvage!=DPT_SalvageLatch && DPT_SalvageLatch!=DPT_SalvageProcess);
    CHECK(DPT_Med_In!=DPT_Med_In_Engineering);
    CHECK(CataMessagePod>CATA_LAST_COMPONENT);
    CHECK(StrToShipType("dMessagePod")==CataMessagePod);
    CHECK(!strcmp(ShipTypeToStr(CataMessagePod),"dMessagePod"));
    CHECK(StrToShipRace("RACE_DERELICT")==R1);
    CHECK(StrToShipType("dSlipgateLarge")==CataSlipgateLarge);
    printf("PASS: %d Cataclysm docking type checks\n",checks);return 0;
}
