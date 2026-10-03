#pragma once
#include <algorithm>
#include <cstdint>

namespace hotd2_hud {
template<class Rect> void health_meter(int current,int maximum,Rect rect){
    if(maximum<1||maximum>9||current<0||current>maximum)return;
    const float x=-.75f,y=.315f;
    const uint32_t color=current<=1?0xffff7040u:current*2<=maximum?0xffffc04du:0xff58e89au;
    auto box=[&](float a,float b,float w,float h,uint32_t c){rect(x+a,y+b,x+a+w,y+b+h,c);};
    box(0,0,.32f,.10f,0xa8101820);box(0,0,.32f,.004f,color);
    // Medical cross and exact health number, followed by one bar cell per hit.
    box(.015f,.045f,.029f,.009f,color);box(.025f,.035f,.009f,.029f,color);
    const unsigned masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    const float segments[7][4]={{0,0,.032f,.005f},{.027f,0,.005f,.029f},{.027f,.027f,.005f,.029f},
        {0,.051f,.032f,.005f},{0,.027f,.005f,.029f},{0,0,.005f,.029f},{0,.025f,.032f,.005f}};
    for(unsigned s=0;s<7;++s)if(masks[current]&(1u<<s))box(.056f+segments[s][0],.025f+segments[s][1],segments[s][2],segments[s][3],color);
    const float step=.195f/maximum;
    for(int slot=0;slot<maximum;++slot)box(.109f+slot*step,.035f,step-.004f,.035f,slot<current?color:0xff394654u);
}
// Head-relative tangent coordinates, on a stereo plane two metres away.
// Emit compact vector artwork without textures, font files or frame allocations.
template<class Rect> void ammo_gauge(unsigned hand,int rounds,Rect rect) {
    const float x=hand?-.75f:.43f,y=.43f;
    rounds=std::clamp(rounds,0,6);
    const uint32_t accent=rounds?(hand?0xff00c8ffu:0xff00ff40u):0xffff7040u;
    auto box=[&](float a,float b,float w,float h,uint32_t color){rect(x+a,y+b,x+a+w,y+b+h,color);};
    box(0,0,.32f,.14f,0xa8101820);
    box(0,0,.32f,.004f,accent);
    // L/R hand label, separate from the seven-segment round count.
    box(.014f,.017f,.003f,.021f,accent);
    if(hand)box(.014f,.035f,.014f,.003f,accent);
    else {
        box(.014f,.017f,.013f,.003f,accent);box(.025f,.017f,.003f,.010f,accent);
        box(.014f,.025f,.013f,.003f,accent);box(.022f,.028f,.003f,.004f,accent);
        box(.025f,.032f,.003f,.006f,accent);
    }
    const unsigned masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d};
    const float segments[7][4]={{0,0,.032f,.005f},{.027f,0,.005f,.029f},{.027f,.027f,.005f,.029f},
        {0,.051f,.032f,.005f},{0,.027f,.005f,.029f},{0,0,.005f,.029f},{0,.025f,.032f,.005f}};
    for(unsigned s=0;s<7;++s)if(masks[rounds]&(1u<<s))
        box(.015f+segments[s][0],.058f+segments[s][1],segments[s][2],segments[s][3],accent);
    for(int slot=0;slot<6;++slot) {
        const uint32_t color=slot<rounds?accent:0xff394654u;
        const float a=.076f+slot*.038f;
        box(a,.065f,.023f,.047f,color);box(a+.004f,.056f,.015f,.009f,color);
    }
}
}
