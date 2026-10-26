#pragma once
#include <cstdint>
#include <vector>
namespace hwmodern {
struct RasterEmissionSource { std::uint64_t token=0;unsigned width=0,height=0; };
using RasterEmissionDescribe=bool (*)(const void*,RasterEmissionSource&);
using RasterEmissionRead=bool (*)(std::uint64_t,std::vector<unsigned char>&);
}
