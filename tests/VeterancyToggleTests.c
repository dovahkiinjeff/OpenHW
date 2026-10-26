#include "../src/Game/Veterancy.c"
Universe universe;
udword opRankSystem=1;
void clCommandMessage(char message[MAX_MESSAGE_LENGTH]) { (void)message; }
char *ShipTypeToNiceStr(ShipType type) { (void)type; return "Test ship"; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x); return 1; } } while(0)
int main(void)
{
    Ship attacker={0},victim={0}; ShipStaticInfo fighter={0}; Player players[2]={0};
    fighter.shipclass=CLASS_Fighter;
    attacker.staticinfo=&fighter; victim.staticinfo=&fighter;
    attacker.playerowner=&players[0]; victim.playerowner=&players[1];
    veterancyInitialize(&attacker);
    CHECK(veterancyRankForScore(CLASS_Fighter,0)==0);
    CHECK(veterancyRankForScore(CLASS_Corvette,2)==1);
    CHECK(veterancyRankForScore(CLASS_Frigate,3)==1);
    CHECK(veterancyRankForScore(CLASS_Destroyer,5)==1);
    CHECK(veterancyRankForScore(CLASS_Carrier,10)==1);
    veterancyCreditKill(&attacker,&victim);
    CHECK(attacker.veterancyRank==1 && veterancyFactor(&attacker)==1.1f);
    opRankSystem=0;
    veterancyCreditKill(&attacker,&victim);
    CHECK(veterancyScore(&attacker)==1 && attacker.veterancyRank==1);
    CHECK(veterancyFactor(&attacker)==1.0f);
    opRankSystem=1;
    CHECK(veterancyFactor(&attacker)==1.1f);
    veterancyCreditKill(&attacker,&victim);
    CHECK(attacker.veterancyRank==2 && veterancyFactor(&attacker)==1.2f);
    puts("PASS: rank toggle gates progression and bonuses without erasing earned ranks");
    return 0;
}
