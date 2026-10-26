#include "FrameWorker.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
extern "C" {
#include "Universe.h"
#include "Ships.h"
}
#undef min
#undef max
Universe universe;
static unsigned lazyQueries;
static std::thread::id caller;
static bool32 rmPresentationOwnsHarvestSound(const void *owner) {
    if(std::this_thread::get_id()!=caller)throw std::runtime_error("audio worker performed a lazy native-model lookup");
    ++lazyQueries;return (static_cast<const Ship *>(owner)->shipID.shipNumber&1)?TRUE:FALSE;
}
#include "../src/Game/AudioFrameSnapshot.inc"
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main() try {
    caller=std::this_thread::get_id();
    Ship ships[40]={};Node nodes[40]={};
    for(unsigned i=0;i<40;++i) {
        ships[i].objtype=OBJ_ShipType;ships[i].shiptype=ResourceCollector;
        ships[i].shipID.shipNumber=static_cast<uword>(i);
        ships[i].rceffect=reinterpret_cast<Effect *>(&ships[i]);
        nodes[i].structptr=&ships[i];nodes[i].prev=i?&nodes[i-1]:nullptr;
        nodes[i].next=i<39?&nodes[i+1]:nullptr;
    }
    universe.RenderList.head=&nodes[0];universe.RenderList.tail=&nodes[39];
    hwmodern::FrameWorker audio(L"Audio ownership regression");
    for(unsigned epoch=0;epoch<50;++epoch) {
        soundFrameHarvestSnapshot=FALSE;lazyQueries=0;
        ships[1].shiptype=epoch&1?LightInterceptor:ResourceCollector;
        ships[3].rceffect=epoch&1?nullptr:reinterpret_cast<Effect *>(&ships[3]);
        require(soundEventSnapshotHarvestOwners()!=FALSE,"snapshot failed");
        const auto captured=lazyQueries;
        hwmodern::FrameJob pass(audio,[&] {
            for(unsigned i=0;i<40;++i) {
                bool expected=(i&1)&&ships[i].shiptype==ResourceCollector&&ships[i].rceffect;
                require((soundEventFrameOwnsHarvestSound(&ships[i])!=FALSE)==expected,"audio ownership leaked across frames");
            }
        });
        require(pass.wait(),"audio ownership worker failed");
        require(lazyQueries==captured,"audio worker repeated lazy native lookups");
    }
    soundFrameHarvestSnapshot=FALSE;
    require(soundEventFrameOwnsHarvestSound(&ships[5])!=FALSE,"synchronous caller lost native ownership lookup");
    free(soundFrameHarvestOwners);
    std::puts("PASS: 50 frozen audio frames, native ownership queried only on game thread, changing collectors and synchronous fallback preserved");
    return 0;
} catch(const std::exception &error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
