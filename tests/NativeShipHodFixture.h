#pragma once
#include <cstring>
#include <vector>
using Bytes=std::vector<unsigned char>;
static void le(Bytes& b,unsigned x) { for(unsigned i=0;i<4;++i) b.push_back((x>>(8*i))&255); }
static void be(Bytes& b,unsigned x) { for(int i=3;i>=0;--i) b.push_back((x>>(8*i))&255); }
static void str(Bytes& b,const char* s) { le(b,unsigned(std::strlen(s))); b.insert(b.end(),s,s+std::strlen(s)); }
static void f(Bytes& b,float x) { unsigned v; std::memcpy(&v,&x,4); le(b,v); }
static void chunk(Bytes& b,const char* tag,const Bytes& data,bool form=false) {
    const char* outer=form?"FORM":tag; b.insert(b.end(),outer,outer+4); be(b,unsigned(data.size()+(form?4:0)));
    if(form) b.insert(b.end(),tag,tag+4); b.insert(b.end(),data.begin(),data.end());
}
static Bytes sample(bool badIndex=false,bool textured=false,bool effectProxy=false,
    bool neutralTint=false,unsigned tint=0xc8c8c8ff,const Bytes *dock=nullptr,const Bytes *customHierarchy=nullptr) {
    Bytes top,v; le(v,0x200); chunk(top,"VERS",v);
    Bytes vertices,indices;
    for(unsigned part=0;part<3;++part) for(unsigned i=0;i<3;++i) {
        f(vertices,i==1?1.f:0.f); f(vertices,i==2?1.f:0.f); f(vertices,0); f(vertices,1);
        f(vertices,0); f(vertices,0); f(vertices,1); f(vertices,0); le(vertices,neutralTint?tint:0xffffffff);
        f(vertices,float(i)/2); f(vertices,0); f(vertices,0); f(vertices,0);
        for(unsigned k=0;k<6;++k) f(vertices,0);
        indices.push_back((badIndex && i==2)?3:i); indices.push_back(0);
    }
    Bytes pool; le(pool,0); le(pool,textured?4:0); le(pool,textured?4:0);
    if(textured) pool.insert(pool.end(),{10,20,30,255});
    le(pool,unsigned(vertices.size())); le(pool,unsigned(vertices.size())); pool.insert(pool.end(),vertices.begin(),vertices.end());
    le(pool,unsigned(indices.size())); le(pool,unsigned(indices.size())); pool.insert(pool.end(),indices.begin(),indices.end()); chunk(top,"POOL",pool);
    Bytes hvmd;
    if(textured) {
        Bytes image;str(image,"Diffuse");image.insert(image.end(),{'8','8','8','8'});
        le(image,1);le(image,1);le(image,1);chunk(hvmd,"LMIP",image);
    }
    for(const char* shader:{effectProxy?"innateSS":neutralTint?"default":"ship","thruster","badge"}) {
        Bytes m; be(m,1000); str(m,shader); str(m,shader); le(m,textured?2:0);
        if(textured) {
            le(m,5);le(m,4);le(m,0);str(m,"$diffuse");
            le(m,4);le(m,16);for(float value:{.25f,.5f,.75f,1.f}) f(m,value);str(m,"$color");
        }
        chunk(hvmd,"STAT",m);
    }
    Bytes mult; be(mult,1401); str(mult,"HeavyInterceptor"); str(mult,"HeavyInterceptor"); le(mult,1);
    Bytes bm; be(bm,1401); le(bm,0); le(bm,3);
    for(unsigned i=0;i<3;++i) { le(bm,i); le(bm,0x601f); le(bm,3); bm.push_back(255);bm.push_back(255);le(bm,3); }
    chunk(mult,"BMSH",bm,true); chunk(hvmd,"MULT",mult,true);chunk(top,"HVMD",hvmd,true);
    Bytes hierarchy;le(hierarchy,unsigned(-2));
    for(const char* name:{"Root","HeavyInterceptor"}) {str(hierarchy,name);str(hierarchy,std::strcmp(name,"Root")==0?"":"Root");for(unsigned i=0;i<9;++i) f(hierarchy,0);}
    Bytes transform;chunk(transform,"HIER",customHierarchy?*customHierarchy:hierarchy,true);
    if(dock) chunk(transform,"DOCK",*dock,true);
    chunk(top,"DTRM",transform,true);return top;
}
