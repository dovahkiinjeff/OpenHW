#include "../src/Game/EngineFade.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void near(float a,float b) { if(std::fabs(a-b)>0.0001f) { std::fprintf(stderr,"%f != %f\n",a,b);std::exit(1); } }
int main() {
    near(engineFadeAdvance(0,0.375f,1),0.5f);
    near(engineFadeAdvance(0.5f,0.1875f,0),0.25f);
    near(engineFadeAdvance(0.25f,0.1875f,1),0.5f);
    near(engineFadeAdvance(0.5f,0,1),0.5f);
    near(engineFadeAdvance(0.5f,-1,0),0.5f);
    near(engineFadeAdvance(0,10,1),1);
    near(engineFadeAdvance(1,10,0),0);
    float slow=0,fast=0,idle=0;
    for(int i=0;i<15;++i)slow=engineFadeAdvance(slow,1.0f/30,1);
    for(int i=0;i<120;++i) { fast=engineFadeAdvance(fast,1.0f/240,1);idle=engineFadeAdvance(idle,1.0f/240,0); }
    near(slow,fast);near(idle,0);
    return 0;
}
