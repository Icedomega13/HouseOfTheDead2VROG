#include "xr_pixels.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>

static unsigned checks=0;
static void require(bool ok,const char* message) {++checks;if(!ok) {fprintf(stderr,"FAIL %s\n",message);exit(1);}}
static uint8_t legacy(uint32_t pixel,uint32_t mask) {
    if(!mask) return 0;
    unsigned shift=0;while(!(mask&1)) {mask>>=1;++shift;}
    return static_cast<uint8_t>(static_cast<uint64_t>((pixel>>shift)&mask)*255/mask);
}
static void legacy_atlas(const void* pixels,ptrdiff_t pitch,unsigned size,unsigned bits,uint32_t r,uint32_t g,uint32_t b,std::vector<uint8_t> (&rgba)[2]) {
    for(unsigned eye=0;eye<2;++eye) {
        rgba[eye].resize(size*size*4);
        for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
            uint32_t pixel=0;std::memcpy(&pixel,static_cast<const uint8_t*>(pixels)+y*pitch+(x+eye*size)*(bits/8),bits/8);
            auto out=rgba[eye].data()+(y*size+x)*4;
            out[0]=legacy(pixel,r);out[1]=legacy(pixel,g);out[2]=legacy(pixel,b);out[3]=255;
        }
    }
}
int main(int argc,char**) {
    using namespace hotd2_xr; EyeAtlasConverter converter;
    for(uint32_t mask:{0xf800u,0x7e0u,0x1fu,0x7c00u,0x3e0u}) {
        PixelChannel channel(mask);bool exact=true;
        for(unsigned pixel=0;pixel<65536;++pixel) exact&=channel.decode(pixel)==legacy(pixel,mask);
        require(exact,"exhaustive 16-bit channel equivalence");
    }
    for(uint32_t mask:{0xff0000u,0xff00u,0xffu,0x3ffu,0u}) {
        PixelChannel channel(mask);bool exact=true;uint32_t pixel=123456789;
        for(unsigned i=0;i<10000;++i) {pixel=pixel*1664525u+1013904223u;exact&=channel.decode(pixel)==legacy(pixel,mask);}
        require(exact,"32-bit and wide/empty channel equivalence");
    }
    // Two 2x2 eye images in one padded atlas, left/right and top/bottom colours distinct.
    uint16_t source[]={0xf800,0x07e0,0x001f,0xffff,0x9999,0x9999,0,0x8410,0xffe0,0xf81f,0x9999,0x9999};
    auto copy=std::vector<uint16_t>(source,source+12);
    std::vector<uint8_t> rgba[2],expected[2];
    require(converter.convert(source,12,2,16,0xf800,0x7e0,0x1f,rgba),"padded RGB565 atlas conversion");
    legacy_atlas(source,12,2,16,0xf800,0x7e0,0x1f,expected);
    require(rgba[0]==expected[0]&&rgba[1]==expected[1],"both eye crops, pitch, alpha and colours unchanged");
    require(rgba[0][0]==255&&rgba[1][2]==255,"left and right eyes stay in order");
    require(std::memcmp(source,copy.data(),sizeof(source))==0,"conversion preserves native source pixels");
    auto allocation=rgba[0].data();converter.convert(source,12,2,16,0xf800,0x7e0,0x1f,rgba);
    require(allocation==rgba[0].data(),"conversion reuses eye storage");
    require(converter.convert(source+6,-12,2,16,0xf800,0x7e0,0x1f,rgba),"negative pitch supported");
    legacy_atlas(source+6,-12,2,16,0xf800,0x7e0,0x1f,expected);
    require(rgba[0]==expected[0]&&rgba[1]==expected[1],"negative pitch preserves row order");
    require(converter.convert(source,12,2,16,0x1f,0x7e0,0xf800,rgba),"changed 16-bit channel masks rebuild lookup");
    legacy_atlas(source,12,2,16,0x1f,0x7e0,0xf800,expected);
    require(rgba[0]==expected[0]&&rgba[1]==expected[1],"rebuilt lookup has exact swapped channels");
    uint32_t source32[]={0x123456,0x789abc,0xdef012,0x345678};
    require(converter.convert(source32,8,1,32,0xff0000,0xff00,0xff,rgba),"32-bit atlas conversion");
    require(rgba[0]==std::vector<uint8_t>({0x12,0x34,0x56,255})&&rgba[1]==std::vector<uint8_t>({0x78,0x9a,0xbc,255}),"32-bit BGR to RGBA exact");
    require(!converter.convert(source,2,2,16,0xf800,0x7e0,0x1f,rgba),"short pitch rejected");
    require(!converter.convert(source,12,2,24,0xff0000,0xff00,0xff,rgba),"unsupported format rejected");
    require(!converter.convert(nullptr,12,2,16,0xf800,0x7e0,0x1f,rgba),"missing source rejected");
    printf("PASS %u eye pixel-transfer checks\n",checks);
    if(argc>1) {
        constexpr unsigned size=800;std::vector<uint16_t> atlas(size*size*2);
        for(unsigned i=0;i<atlas.size();++i) atlas[i]=static_cast<uint16_t>(i*31);
        std::vector<uint8_t> optimized[2],original[2];
        auto measure=[&](bool old) {
            auto start=std::chrono::steady_clock::now();
            for(unsigned i=0;i<30;++i) {
                if(old) legacy_atlas(atlas.data(),size*4,size,16,0xf800,0x7e0,0x1f,original);
                else converter.convert(atlas.data(),size*4,size,16,0xf800,0x7e0,0x1f,optimized);
            }
            return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
        };
        double before=measure(true),after=measure(false);
        require(original[0]==optimized[0]&&original[1]==optimized[1],"full-size benchmark images match");
        printf("CPU conversion only, synthetic 1600x800 RGB565: legacy=%.3f ms optimized=%.3f ms; no headset latency measurement\n",before,after);
    }
    return 0;
}
