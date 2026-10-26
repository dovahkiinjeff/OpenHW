#include "../src/Game/ClassicGunRestPose.h"
#include <vector>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <array>
#include <fcntl.h>
#include <io.h>
static void require(bool value,const char *reason) {if(!value) throw std::runtime_error(reason);}
static void u32(std::vector<unsigned char>& b,size_t offset,uint32_t value) {
    for(unsigned i=0;i<4;++i) b[offset+i]=(value>>(8*i))&255;
}
static void matrix(std::vector<unsigned char>& b,size_t object,const float *m) {
    for(unsigned i=0;i<16;++i) {uint32_t bits;memcpy(&bits,&m[i],4);u32(b,object+48+4*i,bits);}
}
int main(int argc,char **argv) {
    try {
        if(argc>1) {
            _setmode(_fileno(stdin),_O_BINARY);
            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(std::cin)),{});
            for(int i=1;i<argc;++i) {
                float pose[16],local[16];
                require(classicGunRestPose(bytes.data(),bytes.size(),argv[i],pose,local)!=0,"real classic gun frame missing");
                float determinant=pose[0]*(pose[5]*pose[10]-pose[9]*pose[6])-
                    pose[4]*(pose[1]*pose[10]-pose[9]*pose[2])+pose[8]*(pose[1]*pose[6]-pose[5]*pose[2]);
                require(std::abs(determinant-1)<.001f,"real classic gun frame not a rotation");
            }
            return 0;
        }
        std::vector<unsigned char> bytes(68+224+10,0);
        memcpy(bytes.data(),"RMF99ba",7);u32(bytes,8,0x402);u32(bytes,40,2);
        u32(bytes,68,292);memcpy(bytes.data()+292,"Hull",5);
        u32(bytes,180,297);memcpy(bytes.data()+297,"Gun",4);u32(bytes,180+36,68);
        const float parent[]={-1,0,0,0,0,-1,0,0,0,0,1,0,10,20,30,1};
        const float child[]={1,0,0,0,0,1,0,0,0,0,1,0,2,3,4,1};
        matrix(bytes,68,parent);matrix(bytes,180,child);
        float result[16],local[16];
        require(classicGunRestPose(bytes.data(),bytes.size(),"Gun",result,local)!=0,"synthetic pose failed");
        require(result[0]==-1 && result[5]==-1 && result[10]==1,"parent rest rotation lost");
        require(result[12]==8 && result[13]==17 && result[14]==34,"parent composition wrong");
        require(memcmp(local,child,sizeof(child))==0,"local rest frame changed");
        std::array<float,16> prior;memcpy(prior.data(),result,sizeof(result));
        require(!classicGunRestPose(bytes.data(),bytes.size(),"Missing",result,local),"missing name accepted");
        require(memcmp(result,prior.data(),sizeof(result))==0,"failure wrote output");
        u32(bytes,180+36,180);
        require(!classicGunRestPose(bytes.data(),bytes.size(),"Gun",result,local),"cycle accepted");
        u32(bytes,180+36,69);
        require(!classicGunRestPose(bytes.data(),bytes.size(),"Gun",result,local),"unaligned parent accepted");
        require(!classicGunRestPose(bytes.data(),68,"Gun",result,local),"truncated objects accepted");
        u32(bytes,180+36,68);u32(bytes,68,297);
        require(!classicGunRestPose(bytes.data(),bytes.size(),"Gun",result,local),"ambiguous name accepted");
        std::cout<<"Classic combat rest-frame metadata tests passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
