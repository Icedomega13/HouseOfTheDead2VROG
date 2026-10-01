#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

namespace hotd2_xr {
// Prepare channel decoding once per locked atlas, instead of finding each mask's
// shift millions of times per frame. Keep the original integer scaling exactly.
struct PixelChannel {
    uint32_t mask=0,maximum=0;
    unsigned shift=0;
    uint8_t values[256]={};
    explicit PixelChannel(uint32_t bits):mask(bits),maximum(bits) {
        if(!bits) return;
        while(!(maximum&1)) {maximum>>=1;++shift;}
        if(maximum<256) for(unsigned i=0;i<=maximum;++i) values[i]=static_cast<uint8_t>(i*255/maximum);
    }
    uint8_t decode(uint32_t pixel) const {
        if(!mask) return 0;
        auto value=(pixel&mask)>>shift;
        return maximum<256?values[value]:static_cast<uint8_t>(static_cast<uint64_t>(value)*255/maximum);
    }
};
class EyeAtlasConverter {
    uint32_t cached_red=0,cached_green=0,cached_blue=0;
    std::vector<uint32_t> table16;
public:
bool convert(const void* pixels,ptrdiff_t pitch,unsigned size,unsigned bits,
    uint32_t red,uint32_t green,uint32_t blue,std::vector<uint8_t> (&rgba)[2]) {
    if(!pixels||!size||size>1600||(bits!=16&&bits!=32)||!red||!green||!blue) return false;
    unsigned bytes=bits/8;
    if(pitch<static_cast<ptrdiff_t>(size*2*bytes)&&pitch>-static_cast<ptrdiff_t>(size*2*bytes)) return false;
    PixelChannel r(red),g(green),b(blue);
    if(bits==16&&(table16.empty()||red!=cached_red||green!=cached_green||blue!=cached_blue)) {
        table16.resize(65536);
        for(unsigned pixel=0;pixel<65536;++pixel)
            table16[pixel]=r.decode(pixel)|(static_cast<uint32_t>(g.decode(pixel))<<8)|(static_cast<uint32_t>(b.decode(pixel))<<16)|0xff000000u;
        cached_red=red;cached_green=green;cached_blue=blue;
    }
    for(unsigned eye=0;eye<2;++eye) {
        // Retain capacity between frames, including the normal 800-square case.
        rgba[eye].resize(static_cast<size_t>(size)*size*4);
        for(unsigned y=0;y<size;++y) {
            auto source=static_cast<const uint8_t*>(pixels)+static_cast<ptrdiff_t>(y)*pitch+eye*size*bytes;
            auto output=rgba[eye].data()+static_cast<size_t>(y)*size*4;
            for(unsigned x=0;x<size;++x) {
                if(bits==16) {
                    uint16_t pixel;std::memcpy(&pixel,source+x*2,2);
                    std::memcpy(output+x*4,&table16[pixel],4);
                    continue;
                }
                uint32_t pixel=0;std::memcpy(&pixel,source+x*bytes,bytes);
                output[x*4]=r.decode(pixel);output[x*4+1]=g.decode(pixel);output[x*4+2]=b.decode(pixel);output[x*4+3]=255;
            }
        }
    }
    return true;
}
};
}
