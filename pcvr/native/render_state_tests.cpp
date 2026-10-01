#include "render_state.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

static unsigned checks=0,failures=0;
static std::string diagnostics;
static void log_error(const char* format,...) {
    char message[256];va_list args;va_start(args,format);
    vsnprintf(message,sizeof(message),format,args);va_end(args);
    diagnostics+=message;diagnostics+='\n';
}
static void check(bool condition,const char* name) {
    ++checks;if(!condition) {++failures;std::printf("FAIL %s\n",name);}
}
struct Surface {
    unsigned references=1;
    ULONG Release() {return --references;}
};
struct Device {
    Surface desktop,atlas;
    Surface* target=&desktop;
    D3DVIEWPORT7 viewport={7,11,640,480,.1f,.9f};
    D3DVIEWPORT7 captured={};
    int state=42,saved_state=0;
    unsigned blocks=0,target_calls=0,viewport_calls=0;
    const char* fail=nullptr;
    bool null_target=false;
    std::string operations;
    bool failing(const char* operation) const {return fail&&std::strcmp(fail,operation)==0;}
    HRESULT GetViewport(D3DVIEWPORT7* out) {
        if(failing("capture_viewport")) return E_FAIL;*out=viewport;return S_OK;
    }
    HRESULT GetRenderTarget(Surface** out) {
        if(failing("capture_target")) return E_FAIL;
        if(null_target) {*out=nullptr;return S_OK;}
        *out=target;++target->references;return S_OK;
    }
    HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE,DWORD* out) {
        if(failing("capture_state")) return E_FAIL;
        *out=123;captured=viewport;saved_state=state;++blocks;return S_OK;
    }
    HRESULT SetRenderTarget(Surface* destination,DWORD) {
        ++target_calls;const char* operation=destination==&atlas?"bind_target":"restore_target";
        operations+=operation;operations+=';';
        if(failing(operation)) return E_FAIL;
        target=destination;viewport={0,0,1600,800,0,1};return S_OK;
    }
    HRESULT ApplyStateBlock(DWORD) {
        operations+="restore_state;";
        if(failing("restore_state")) return E_FAIL;
        state=saved_state;viewport=captured;return S_OK;
    }
    HRESULT SetViewport(D3DVIEWPORT7* source) {
        ++viewport_calls;operations+="restore_viewport;";
        if(failing("restore_viewport")) return E_FAIL;
        viewport=*source;return S_OK;
    }
    HRESULT DeleteStateBlock(DWORD) {
        operations+="delete_state;";--blocks;
        return failing("delete_state")?E_FAIL:S_OK;
    }
};
using Saved=hotd2_render::SavedState<Device,Surface>;
static bool original_viewport(const Device& device) {
    return device.viewport.dwX==7&&device.viewport.dwY==11&&device.viewport.dwWidth==640&&
        device.viewport.dwHeight==480&&device.viewport.dvMinZ==.1f&&device.viewport.dvMaxZ==.9f;
}
static void draw_and_return(Device& device) {
    Saved saved(&device,true,log_error,"early_return");
    if(!saved.bind(&device.atlas)) return;
    device.state=99;device.viewport={0,0,800,800,0,1};
    return;
}
int main() {
    {
        Device device;draw_and_return(device);
        check(device.target==&device.desktop,"early return restores target");
        check(original_viewport(device),"early return restores viewport");
        check(device.state==42,"early return restores full render state");
        check(device.operations=="bind_target;restore_target;restore_state;restore_viewport;delete_state;","restoration order");
        check(device.blocks==0&&device.desktop.references==1,"successful draw balances resources");
    }
    {
        Device device;
        {Saved saved(&device,false,log_error,"clear");check(saved.bind(&device.atlas),"clear binds atlas");}
        check(device.target==&device.desktop&&original_viewport(device),"clear restores target and viewport");
        check(device.operations=="bind_target;restore_target;restore_viewport;","clear has no full state block");
        check(device.desktop.references==1&&device.blocks==0,"clear balances resources");
    }
    for(const char* failure:{"capture_viewport","capture_target","capture_state","bind_target"}) {
        Device device;device.fail=failure;diagnostics.clear();
        {Saved saved(&device,true,log_error,"failure");check(!saved.bind(&device.atlas),"failed capture or bind skips draw");}
        check(device.target==&device.desktop&&original_viewport(device)&&device.state==42,"failure preserves native state");
        check(device.desktop.references==1&&device.blocks==0,"failure balances captured resources");
        check(diagnostics.find(failure)!=std::string::npos,"failure names operation in diagnostics");
        check(device.viewport_calls==0,"failed draw does not restore uncaptured viewport");
    }
    {
        Device device;device.null_target=true;
        {Saved saved(&device,true,log_error,"null_target");check(!saved.bind(&device.atlas),"null native target skips draw");}
        check(device.target_calls==0&&device.blocks==0,"null target does not mutate state");
    }
    {
        Device device;{Saved saved(&device,true,log_error,"unused");}
        check(device.target_calls==0&&device.viewport_calls==0,"unused guard leaves device untouched");
        check(device.desktop.references==1&&device.blocks==0,"unused guard balances resources");
    }
    for(const char* failure:{"restore_target","restore_state","restore_viewport","delete_state"}) {
        Device device;diagnostics.clear();
        {Saved saved(&device,true,log_error,"restore_failure");saved.bind(&device.atlas);device.fail=failure;}
        check(diagnostics.find(failure)!=std::string::npos,"restore failure names operation");
        check(device.desktop.references==1&&device.blocks==0,"restore failure still releases resources");
        check(device.operations.find("restore_viewport;delete_state;")!=std::string::npos,"restore failure continues remaining cleanup");
    }
    std::printf("render_state_tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
