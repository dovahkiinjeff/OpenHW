#ifdef NDEBUG
#undef NDEBUG
#endif
#include "RemasteredWeaponFXData.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <limits>
static_assert(sizeof(RmFxHeader)==28 && sizeof(RmFxMapping)==52 && sizeof(RmFxEffect)==12 && sizeof(RmFxLayer)==108 && sizeof(RmFxKey)==20,"FX package ABI");
int main(int argc,char **argv)
{
    assert(argc==2);
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
    assert(!bytes.empty() && rmFxValidate(bytes.data(),bytes.size()));
    auto corrupt=bytes; reinterpret_cast<RmFxHeader *>(corrupt.data())->layers=UINT32_MAX;
    assert(!rmFxValidate(corrupt.data(),corrupt.size()));
    assert(!rmFxValidate(bytes.data(),bytes.size()-1));
    corrupt=bytes; reinterpret_cast<RmFxMapping *>(corrupt.data()+sizeof(RmFxHeader))->scale=std::numeric_limits<float>::quiet_NaN();
    assert(!rmFxValidate(corrupt.data(),corrupt.size()));
    corrupt=bytes; reinterpret_cast<RmFxMapping *>(corrupt.data()+sizeof(RmFxHeader))->projectile=999999;
    assert(!rmFxValidate(corrupt.data(),corrupt.size()));
    corrupt=bytes; corrupt[0]='X'; assert(!rmFxValidate(corrupt.data(),corrupt.size()));
}
