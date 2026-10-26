#include "RemasteredAudioDecode.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The Relic v1.6 transform and quantization use the same conventions as
   fqcodec/fquant in this engine. Keep decoder state local: the old FFT has
   shared scratch variables and cannot also run on an asset-loading thread. */
#define RM_PI 3.14159265358979323846f
static unsigned be16(const unsigned char *p) {return (unsigned)p[0]*256+p[1];}
static unsigned be32(const unsigned char *p)
{return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
int hwRmAudioInspect(const void *bytes,size_t size,HwRmAudioInfo *info)
{
    const unsigned char *b=(const unsigned char *)bytes,*comm=NULL,*data=NULL;
    size_t at=12,commSize=0,dataSize=0,offset;
    HwRmAudioInfo result={0};
    if(info)memset(info,0,sizeof(*info));
    if(!b || !info || size<12 || memcmp(b,"FORM",4) || memcmp(b+8,"AIFC",4) ||
       (size_t)be32(b+4)!=size-8) return 0;
    while(at<size) {
        size_t n;
        if(size-at<8)return 0;
        n=be32(b+at+4);
        if(n>size-at-8)return 0;
        if(!memcmp(b+at,"COMM",4)) {if(comm)return 0;comm=b+at+8;commSize=n;}
        if(!memcmp(b+at,"SSND",4)) {if(data)return 0;data=b+at+8;dataSize=n;}
        at+=8+n;
        if(n&1) {if(at==size)return 0;++at;}
    }
    if(!comm || commSize<39 || !data || dataSize<10 || memcmp(comm+18,"COMP",4) ||
       comm[22]!=16 || (size_t)comm[22]>commSize-23 ||
       memcmp(comm+23,"Relic Codec v1.6",16) || be16(comm+6)!=16) return 0;
    /* The supported RM assets use the codec's fixed 44100 Hz output mode.
       Validate the full extended-precision input rate instead of guessing. */
    {
        static const unsigned char rate[]={0x40,0x0e,0xac,0x44,0,0,0,0,0,0};
        if(comm[8]!=rate[0] || (comm[9]!=14 && comm[9]!=13) || memcmp(comm+10,rate+2,8))return 0;
        result.codecRate=comm[9]==14?44100:22050;
    }
    result.channels=be16(comm);result.blocks=be32(comm+2);result.sampleRate=44100;
    if(result.channels<1 || result.channels>2 || !result.blocks)return 0;
    offset=be32(data);
    if(offset>dataSize-10)return 0;
    result.bitrate=be16(data+8+offset);
    if(result.bitrate<256 || result.bitrate>2048 || result.bitrate%8)return 0;
    result.dataOffset=(size_t)(data-b)+10+offset;result.dataBytes=dataSize-10-offset;
    if(result.blocks>SIZE_MAX/(512u*result.channels*sizeof(int16_t)))return 0;
    result.frames=(size_t)result.blocks*512;
    if(result.frames*result.channels*sizeof(int16_t)>256u*1024*1024)return 0;
    if(result.dataBytes!=(size_t)result.blocks*result.channels*(result.bitrate/8))return 0;
    *info=result;return 1;
}
typedef struct RmBitReader {const unsigned char *p;unsigned at,limit;int bad;} RmBitReader;
static unsigned bits(RmBitReader *r,unsigned count)
{
    unsigned v=0,i;
    if(count>16 || r->at>r->limit || count>r->limit-r->at){r->bad=1;return 0;}
    for(i=0;i<count;++i) v|=((unsigned)(r->p[(r->at+i)/8]>>((r->at+i)%8))&1u)<<i;
    r->at+=count;return v;
}
static int signedBits(RmBitReader *r,unsigned count)
{
    unsigned v=bits(r,count),sign=1u<<(count-1);
    return v&sign?-(int)(v&(sign-1)):(int)v;
}
static int unpack(const unsigned char *p,unsigned bytes,unsigned char *exponents,
                  float spectra[2][256],const float scales[6])
{
    static const unsigned bands[]={0,1,2,3,4,5,6,7,9,11,13,15,17,20,23,27,31,37,43,51,62,74,89,110,139,180,256};
    RmBitReader r={p,0,bytes*8,0};
    unsigned flags=bits(&r,2),bandBits=bits(&r,3),expBits=bits(&r,2),indexBits=bits(&r,4);
    unsigned i,j,pos=0,part;
    memset(spectra,0,2*256*sizeof(float));
    if(flags&1)memset(exponents,0,256);
    if(bandBits && expBits) for(i=0;i<26;++i) {
        unsigned move=bits(&r,bandBits),exponent;
        if(r.bad)return 0;
        if(i && !move)break;
        pos+=move;exponent=bits(&r,expBits);
        if(r.bad || pos>=26)return 0;
        for(j=bands[pos];j<bands[pos+1];++j)exponents[j]=(unsigned char)exponent;
    }
    if(indexBits)for(part=0;part<2;++part) {
        if(part && (flags&2)){memcpy(spectra[1],spectra[0],256*sizeof(float));break;}
        pos=0;
        for(i=0;i<256;++i) {
            unsigned move=bits(&r,indexBits),exponent;int v;
            if(r.bad)return 0;
            if(i && !move)break;
            pos+=move;if(pos>=256)return 0;
            exponent=exponents[pos];if(exponent>=6)return 0;
            v=signedBits(&r,exponent+2);if(r.bad)return 0;
            spectra[part][pos]=scales[exponent]*v;
        }
    }
    return !r.bad;
}
/* In-place radix-two complex FFT; all scratch storage belongs to the caller. */
static void fft128(float re[128],float im[128])
{
    unsigned i,j=0,len;
    for(i=1;i<128;++i) {
        unsigned bit=64;
        while(j&bit){j^=bit;bit>>=1;}j^=bit;
        if(i<j){float v=re[i];re[i]=re[j];re[j]=v;v=im[i];im[i]=im[j];im[j]=v;}
    }
    for(len=2;len<=128;len*=2) {
        unsigned base,k;float angle=-2*RM_PI/len,wr=cosf(angle),wi=sinf(angle);
        for(base=0;base<128;base+=len) {
            float tr=1,ti=0;
            for(k=0;k<len/2;++k) {
                unsigned a=base+k,b=a+len/2;
                float br=re[b]*tr-im[b]*ti,bi=re[b]*ti+im[b]*tr,next;
                re[b]=re[a]-br;im[b]=im[a]-bi;re[a]+=br;im[a]+=bi;
                next=tr*wr-ti*wi;ti=tr*wi+ti*wr;tr=next;
            }
        }
    }
}
static void transform(const float *spectrum,float *output,const float *rotation)
{
    float re[128],im[128],temporary[512];unsigned i;
    const float factor=8.0f/sqrtf(512.0f);
    for(i=0;i<128;++i) {
        float a=spectrum[i*2]*.5f,b=spectrum[255-i*2]*.5f;
        re[i]=a*rotation[128+i]+b*rotation[i];
        im[i]=-a*rotation[i]+b*rotation[128+i];
    }
    fft128(re,im);
    for(i=0;i<128;++i) {
        temporary[i*2]=(re[i]*rotation[128+i]+im[i]*rotation[i])*factor;
        temporary[i*2+256]=(-re[i]*rotation[i]+im[i]*rotation[128+i])*factor;
    }
    for(i=1;i<512;i+=2)temporary[i]=-temporary[511-i];
    memcpy(output,temporary+128,384*sizeof(float));
    for(i=384;i<512;++i)output[i]=-temporary[i-384];
}
int hwRmAudioDecode(const void *bytes,size_t size,int16_t **pcm,HwRmAudioInfo *info)
{
    HwRmAudioInfo result;int16_t *out;
    float rotation[256],window[512],scales[6],previous[2][512]={{0}};
    unsigned char exponents[2][256]={{0}};
    const unsigned char *at;unsigned i,block,channel,frameBytes;
    if(pcm)*pcm=NULL;
    if(info)memset(info,0,sizeof(*info));
    if(!pcm || !info || !hwRmAudioInspect(bytes,size,&result))return 0;
    out=(int16_t *)malloc(result.frames*result.channels*sizeof(int16_t));if(!out)return 0;
    for(i=0;i<128;++i) {
        float angle=((float)i+.125f)*(RM_PI*2)*(1.0f/512);
        rotation[i]=(float)sin(angle);rotation[128+i]=(float)cos(angle);
    }
    for(i=0;i<512;++i)window[i]=(float)sin(RM_PI/512*i);
    scales[0]=10;for(i=1;i<6;++i)scales[i]=scales[i-1]*10;
    for(i=0;i<6;++i)scales[i]=(float)(1.0/(double)((1u<<(i+1))-1)*scales[i]);
    frameBytes=result.bitrate/8;at=(const unsigned char *)bytes+result.dataOffset;
    for(block=0;block<result.blocks;++block)for(channel=0;channel<result.channels;++channel) {
        float spectrum[2][256],first[512],second[512],current[512];
        unsigned char padded[260]={0};
        memcpy(padded,at,frameBytes);at+=frameBytes;
        /* The original codec permits zero padding at a block's bit boundary. */
        if(!unpack(padded,sizeof(padded),exponents[channel],spectrum,scales)) {
            /* A few shipped streams contain out-of-range coefficients. Match
               decoder recovery: silence this block, reset overlap, continue
               at the next independently sized block. Never read outside an
               exponent or spectrum array to salvage a coefficient. */
            ++result.concealedBlocks;
            memset(previous[channel],0,sizeof(previous[channel]));
            memset(exponents[channel],0,sizeof(exponents[channel]));
            for(i=0;i<512;++i)out[((size_t)block*512+i)*result.channels+channel]=0;
            continue;
        }
        if(result.codecRate==22050) {
            memset(spectrum[0]+128,0,128*sizeof(float));
            memset(spectrum[1]+128,0,128*sizeof(float));
        }
        memcpy(current,previous[channel],sizeof(current));
        transform(spectrum[0],first,rotation);transform(spectrum[1],second,rotation);
        memcpy(previous[channel],second,sizeof(second));
        for(i=0;i<256;++i) {
            current[i+256]=current[i+256]*window[i+256]+first[i]*window[i];
            previous[channel][i]=first[i+256]*window[i+256]+second[i]*window[i];
        }
        for(i=0;i<512;++i) {
            float v=current[i];
            if(!isfinite(v)){free(out);return 0;}
            out[((size_t)block*512+i)*result.channels+channel]=(int16_t)(v>32767?32767:v<-32768?-32768:v);
        }
    }
    *pcm=out;*info=result;return 1;
}
