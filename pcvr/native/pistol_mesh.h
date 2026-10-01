#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

// Original authored AMS-inspired pistol. Metres, +Z bore, +Y up; the existing
// controller aim matrix, muzzle reach and sight height remain unchanged.
namespace hotd2_pistol {
struct Vertex {float x,y,z;uint32_t color;};
struct Point {float x,y,z;};
struct Profile {float y,z;};
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
class Builder {
public:
    std::vector<Vertex> vertices;
    void triangle(Point a,Point b,Point c,uint32_t color,Point outward={0,0,0}) {
        Point normal=cross(sub(b,a),sub(c,a));float length=std::sqrt(dot(normal,normal));
        if(length<1e-10f) return;
        if(dot(normal,outward)<0){std::swap(b,c);normal={-normal.x,-normal.y,-normal.z};}
        // Fixed face lighting gives faceted, baked-looking silver instead of
        // a modern shiny PBR material, and is independent of native game lights.
        float light=std::max(0.0f,(-.35f*normal.x+.82f*normal.y-.45f*normal.z)/length);
        unsigned shade=static_cast<unsigned>(145+110*std::min(light,1.0f));
        uint32_t shaded=0xff000000u|(((color>>16)&255)*shade/255<<16)|(((color>>8)&255)*shade/255<<8)|((color&255)*shade/255);
        for(auto p:{a,b,c}) vertices.push_back({p.x,p.y,p.z,shaded});
    }
    void quad(Point a,Point b,Point c,Point d,uint32_t color,Point outward={0,0,0}) {
        triangle(a,b,c,color,outward);triangle(a,c,d,color,outward);
    }
    void prism(float x0,float x1,std::vector<Profile> outline,uint32_t color) {
        float area=0;for(size_t i=0;i<outline.size();++i){auto a=outline[i],b=outline[(i+1)%outline.size()];area+=a.y*b.z-b.y*a.z;}
        if(area<0) std::reverse(outline.begin(),outline.end());
        for(size_t i=1;i+1<outline.size();++i) {
            triangle({x0,outline[0].y,outline[0].z},{x0,outline[i].y,outline[i].z},{x0,outline[i+1].y,outline[i+1].z},color,{-1,0,0});
            triangle({x1,outline[0].y,outline[0].z},{x1,outline[i].y,outline[i].z},{x1,outline[i+1].y,outline[i+1].z},color,{1,0,0});
        }
        for(size_t i=0;i<outline.size();++i) {
            auto a=outline[i],b=outline[(i+1)%outline.size()];
            quad({x0,a.y,a.z},{x0,b.y,b.z},{x1,b.y,b.z},{x1,a.y,a.z},color,{0,b.z-a.z,a.y-b.y});
        }
    }
    void box(float x0,float y0,float z0,float x1,float y1,float z1,uint32_t color) {
        prism(x0,x1,{{y0,z0},{y1,z0},{y1,z1},{y0,z1}},color);
    }
    void guard() {
        constexpr unsigned count=16;constexpr float pi=3.14159265358979323846f;
        for(unsigned i=0;i<count;++i) {
            float a=i*2*pi/count,b=(i+1)*2*pi/count;
            Profile outer0={-.039f+.022f*std::sin(a),.046f+.034f*std::cos(a)};
            Profile outer1={-.039f+.022f*std::sin(b),.046f+.034f*std::cos(b)};
            Profile inner0={-.039f+.0155f*std::sin(a),.046f+.025f*std::cos(a)};
            Profile inner1={-.039f+.0155f*std::sin(b),.046f+.025f*std::cos(b)};
            for(float x:{-.009f,.009f}) quad({x,outer0.y,outer0.z},{x,outer1.y,outer1.z},{x,inner1.y,inner1.z},{x,inner0.y,inner0.z},0xff3c4045,{x,0,0});
            quad({-.009f,outer0.y,outer0.z},{.009f,outer0.y,outer0.z},{.009f,outer1.y,outer1.z},{-.009f,outer1.y,outer1.z},0xff45494e,{0,std::sin(a),std::cos(a)});
            quad({-.009f,inner0.y,inner0.z},{.009f,inner0.y,inner0.z},{.009f,inner1.y,inner1.z},{-.009f,inner1.y,inner1.z},0xff292d32,{0,-std::sin(a),-std::cos(a)});
        }
    }
    void slide() {
        struct Section {float z,width,lo,hi,bevel;};
        Section sections[]={{-.040f,.016f,-.014f,.019f,.004f},{-.030f,.020f,-.017f,.022f,.005f},
            {.160f,.020f,-.017f,.022f,.005f},{.172f,.016f,-.013f,.017f,.003f}};
        std::array<Point,8> previous={};
        for(unsigned s=0;s<4;++s) {
            auto p=sections[s];float w=p.width,b=p.bevel;
            std::array<Point,8> ring={Point{-w+b,p.lo,p.z},{w-b,p.lo,p.z},{w,p.lo+b,p.z},{w,p.hi-b,p.z},
                {w-b,p.hi,p.z},{-w+b,p.hi,p.z},{-w,p.hi-b,p.z},{-w,p.lo+b,p.z}};
            if(s==0||s==3) for(unsigned i=0;i<8;++i) triangle({0,(p.lo+p.hi)*.5f,p.z},ring[i],ring[(i+1)%8],0xffa8afb5,{0,0,s==0?-1.0f:1.0f});
            if(s) for(unsigned i=0;i<8;++i) quad(previous[i],previous[(i+1)%8],ring[(i+1)%8],ring[i],0xffb6bec4,
                {ring[i].x+ring[(i+1)%8].x,ring[i].y+ring[(i+1)%8].y-.005f,0});
            previous=ring;
        }
    }
    void bore() {
        constexpr unsigned n=12;constexpr float pi=3.14159265358979323846f;
        for(unsigned i=0;i<n;++i) {
            float a=i*2*pi/n,b=(i+1)*2*pi/n;
            Point a0={.0068f*std::cos(a),.0068f*std::sin(a),.172f},b0={.0068f*std::cos(b),.0068f*std::sin(b),.172f};
            Point a1=a0,b1=b0;a1.z=b1.z=.18f;
            Point ia={.0045f*std::cos(a),.0045f*std::sin(a),.1802f},ib={.0045f*std::cos(b),.0045f*std::sin(b),.1802f};
            quad(a0,b0,b1,a1,0xff818b94,{std::cos(a),std::sin(a),0});
            quad(a1,b1,ib,ia,0xffa4adb3,{0,0,1});
            Point rear_a=ia,rear_b=ib;rear_a.z=rear_b.z=.1725f;
            quad(ia,ib,rear_b,rear_a,0xff12161c,{-std::cos(a),-std::sin(a),0});
            triangle({0,0,.1723f},rear_a,rear_b,0xff080a0c,{0,0,1});
        }
    }
};
inline std::vector<Vertex> build() {
    Builder m;m.vertices.reserve(6000);
    m.slide();m.bore();
    m.prism(-.017f,.017f,{{-.016f,-.038f},{-.014f,.134f},{-.023f,.141f},{-.029f,.111f},{-.028f,.023f},{-.038f,-.039f}},0xff42474d);
    m.prism(-.015f,.015f,{{-.023f,-.038f},{-.024f,.012f},{-.101f,-.015f},{-.105f,-.060f},{-.093f,-.067f}},0xff282b30);
    m.prism(-.0165f,.0165f,{{-.030f,-.036f},{-.033f,.005f},{-.090f,-.015f},{-.094f,-.055f}},0xff34383c);
    m.guard();
    m.prism(-.003f,.003f,{{-.021f,.028f},{-.028f,.029f},{-.039f,.036f},{-.049f,.044f},{-.050f,.049f},{-.039f,.042f},{-.027f,.035f}},0xff8c9296);
    // Separate magazine heel, visible slide serrations and grip ribs.
    m.prism(-.0165f,.0165f,{{-.100f,-.059f},{-.105f,-.060f},{-.106f,-.014f},{-.100f,-.013f}},0xff5a6065);
    for(unsigned i=0;i<7;++i) {
        float z=-.027f+i*.004f;
        for(float side:{-1.0f,1.0f}) m.box(side<0?-.0203f:.020f,-.009f,z,side<0?-.020f:.0203f,.014f,z+.0014f,0xff59636d);
    }
    for(unsigned i=0;i<8;++i) {
        float y=-.042f-i*.006f,z=-.038f-i*.0018f;
        for(float side:{-1.0f,1.0f}) m.box(side<0?-.0168f:.0165f,y,z,side<0?-.0165f:.0168f,y+.0018f,z+.027f,0xff21252a);
    }
    // Ejection-port recess, safety lever and exposed hammer are geometry details.
    m.box(-.007f,.0221f,.035f,.010f,.0224f,.064f,0xff242a30);
    m.box(-.014f,.014f,.036f,-.012f,.020f,.064f,0xff79838c);
    m.box(-.021f,-.012f,-.030f,-.0203f,-.007f,-.010f,0xff8d969e);
    m.prism(-.006f,.006f,{{.004f,-.040f},{.012f,-.045f},{.014f,-.051f},{.009f,-.054f},{0,-.044f}},0xff555e67);
    // Keep a clear rear notch and the accepted three-dot aiming references.
    m.box(-.013f,.022f,-.033f,-.005f,.027f,-.025f,0xff1e252c);
    m.box(.005f,.022f,-.033f,.013f,.027f,-.025f,0xff1e252c);
    m.box(-.011f,.023f,-.0333f,-.008f,.0255f,-.033f,0xff00e848);
    m.box(.008f,.023f,-.0333f,.011f,.0255f,-.033f,0xff00e848);
    m.box(-.003f,.017f,.147f,.003f,.025f,.155f,0xff182028);
    m.box(-.0017f,.022f,.1467f,.0017f,.0245f,.147f,0xff00ff40);
    return std::move(m.vertices);
}
inline const std::vector<Vertex>& mesh(){static const auto data=build();return data;}
}
