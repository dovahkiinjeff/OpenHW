#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include "../src/Game/PilotCollisionPolicy.h"
int main(void)
{
    assert(pilotRamScale(0,100)==0);
    assert(pilotRamScale(-10,100)==0);
    assert(pilotRamScale(100,0)==0);
    assert(pilotRamScale(10,100)==0.1f);
    assert(pilotRamScale(100,100)==1);
    assert(pilotRamScale(200,100)==2);
    assert(pilotContactImpulse(-1,1,1)==0);
    assert(pilotContactImpulse(10,0,0)==0);
    assert(pilotContactImpulse(10,1,1)==5);
    assert(pilotContactImpulse(10,1,0)==10);
    /* Inelastic normal impulse removes closing velocity, conserves momentum,
       and never supplies a sideways or distance-based repulsion component. */
    {
        float a=30,b=-10,m1=2,m2=8;
        float impulse=pilotContactImpulse(a-b,1/m1,1/m2);
        float after1=a-impulse/m1,after2=b+impulse/m2;
        assert(fabsf(after1-after2)<0.0001f);
        assert(fabsf(after1*m1+after2*m2-(a*m1+b*m2))<0.0001f);
    }
    /* Cancel inward thrust at rest; a second response in the same tick must
       not add another impulse after the accumulated force is corrected. */
    {
        float dt=0.05f, inverseMass=0.5f, force=100;
        float predicted=force*inverseMass*dt;
        float impulse=pilotContactImpulse(predicted,inverseMass,0);
        force-=impulse/dt;
        assert(fabsf(force)<0.0001f);
        assert(pilotContactImpulse(force*inverseMass*dt,inverseMass,0)==0);
    }
    return 0;
}
