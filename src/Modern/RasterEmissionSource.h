#pragma once
#include <cstdint>
#include <vector>
namespace hwmodern {
enum class RasterChannel : unsigned { emission, nativeControl, nativeNormal };
struct RasterEmissionSource { std::uint64_t token=0;unsigned width=0,height=0; };
using RasterEmissionDescribe=bool (*)(const void*,RasterChannel,RasterEmissionSource&);
using RasterEmissionRead=bool (*)(std::uint64_t,std::vector<unsigned char>&);
}
