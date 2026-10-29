#include "ResourceScalingPolicy.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x);return 1; } } while(0)
int main(void)
{
    unsigned int count,flags,enabled,sp,step;
    const unsigned int modifiers[]={0,ATTRIBUTES_ScaleResources,
        ATTRIBUTES_ScaleResources|ATTRIBUTES_KillerCollDamage,
        ATTRIBUTES_ScaleResources|ATTRIBUTES_Regrow};
    for(count=0;count<=1000;count+=count<10?1:37)
        for(flags=0;flags<4;++flags)
            for(enabled=0;enabled<2;++enabled)
                for(sp=0;sp<2;++sp)
                    for(step=0;step<=100;++step) {
                        float strength=step/100.0f;
                        unsigned int expected=count;
                        if(sp && !enabled && (modifiers[flags]&ATTRIBUTES_ScaleResources))
                            expected=(unsigned int)((float)count*((modifiers[flags]&ATTRIBUTES_KillerCollDamage)?strength:1-strength));
                        CHECK(resourceCountForFleet(count,sp,modifiers[flags],strength,enabled,101)==expected);
                        CHECK(resourceCountForFleet(count,sp,modifiers[flags],strength,enabled,0)==
                            resourceCountForFleet(count,sp,modifiers[flags],strength,0,101));
                        CHECK(resourceCountForFleet(count,sp,modifiers[flags],strength,enabled,100)==count);
                        {
                            unsigned int vanilla=resourceCountForFleet(count,sp,modifiers[flags],strength,0,101);
                            unsigned int mid=resourceCountForFleet(count,sp,modifiers[flags],strength,enabled,50);
                            CHECK(mid>=vanilla && mid<=count);
                            CHECK(mid==vanilla+(count-vanilla)/2);
                        }
                    }
    /* Mission 11: two groups of 27 DustCloud1 resources. A full fleet used
       to eliminate both; enabling the option must preserve all 54. */
    CHECK(2*resourceCountForFleet(27,1,16384,1,0,0)==0);
    CHECK(2*resourceCountForFleet(27,1,16384,1,1,101)==54);
    CHECK(resourceCountForFleet(27,1,16384,.5f,0,0)==13);
    CHECK(resourceCountForFleet(27,1,16384,.5f,0,50)==20);
    CHECK(resourceCountForFleet(27,1,16384,.5f,0,100)==27);
    CHECK(resourceCountForFleet(27,0,16384,1,0,50)==27);
    CHECK(resourceScalingPercent(101,1)==100 && resourceScalingPercent(101,0)==0);
    CHECK(resourceScalingPercent(1,1)==1 && resourceScalingPercent(50,1)==50);
    puts("PASS: full authored SP resource counts, legacy scaling parity, Mission 11 dust and multiplayer isolation");
    return 0;
}
