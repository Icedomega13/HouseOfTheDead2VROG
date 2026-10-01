#include "reload_gesture.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static unsigned checks=0;
static void require(bool ok,const char* label) {++checks;if(!ok) {fprintf(stderr,"FAIL %s\n",label);exit(1);}}
int main() {
    using hotd2_xr::DownReloadGesture;
    constexpr float down=-.819152f,up=-.573576f;
    DownReloadGesture g;
    require(!g.update(true,-1,0,down,up),"starting pointed down does not reload");
    require(!g.update(true,-1,1000000000,down,up),"holding initial down does not reload");
    require(!g.update(true,0,1100000000,down,up)&&g.armed,"raising arms gesture");
    require(!g.update(true,-.7f,1200000000,down,up),"ordinary downward aim does not reload");
    require(!g.update(true,-.9f,1300000000,down,up),"brief dip starts dwell without reload");
    require(!g.update(true,-.7f,1340000000,down,up),"leaving down threshold cancels dwell");
    require(!g.update(true,-.9f,1350000000,down,up),"new dip starts new dwell");
    require(!g.update(true,-.9f,1429999999,down,up),"dwell shorter than 80 ms rejected");
    require(g.update(true,-.9f,1430000000,down,up)&&g.triggered,"80 ms dip triggers exactly once");
    require(g.update(true,-1,1500000000,down,up)&&!g.triggered,"120 ms reload pulse persists across polls");
    require(!g.update(true,-1,1550000000,down,up),"reload pulse releases at 120 ms");
    require(!g.update(true,-1,3000000000,down,up)&&!g.armed,"holding down does not repeat");
    require(!g.update(true,-.6f,3100000000,down,up)&&!g.armed,"partial raise does not rearm");
    require(!g.update(true,0,3200000000,down,up)&&g.armed,"full raise rearms");
    g.update(true,-1,3300000000,down,up);
    require(g.update(true,-1,3380000000,down,up)&&g.triggered,"second deliberate dip reloads");
    g.update(true,0,3520000000,down,up);
    require(!g.armed,"quick raise during cooldown does not rearm");
    require(!g.update(true,0,3880000000,down,up)&&g.armed,"500 ms cooldown allows rearm");
    g.update(true,-1,4000000000,down,up);
    require(!g.update(false,-1,4100000000,down,up)&&!g.armed,"tracking loss cancels pending gesture");
    require(!g.update(true,-1,4300000000,down,up),"tracking recovery pointed down requires raise");
    g.update(true,0,4400000000,down,up);g.update(true,-1,4500000000,down,up);
    require(!g.update(true,NAN,4600000000,down,up)&&!g.armed,"invalid direction cancels gesture");
    g.update(true,0,4700000000,down,up);g.update(true,-1,4800000000,down,up);
    require(!g.update(true,-1,4600000000,down,up)&&!g.armed,"backward runtime clock cancels gesture");
    // Same dwell works at 72, 90 and 120 Hz, independent of a fixed frame count.
    for(int hz:{72,90,120}) {
        g.reset();g.update(true,0,0,down,up);bool fired=false;
        for(int frame=1;frame<=hz;++frame) if(g.update(true,-1,static_cast<int64_t>(frame)*1000000000/hz,down,up)&&g.triggered) fired=true;
        require(fired&&!g.armed,"refresh-rate independent reload");
    }
    printf("PASS %u aim-down reload gesture checks\n",checks);return 0;
}
