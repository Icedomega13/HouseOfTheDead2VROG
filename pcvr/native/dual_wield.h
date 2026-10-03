#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace hotd2_input {
struct HandShot {unsigned hand=0;float x=320,y=240;std::uint64_t time=0;};
// One native player/mouse channel: hold an immutable aim/fire pair through the
// cursor/state polls, insert a release presentation, then deliver the next hand.
// Counters describe routed input, not engine-confirmed shots or hits.
struct DualShotRouter {
    std::array<HandShot,16> pending{};unsigned count=0;
    HandShot current{};enum Phase {Idle,Down,Up};Phase phase=Idle;
    bool held[2]={},armed[2]={true,true},valid[2]={};
    bool cursor_polled=false,mouse_polled=false;unsigned tie=0;
    unsigned queued=0,delivered[2]={},discarded=0;
    static constexpr std::uint64_t max_age_ms=250;
    void reset() {count=0;phase=Idle;cursor_polled=mouse_polled=false;for(unsigned h=0;h<2;++h){held[h]=false;armed[h]=false;valid[h]=false;}}
    void remove(unsigned index) {for(unsigned i=index+1;i<count;++i)pending[i-1]=pending[i];--count;}
    void cancel_hand(unsigned hand) {
        for(unsigned i=0;i<count;)if(pending[i].hand==hand)remove(i);else ++i;
        if(phase==Down&&current.hand==hand){phase=Up;cursor_polled=mouse_polled=false;}
        if(hand<2)armed[hand]=false;
    }
    void select(std::uint64_t now) {
        if(phase!=Idle)return;
        while(count) {
            auto shot=pending[0];remove(0);
            if(!valid[shot.hand]||now<shot.time||now-shot.time>max_age_ms){++discarded;continue;}
            current=shot;phase=Down;cursor_polled=mouse_polled=false;return;
        }
    }
    void observe(bool active,const bool tracked[2],const bool trigger[2],const float x[2],const float y[2],std::uint64_t now) {
        if(!active){reset();return;}
        bool edge[2]={};
        for(unsigned h=0;h<2;++h) {
            valid[h]=tracked[h]&&std::isfinite(x[h])&&std::isfinite(y[h])&&x[h]>=0&&x[h]<=640&&y[h]>=0&&y[h]<=480;
            if(!valid[h]){held[h]=false;armed[h]=false;continue;}
            if(!trigger[h])armed[h]=true;
            edge[h]=trigger[h]&&!held[h]&&armed[h];held[h]=trigger[h];
        }
        // Alternate priority for exact ties; keep the other shot's press-time aim.
        for(unsigned i=0;i<2;++i){unsigned h=(tie+i)%2;if(edge[h]){if(count<pending.size()){pending[count++]={h,x[h],y[h],now};++queued;}else ++discarded;}}
        if(edge[0]&&edge[1])tie=1-tie;
        if(phase==Down&&(!valid[current.hand]||now<current.time||now-current.time>max_age_ms)) {phase=Up;cursor_polled=mouse_polled=false;++discarded;}
        select(now);
    }
    void cursor_poll(){if(phase==Down)cursor_polled=true;}
    void mouse_poll(){if(phase!=Idle)mouse_polled=true;}
    void present(std::uint64_t now) {
        if(phase==Down&&cursor_polled&&mouse_polled){++delivered[current.hand];phase=Up;cursor_polled=mouse_polled=false;}
        else if(phase==Up&&mouse_polled){phase=Idle;cursor_polled=mouse_polled=false;select(now);}
        else if(phase==Down&&(now<current.time||now-current.time>max_age_ms)){phase=Up;cursor_polled=mouse_polled=false;++discarded;}
        else select(now);
    }
};
}
