#pragma once
#include <cstdint>
#include <cmath>

namespace hotd2_xr {
// Gravity-relative aim gesture. Timings use the runtime's predicted display
// clock, not frame counts, so the gesture works at different headset refresh rates.
struct DownReloadGesture {
    bool armed=false,dwelling=false,has_time=false,has_reloaded=false,triggered=false;
    int64_t down_started=0,last_time=0,last_reload=0;
    void reset() {*this=DownReloadGesture{};}
    bool update(bool tracked,float direction_y,int64_t time,float down_y,float rearm_y) {
        triggered=false;
        if(!tracked||!std::isfinite(direction_y)||(has_time&&time<last_time)) {reset();return false;}
        has_time=true;last_time=time;
        bool cooled=!has_reloaded||time-last_reload>=500000000;
        if(direction_y>rearm_y) {
            if(cooled) armed=true;
            dwelling=false;
        }
        if(armed&&direction_y<=down_y) {
            if(!dwelling) {dwelling=true;down_started=time;}
            if(time-down_started>=80000000&&cooled) {
                armed=false;dwelling=false;has_reloaded=true;last_reload=time;triggered=true;
            }
        } else dwelling=false;
        // A short mouse-button pulse spans several input polls; holding the gun
        // down never generates another pulse until it has been raised again.
        return has_reloaded&&time-last_reload<120000000;
    }
};
}
