#include "../src/Game/EngineTrailAnchor.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
static void near(float a,float b){if(std::fabs(a-b)>0.0001f){std::fprintf(stderr,"%f != %f\n",a,b);std::exit(1);}}
int main()
{
    const float radius=engineGlowRadius(10,-1,1,1,.82f,.2f);
    near(radius,8.56f);
    const float offset=engineGlowOriginOffset(100,8,radius);
    near(offset,108.0528f);
    /* A large authored sprite offset must move every ribbon sample, rather
       than leaving a visible line between the hull and the glow bulb. */
    float nozzle[3]={12,-9,-5},head[3]={12,-9,-5},history[3]={12,-9,-5};
    head[2]-=offset;history[2]-=offset;
    near(head[2],-113.0528f);near(head[2],history[2]);near(head[0],nozzle[0]);near(head[1],nozzle[1]);
    near(engineGlowRadius(100,10,1,1,.82f,.2f),radius);
    near(engineGlowOriginOffset(0,8,0),5.15f);
    for(float ratio:{0.f,.02f,.5f,1.f,2.f})for(float pulse:{.95f,1.f})
    {
        float rad=engineGlowRadius(10,-1,ratio,pulse,.82f,.2f);
        float gap=engineGlowOriginOffset(0,8,rad);
        if(!std::isfinite(gap)||gap<4.8f)std::exit(1);
    }
    std::puts("Glow radius preservation and trail anchor offset checks passed.");return 0;
}
