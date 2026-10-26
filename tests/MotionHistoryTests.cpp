#include "../src/Modern/MotionHistory.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace allocationprobe
{
bool enabled=false;
size_t allocations=0,deallocations=0;
}
// Count real heap traffic only inside the isolated table-reuse fixture. Other
// test vectors, source reads and the C++ runtime remain outside that interval.
void *operator new(size_t bytes)
{
    if(allocationprobe::enabled) ++allocationprobe::allocations;
    if(void *memory=std::malloc(bytes?bytes:1)) return memory;
    throw std::bad_alloc();
}
void *operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void *memory) noexcept
{
    if(memory && allocationprobe::enabled) ++allocationprobe::deallocations;
    std::free(memory);
}
void operator delete[](void *memory) noexcept { ::operator delete(memory); }
void operator delete(void *memory,size_t) noexcept { ::operator delete(memory); }
void operator delete[](void *memory,size_t) noexcept { ::operator delete(memory); }

namespace
{
namespace motion = hwmodern::motionhistory;
unsigned int checks = 0;
void require(bool ok, const char *message)
{ ++checks; if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); } }
using Matrix = std::array<float, 16>;
Matrix identity() { Matrix m{}; m[0]=m[5]=m[10]=m[15]=1; return m; }
Matrix object(float x,float y,float z)
{ auto m=identity(); m[12]=x; m[13]=y; m[14]=z; return m; }
Matrix view(float cameraZ,float angle=0)
{
    auto m=identity(); m[0]=m[10]=std::cos(angle); m[2]=std::sin(angle); m[8]=-std::sin(angle);
    m[14]=-cameraZ; return m;
}
Matrix multiply(const Matrix &a,const Matrix &b)
{ Matrix out{}; require(motion::multiplyAffine(a.data(),b.data(),out.data()),"production affine multiply"); return out; }
Matrix recover(const Matrix &camera,const Matrix &modelView)
{
    Matrix inverse{}; require(motion::inverseAffine(camera.data(),inverse.data()),"production camera affine inverse");
    return multiply(inverse,modelView);
}
struct Instance
{
    float worldTransform[16] = {};
    bool worldTransformValid = true;
    Matrix modelView{};
};
Instance instance(const Matrix &camera,const Matrix &world)
{
    Instance out; out.modelView=multiply(camera,world); const auto transform=recover(camera,out.modelView);
    std::copy(transform.begin(),transform.end(),out.worldTransform); return out;
}

void repeatedShips()
{
    const Matrix worlds[] = {object(0,0,0),object(0,0,100),object(0,0,200)};
    const auto oldView=view(1000);
    std::vector<Instance> previous;
    for(const auto &world:worlds) previous.push_back(instance(oldView,world));
    const std::vector<size_t> bucket={0,1,2};
    for(float cameraZ:{900.0f,1100.0f,750.0f,1250.0f})
    {
        const auto currentView=view(cameraZ);
        for(const std::vector<size_t> order:{std::vector<size_t>{0,1,2},{2,0,1},{1,2,0}})
        {
            std::vector<unsigned char> matched(3,0);
            for(const auto id:order)
            {
                const auto current=instance(currentView,worlds[id]);
                const auto found=motion::nearest(current.worldTransform,previous,bucket,matched);
                require(found==id,"identical frigates retain identity through dolly in/out and render reorder");
                matched[found]=1;
            }
        }
    }
    const auto current=instance(view(900),worlds[0]);
    require(motion::transformDistance(previous[1].modelView.data(),current.modelView.data())==0 &&
        motion::transformDistance(previous[0].modelView.data(),current.modelView.data())>0,
        "fixture reproduces original view-space nearest-neighbor identity swap");
    const std::vector<size_t> culledBucket={0,2};
    std::vector<unsigned char> matched(3,0);
    require(motion::nearest(instance(view(900),worlds[2]).worldTransform,previous,culledBucket,matched)==2,
        "culling another same-geometry frigate does not shift identity");
    matched[2]=1;
    require(motion::nearest(instance(view(1100),worlds[0]).worldTransform,previous,culledBucket,matched)==0,
        "one-to-one matching survives culling and reverse submit order");
    matched[0]=1;
    require(motion::nearest(current.worldTransform,previous,culledBucket,matched)==previous.size(),
        "matched instances cannot be assigned twice");
    previous[1].worldTransformValid=false;
    require(motion::nearest(current.worldTransform,previous,std::vector<size_t>{99,1},matched)==previous.size(),
        "invalid transforms/bucket entries cannot poison motion history");
}

void orbitAndObjectMotion()
{
    const Matrix worlds[]={object(-200,30,0),object(0,0,0),object(200,-30,0)};
    std::vector<Instance> previous;
    for(const auto &world:worlds) previous.push_back(instance(view(1000),world));
    for(float angle:{-.7f,-.035f,.035f,.7f})
    {
        std::vector<unsigned char> matched(3,0);
        const auto camera=view(1000,angle);
        for(size_t id:{2u,0u,1u})
        {
            const auto current=instance(camera,worlds[id]);
            for(unsigned axis=0;axis<3;++axis)
                require(std::abs(current.worldTransform[12+axis]-worlds[id][12+axis])<.0002f,
                    "world transform recovery cancels full orbit rotation and translation");
            const auto found=motion::nearest(current.worldTransform,previous,std::vector<size_t>{0,1,2},matched);
            require(found==id,"identical frigates retain world identity through camera orbit"); matched[found]=1;
        }
    }
    const auto movedWorld=object(-197,35,1);
    const auto moved=instance(view(900,.1f),movedWorld);
    std::vector<unsigned char> matched(3,0);
    const auto id=motion::nearest(moved.worldTransform,previous,std::vector<size_t>{0,1,2},matched);
    require(id==0,"genuinely moving frigate stays paired with its prior object");
    require(previous[id].modelView[12]==-200 && previous[id].modelView[13]==30,
        "real object motion retains prior raw model-view, not reconstructed current pose");
    const auto cameraOnly=multiply(view(1000),movedWorld);
    require(cameraOnly[12]==-197 && cameraOnly[13]==35 && previous[id].modelView[12]!=cameraOnly[12],
        "matched history includes object motion rather than camera-only fallback");
    auto rotated=identity(); rotated[0]=rotated[5]=0; rotated[1]=1; rotated[4]=-1;
    std::vector<Instance> modules={instance(view(1000),identity()),instance(view(1000),rotated)};
    const auto rotatedCurrent=instance(view(800,.2f),rotated);
    require(motion::nearest(rotatedCurrent.worldTransform,modules,std::vector<size_t>{0,1},std::vector<unsigned char>(2,0))==1,
        "world orientation breaks tied-position repeated-module matches");
}

void stableShipsAndLifetime()
{
    // Identical models deliberately exchange world positions. A proximity
    // matcher picks the wrong ship even after camera motion is removed.
    const auto oldCamera=view(1000);
    const Matrix oldWorlds[]={object(-100,0,0),object(100,0,0)};
    const std::vector<Instance> previous={instance(oldCamera,oldWorlds[0]),instance(oldCamera,oldWorlds[1])};
    const motion::StableKey owners[]={{101,7001},{102,7001}};
    motion::StableIndex index; index.reserve(2);
    index.insert(owners[0],0); index.insert(owners[1],1);
    for(const std::vector<size_t> order:{std::vector<size_t>{0,1},{1,0}})
    {
        std::vector<unsigned char> claimed(2,0);
        for(const auto id:order)
        {
            const auto current=instance(view(750,.2f),oldWorlds[1-id]);
            const auto found=index.find(owners[id]);
            require(found==id,"crossing identical ships retain their own lifetime and part history");
            require(!claimed[found],"stable selection remains one-to-one through render reorder");
            claimed[found]=1;
            require(previous[found].modelView[12]==oldWorlds[id][12],
                "crossing ship uses its actual old pose rather than the nearest ship pose");
            require(motion::nearest(current.worldTransform,previous,std::vector<size_t>{0,1},
                std::vector<unsigned char>(2,0))==1-id,
                "crossing fixture reproduces incorrect legacy nearest-object association");
        }
    }

    const motion::StableKey newAtSamePose={103,7001};
    require(index.find(newAtSamePose)==motion::StableIndex::npos,
        "a newly created same-model ship at a deleted ship pose inherits no history");
    const auto reusedPose=instance(view(900),oldWorlds[0]);
    require(motion::nearest(reusedPose.worldTransform,previous,std::vector<size_t>{0,1},
        std::vector<unsigned char>(2,0))==0,
        "new-ship fixture exposes the legacy nearest-object history steal");
    const motion::StableKey recycledAddressNewGeneration={104,7001};
    require(index.find(recycledAddressNewGeneration)==motion::StableIndex::npos,
        "a reused allocation with a new owner lifetime cannot inherit a retired owner's pose");

    index.clear(); index.insert(owners[1],0);
    require(index.find(owners[0])==motion::StableIndex::npos && index.find(owners[1])==0,
        "a ship culled in the prior frame has no stale history while a visible ship still matches");
    index.clear(); index.insert(owners[0],4);
    require(index.find(owners[0])==4 && index.find(owners[1])==motion::StableIndex::npos,
        "clearing the frame index fully retires previous visible submissions");
}

void stablePartsAndInvalidKeys()
{
    motion::StableIndex index; index.reserve(8);
    const motion::StableKey body={201,9001},leftTurret={201,9002},rightTurret={201,9003};
    index.insert(body,0); index.insert(leftTurret,1); index.insert(rightTurret,2);
    // Identical modules can have precisely the same transform before their
    // independent animations diverge. Transform tie-breaking is insufficient.
    const std::vector<Instance> previous={instance(view(1000),identity()),
        instance(view(1000),identity()),instance(view(1000),identity())};
    auto rotated=identity(); rotated[0]=rotated[5]=0; rotated[1]=1; rotated[4]=-1;
    const auto animated=instance(view(700,.3f),rotated);
    require(motion::nearest(animated.worldTransform,previous,std::vector<size_t>{1,2},
        std::vector<unsigned char>(3,0))==1,
        "coincident-module fixture exposes arbitrary transform tie-breaking");
    require(index.find(rightTurret)==2 && index.find(leftTurret)==1 && index.find(body)==0,
        "owner and exact part keep coincident animated modules independent of submission order");
    require(index.find(motion::StableKey{202,9003})==motion::StableIndex::npos,
        "shared module geometry on another ship cannot steal this ship's part history");
    require(index.find(motion::StableKey{201,9101})==motion::StableIndex::npos,
        "an unmatched LOD part requests camera-only motion rather than guessing a previous part");
    require(index.find(motion::StableKey{})==motion::StableIndex::npos &&
        index.find(motion::StableKey{0,9001})==motion::StableIndex::npos &&
        index.find(motion::StableKey{201,0})==motion::StableIndex::npos,
        "missing lifetime or part identity cannot produce a stable match");
    index.insert(motion::StableKey{},3); index.insert(motion::StableKey{0,9001},4);
    index.insert(motion::StableKey{201,0},5);
    require(index.find(body)==0 && index.find(motion::StableKey{})==motion::StableIndex::npos,
        "invalid submissions cannot poison a valid object's entry");
    index.insert(body,7);
    require(index.find(body)==motion::StableIndex::npos && index.find(rightTurret)==2,
        "duplicate prior owner/part keys fail closed without disturbing independent parts");
    index.insert(body,8);
    require(index.find(body)==motion::StableIndex::npos,
        "an ambiguous identity stays invalid after further duplicate submissions");
    index.clear(); index.insert(body,9);
    require(index.find(body)==9,"duplicate ambiguity clears at the next frame boundary");
    const motion::StableKey wideKey={(1ull<<61)+201,(1ull<<59)+9001};
    index.insert(wideKey,10);
    require(index.find(wideKey)==10 && index.find(body)==9 &&
        index.find(motion::StableKey{201,wideKey.part})==motion::StableIndex::npos,
        "full-width owner generations and part identities are retained without truncation");
}

void stableTableGrowthAndCollisions()
{
    // Select real production hash collisions rather than relying on lucky
    // sequential IDs. These keys share their low nine hash bits and therefore
    // form long probe chains through the table's early growth boundaries.
    std::array<motion::StableKey,160> keys{};
    size_t count=0;
    for(std::uint64_t part=1;count<keys.size();++part)
    {
        const motion::StableKey key={3001,part};
        if((motion::StableKeyHash{}(key)&511u)==0) keys[count++]=key;
    }
    motion::StableIndex index; index.reserve(1);
    require(index.insert(keys[0],0),"first colliding key enters an initially small table");
    require(!index.insert(keys[0],999),"duplicate colliding key becomes ambiguous before growth");
    for(size_t n=1;n<keys.size();++n)
    {
        require(index.insert(keys[n],n),"new colliding keys remain distinct across automatic growth");
        require(index.find(keys[n])==n,"new key remains findable at the end of a collision chain");
        if(n%17==0)
        {
            require(index.find(keys[0])==motion::StableIndex::npos,
                "automatic rehash preserves the ambiguity of an earlier duplicate");
            for(size_t old=1;old<=n;++old)
                require(index.find(keys[old])==old,"automatic rehash preserves every older colliding key");
        }
    }
    require(!index.insert(keys[79],999) && !index.insert(keys[159],999),
        "duplicates near the middle and end of a collision chain fail closed");
    index.reserve(2048);
    for(size_t n=0;n<keys.size();++n)
        require(index.find(keys[n])==((n==0||n==79||n==159)?motion::StableIndex::npos:n),
            "explicit large rehash preserves both valid and ambiguous collision entries");
    index.clear(); index.reserve(1);
    for(const auto &key:keys)
        require(index.find(key)==motion::StableIndex::npos,
            "generation clear makes all old collision-chain entries invisible");
    for(size_t n=keys.size();n-- > 0;)
        require(index.insert(keys[n],n+1000),"cleared collision slots accept keys in reversed draw order");
    for(size_t n=0;n<keys.size();++n)
        require(index.find(keys[n])==n+1000,"reversed collision-chain insertion recovers every exact identity");
}

void stableTableReuseWithoutAllocations()
{
    // First demonstrate that the global allocator probe sees this very table's
    // allocations; a permanently zero or disconnected counter would be useless.
    allocationprobe::allocations=allocationprobe::deallocations=0;
    allocationprobe::enabled=true;
    {
        motion::StableIndex cold;
        cold.reserve(16); cold.insert(motion::StableKey{4001,1},0);
        require(cold.find(motion::StableKey{4001,1})==0,"allocator-probe table performs a real insertion");
    }
    allocationprobe::enabled=false;
    require(allocationprobe::allocations>0 && allocationprobe::deallocations>0,
        "heap probe observes cold table allocation and destruction");

    std::array<motion::StableKey,512> keys{};
    for(size_t n=0;n<keys.size();++n) keys[n]={5001+n%7,10001+n};
    motion::StableIndex index; index.reserve(keys.size());
    const size_t populations[]={0,1,17,511,8,512,256,2,64};
    allocationprobe::allocations=allocationprobe::deallocations=0;
    allocationprobe::enabled=true;
    for(size_t frame=0;frame<64;++frame)
    {
        const auto population=populations[frame%std::size(populations)];
        index.clear(); index.reserve(population);
        for(const auto &key:keys)
            require(index.find(key)==motion::StableIndex::npos,
                "each reused frame starts without stale identity entries");
        for(size_t n=0;n<population;++n)
        {
            const size_t id=(frame%2)?population-1-n:(n+frame*7)%population;
            require(index.insert(keys[id],frame*10000+id),
                "warm reused table accepts varying visible populations and draw order");
        }
        if(population>1)
            require(!index.insert(keys[0],999),"duplicate identity still fails closed in a full warm table");
        for(size_t n=0;n<keys.size();++n)
        {
            const auto expected=(n>=population||(population>1&&n==0))?
                motion::StableIndex::npos:frame*10000+n;
            require(index.find(keys[n])==expected,
                "warm frame reuse retains current identities and excludes culled or ambiguous ones");
        }
    }
    const auto allocations=allocationprobe::allocations,deallocations=allocationprobe::deallocations;
    allocationprobe::enabled=false;
    std::printf("Stable identity table: 64 reused frames, %zu heap allocations and %zu frees after reserve.\n",
        allocations,deallocations);
    require(allocations==0 && deallocations==0,
        "warm clear/reserve/insert/find cycles perform no heap allocation or deallocation");
}

void projectionAndLod()
{
    const float p[3]={100,50,-1000}; float oldUv[2]={},newUv[2]={};
    require(motion::project(p,.5f,2.0f,oldUv)&&motion::project(p,1.0f,1.0f,newUv),"old/new camera projections valid");
    require(std::abs(oldUv[0]-.55f)<1e-6f&&std::abs(oldUv[1]-.45f)<1e-6f&&
        std::abs(newUv[0]-.55f)<1e-6f&&std::abs(newUv[1]-.475f)<1e-6f,
        "previous UV uses prior FOV/aspect during zoom");
    require(std::abs((oldUv[1]-newUv[1])*1080+27)<.0001f,
        "FOV zoom generates correct unjittered pixel motion instead of false zero");
    motion::project(p,.5f,1.0f,newUv);
    require(std::abs((oldUv[0]-newUv[0])*1920+96)<.0002f,"aspect change uses independent previous projection");
    const auto world=object(100,0,0);
    const auto current=multiply(view(900),world);
    const auto fallback=multiply(view(1000),recover(view(900),current));
    const float currentPoint[3]={current[12],current[13],current[14]};
    const float previousPoint[3]={fallback[12],fallback[13],fallback[14]};
    motion::project(currentPoint,1,1,newUv); motion::project(previousPoint,1,1,oldUv);
    require(std::abs((oldUv[0]-newUv[0])*2560+14.222222f)<.0003f,
        "new LOD gets camera-only dolly motion instead of zero");
    const float bad[3]={0,0,0}; require(!motion::project(bad,1,1,newUv),"invalid prior depth rejected");
    const float jitterCurrent=.375f,jitterPrevious=-.125f;
    require(std::abs(((oldUv[0]-newUv[0])*2560+jitterCurrent-jitterPrevious)-
        ((oldUv[0]-newUv[0])*2560+.5f))<1e-6f,"projection vectors need exactly one history jitter correction");
    auto singular=identity(); singular[0]=0; Matrix inverse{};
    require(!motion::inverseAffine(singular.data(),inverse.data()),"singular view rejected");
    auto invalid=identity(); invalid[3]=.1f;
    require(!motion::inverseAffine(invalid.data(),inverse.data()),"perspective matrix cannot be used as affine camera");
    invalid=identity(); invalid[2]=std::numeric_limits<float>::quiet_NaN();
    require(!motion::inverseAffine(invalid.data(),inverse.data()),"nonfinite camera rejected");
    auto scaled=identity(); scaled[0]=2; scaled[5]=3; scaled[10]=4; scaled[12]=50;
    const auto restored=recover(scaled,multiply(scaled,world));
    require(std::abs(restored[12]-100)<.0001f,"affine recovery also handles nonunit camera basis safely");
}

void productionWiring()
{
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto read=[](const std::filesystem::path &path) {
        std::ifstream input(path); return std::string(std::istreambuf_iterator<char>(input),{});
    };
    const auto source=read(root/"src/Modern/ModernRaytracing.cpp");
    const auto shader=read(root/"src/Modern/ModernRaytracing.hlsl");
    require(!source.empty()&&!shader.empty(),"read actual production motion integration");
    const auto stableSelection=source.find("state.previousInstanceIdentities.find(instance.motionIdentity)");
    const auto unownedFallback=source.find("// Preserve existing unowned scenery behavior in a separate bucket.");
    const auto nearestSelection=source.find("motionhistory::nearest(instance.worldTransform");
    require(stableSelection!=std::string::npos && unownedFallback>stableSelection &&
        nearestSelection>unownedFallback && nearestSelection!=std::string::npos,
        "production uses tested stable selection for ships and isolates the legacy unowned fallback");
    require(source.find("state.previousInstanceIdentities.insert(previous.motionIdentity, index)")!=std::string::npos &&
        source.find("state.currentInstanceIdentities.insert(")!=std::string::npos,
        "previous and current frame submissions both detect ambiguous exact part identities");
    require(source.find("geometry == state.previousInstances[exact].geometry &&")!=std::string::npos,
        "tracked geometry replacement uses camera-only motion instead of incompatible part-space history");
    require(source.find("instance.surfaceHistoryValid = geometry == previous.geometry &&")!=std::string::npos &&
        source.find("variant == previous.surfaceVariant")!=std::string::npos,
        "geometry and material compatibility separately control radiance history after exact object matching");
    require(source.find("state.previousInstances[previousIndex].modelView")!=std::string::npos,
        "production retains previous raw model-view for matched object motion");
    require(source.find("motionhistory::multiplyAffine(state.previousViewMatrix, instance.worldTransform")!=std::string::npos,
        "production provides camera-only unmatched/LOD motion");
    require(shader.find("? PreviousViewPositionToUv(previousPosition) : currentUv")!=std::string::npos&&
        shader.find("previousUv = PreviousViewDirectionToUv(previousViewDirection)")!=std::string::npos,
        "both closest-hit and sky miss actually use previous projection");
    require(shader.find("payload.previousHistoryDepth = (motion.flags & 2u) != 0u")!=std::string::npos&&
        shader.find("if ((instance.flags & 1u) != 0u")!=std::string::npos,
        "camera-only motion preserves invalid surface history and independent clip flag");
    const std::string correction="currentJitter - previousJitter";
    const auto first=shader.find(correction);
    require(first!=std::string::npos&&shader.find(correction,first+1)==std::string::npos,
        "production applies history projection jitter correction exactly once");
    require(source.find("sizeof(GpuInstanceMotion) == 76")!=std::string::npos&&
        source.find("sizeof(FrameConstants) == 224")!=std::string::npos,
        "instance payload stride preserved and new constants alignment asserted");
}
}
int main()
{
    repeatedShips(); orbitAndObjectMotion(); stableShipsAndLifetime(); stablePartsAndInvalidKeys();
    stableTableGrowthAndCollisions(); stableTableReuseWithoutAllocations();
    projectionAndLod(); productionWiring();
    std::printf("Motion history CPU regression tests passed: %u runtime checks.\n",checks);
}
