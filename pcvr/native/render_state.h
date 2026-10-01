#pragma once
#include <d3d.h>

namespace hotd2_render {
using Log=void(*)(const char*,...);
// Capture before changing any native state. Restore the original target before
// applying a state block, then its viewport last (target switches may reset it).
template<class Device,class Surface> class SavedState {
    Device* device;
    Surface* target=nullptr;
    D3DVIEWPORT7 viewport={};
    DWORD block=0;
    bool ready=false,has_block=false,changed=false;
    Log log;
    const char* label;
    void report(HRESULT hr,const char* operation) {
        if(FAILED(hr)&&log) log("RENDER_STATE context=%s operation=%s hr=%08lx",label,operation,hr);
    }
public:
    SavedState(Device* source,bool all_state,Log logger,const char* context):device(source),log(logger),label(context) {
        HRESULT hr=device->GetViewport(&viewport);
        if(FAILED(hr)) {report(hr,"capture_viewport");return;}
        hr=device->GetRenderTarget(&target);
        if(FAILED(hr)||!target) {report(FAILED(hr)?hr:E_FAIL,"capture_target");return;}
        if(all_state) {
            hr=device->CreateStateBlock(D3DSBT_ALL,&block);
            if(FAILED(hr)) {report(hr,"capture_state");return;}
            has_block=true;
        }
        ready=true;
    }
    SavedState(const SavedState&)=delete;
    SavedState& operator=(const SavedState&)=delete;
    bool bind(Surface* destination) {
        if(!ready) return false;
        HRESULT hr=device->SetRenderTarget(destination,0);
        if(FAILED(hr)) {report(hr,"bind_target");return false;}
        changed=true;return true;
    }
    ~SavedState() {
        if(changed) {
            report(device->SetRenderTarget(target,0),"restore_target");
            if(has_block) report(device->ApplyStateBlock(block),"restore_state");
            report(device->SetViewport(&viewport),"restore_viewport");
        }
        if(has_block) report(device->DeleteStateBlock(block),"delete_state");
        if(target) target->Release();
    }
};
}
