#pragma once
#include <cstdint>

namespace hotd2_input {
// Counts come from the engine's ammo update, never from routed trigger presses.
struct IndependentMagazines {
    int rounds[2]={6,6},selected=0,last_native=-1,last_lives=-1;
    short empty_prompt[2]={},prompt_ticks[2]={};
    bool ready=false;
    unsigned consumed[2]={},reloads[2]={},prevented_refills=0;
    void invalidate(){ready=false;last_native=last_lives=-1;}
    bool begin(int native,int lives,int hand,unsigned reload_mask) {
        if(native<0||native>6||lives<0||lives>99){invalidate();return false;}
        if(!ready||lives!=last_lives){rounds[0]=rounds[1]=6;selected=0;rounds[0]=native;ready=true;for(unsigned h=0;h<2;++h){empty_prompt[h]=rounds[h]?0:1;prompt_ticks[h]=0;}}
        else if(native!=last_native){rounds[selected]=native;empty_prompt[selected]=native?0:1;prompt_ticks[selected]=0;} // Native lifecycle changes outside this call.
        last_lives=lives;
        for(unsigned h=0;h<2;++h)if(reload_mask&(1u<<h)){rounds[h]=6;empty_prompt[h]=prompt_ticks[h]=0;++reloads[h];}
        if(hand>=0&&hand<2)selected=hand;
        return true;
    }
    int finish(int native) {
        int before=rounds[selected];
        if(native>=0&&native<before){consumed[selected]+=static_cast<unsigned>(before-native);rounds[selected]=native;}
        else if(native>before)++prevented_refills; // Native automatic reload cannot refill a lowered/idle hand.
        last_native=rounds[selected];return last_native;
    }
};
}
