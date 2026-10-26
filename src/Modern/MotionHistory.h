#pragma once
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstdint>
#include <vector>

namespace hwmodern { namespace motionhistory {
// The owner token identifies a live game object, not its allocation address or
// wrapping game/save ID. The part identifies its exact submitted mesh node.
// Geometry/material compatibility is deliberately a separate history check.
struct StableKey
{
    std::uint64_t owner = 0, part = 0;
    bool valid() const { return owner != 0 && part != 0; }
    bool operator==(const StableKey &other) const
    { return owner == other.owner && part == other.part; }
};
struct StableKeyHash
{
    size_t operator()(const StableKey &key) const
    {
        std::uint64_t value = key.owner ^ (key.part + 0x9e3779b97f4a7c15ull +
            (key.owner << 6) + (key.owner >> 2));
        value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ull;
        value ^= value >> 27; value *= 0x94d049bb133111ebull;
        return static_cast<size_t>(value ^ (value >> 31));
    }
};
class StableIndex
{
public:
    static constexpr size_t npos = static_cast<size_t>(-1);
    void clear()
    {
        used = 0;
        if (++generation == 0)
        {
            for (auto &entry : entries) entry.generation = 0;
            generation = 1;
        }
    }
    void reserve(size_t count)
    {
        if (count <= entries.size() / 2) return;
        size_t capacity = entries.empty() ? 8 : entries.size();
        while (capacity / 2 < count) capacity *= 2;
        std::vector<Entry> old;
        old.swap(entries);
        entries.resize(capacity);
        for (const auto &entry : old)
            if (entry.generation == generation) entries[slot(entry.key)] = entry;
    }
    bool insert(StableKey key, size_t index)
    {
        if (!key.valid() || index == npos) return false;
        if (entries.empty()) reserve(used + 1);
        size_t target = slot(key);
        // Duplicate identities are ambiguous. Never pick one by draw order.
        if (entries[target].generation == generation)
        { entries[target].index = npos; return false; }
        if (used + 1 > entries.size() / 2)
        {
            reserve(used + 1);
            target = slot(key);
        }
        entries[target] = {key, index, generation};
        ++used;
        return true;
    }
    size_t find(StableKey key) const
    {
        if (!key.valid() || entries.empty()) return npos;
        const Entry &entry = entries[slot(key)];
        return entry.generation == generation ? entry.index : npos;
    }
private:
    struct Entry
    {
        StableKey key;
        size_t index = npos;
        std::uint64_t generation = 0;
    };
    size_t slot(StableKey key) const
    {
        const size_t mask = entries.size() - 1;
        size_t index = StableKeyHash{}(key) & mask;
        while (entries[index].generation == generation && !(entries[index].key == key))
            index = (index + 1) & mask;
        return index;
    }
    // Reuse a flat table across frames: no per-object node allocation/free and
    // no shrinking/rebuilding when the camera changes the visible population.
    std::vector<Entry> entries;
    size_t used = 0;
    std::uint64_t generation = 1;
};

// Column-major affine transforms; compute the camera inverse once per scene.
inline bool inverseAffine(const float m[16], float inverse[16])
{
    for (unsigned n=0;n<16;++n) if(!std::isfinite(m[n])) return false;
    if(std::abs(m[3])>1e-6f || std::abs(m[7])>1e-6f || std::abs(m[11])>1e-6f || std::abs(m[15]-1)>1e-6f) return false;
    const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-12) return false;
    const double rows[3][3]={
        {(e*i-f*h)/determinant,(c*h-b*i)/determinant,(b*f-c*e)/determinant},
        {(f*g-d*i)/determinant,(a*i-c*g)/determinant,(c*d-a*f)/determinant},
        {(d*h-e*g)/determinant,(b*g-a*h)/determinant,(a*e-b*d)/determinant}};
    for(unsigned row=0;row<3;++row) {
        for(unsigned col=0;col<3;++col) inverse[col*4+row]=static_cast<float>(rows[row][col]);
        inverse[12+row]=static_cast<float>(-(rows[row][0]*m[12]+rows[row][1]*m[13]+rows[row][2]*m[14]));
    }
    inverse[3]=inverse[7]=inverse[11]=0; inverse[15]=1;
    for(unsigned n=0;n<16;++n) if(!std::isfinite(inverse[n])) return false;
    return true;
}
inline bool multiplyAffine(const float left[16],const float right[16],float output[16])
{
    float temporary[16]={};
    for(unsigned col=0;col<4;++col) for(unsigned row=0;row<3;++row) {
        double value=col==3?left[12+row]:0.0;
        for(unsigned axis=0;axis<3;++axis) value+=static_cast<double>(left[axis*4+row])*right[col*4+axis];
        temporary[col*4+row]=static_cast<float>(value);
        if(!std::isfinite(temporary[col*4+row])) return false;
    }
    temporary[15]=1; for(unsigned n=0;n<16;++n) output[n]=temporary[n]; return true;
}
inline double transformDistance(const float previous[16],const float current[16])
{
    double distance=0;
    for(unsigned n=0;n<15;++n) {
        const double delta=static_cast<double>(previous[n])-current[n];
        distance+=delta*delta*(n>=12?1.0:0.01);
    }
    return distance;
}
// The caller supplies a geometry/material bucket. Keep this exact selector
// shared with the repeated-frigate CPU tests, rather than copying its logic.
template<class Instances,class Bucket,class Matched>
size_t nearest(const float world[16],const Instances &previous,const Bucket &bucket,const Matched &matched)
{
    size_t selected=previous.size(); double best=std::numeric_limits<double>::infinity();
    for(const auto index:bucket) {
        if(index>=previous.size()||index>=matched.size()||matched[index]||!previous[index].worldTransformValid) continue;
        const double distance=transformDistance(previous[index].worldTransform,world);
        if(distance<best) { selected=index; best=distance; }
    }
    return selected;
}
// Unjittered CPU reference of the shader projection. Previous positions need
// previous tan-FOV/aspect; jitter belongs only to history texture sampling.
inline bool project(const float position[3],float tanHalf,float aspect,float uv[2])
{
    if(!std::isfinite(tanHalf)||!std::isfinite(aspect)||tanHalf<=0||aspect<=0||
       !std::isfinite(position[0])||!std::isfinite(position[1])||!std::isfinite(position[2])||position[2]>=-.05f) return false;
    uv[0]=.5f+position[0]/(-position[2]*tanHalf*aspect)*.5f;
    uv[1]=.5f-position[1]/(-position[2]*tanHalf)*.5f;
    return std::isfinite(uv[0])&&std::isfinite(uv[1]);
}
} }
