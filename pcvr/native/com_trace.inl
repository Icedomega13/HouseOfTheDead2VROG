#include "vtable_hooks.inl"
static void trace_draw7(void*);
static void trace_d3d7(void*);
static void trace_device7(void*);
static IDirect3DDevice7* last_rendering_device=nullptr; // Borrowed, used only at the subsequent presentation.
#include "eye_targets.inl"
#include "input_bridge.inl"
#include "native_ammo.inl"
#include "native_status.inl"
#include "timing_research.inl"
#include "texture_pack.inl"
#include "hud_prompt.inl"
static void update_cinematic_guard();
static volatile LONG presentation_count=0;
static HRESULT WINAPI trace_flip(void* object,LPDIRECTDRAWSURFACE7 target,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,LPDIRECTDRAWSURFACE7,DWORD);
    update_cinematic_guard();submit_xr_frame(last_rendering_device,eye_atlas);
    xr_native_present();native_ammo_present();native_health_present();
    timing_research_present(last_rendering_device);
    LONG present=InterlockedIncrement(&presentation_count);
    if(present<=3||present%120==0) log_line("PRESENT flip=%ld",present);
    return original_slot<Fn>(object,11)(object,target,timing_research_flip_flags(flags));
}
static HRESULT WINAPI trace_blt(void* object,LPRECT dest,LPDIRECTDRAWSURFACE7 source,LPRECT src,DWORD flags,LPDDBLTFX fx) {
    using Fn=HRESULT(WINAPI*)(void*,LPRECT,LPDIRECTDRAWSURFACE7,LPRECT,DWORD,LPDDBLTFX);
    DDSCAPS2 caps={};
    if(SUCCEEDED(static_cast<IDirectDrawSurface7*>(object)->GetCaps(&caps))&&(caps.dwCaps&DDSCAPS_PRIMARYSURFACE)) {
        update_cinematic_guard();submit_xr_frame(last_rendering_device,eye_atlas);
        xr_native_present();native_ammo_present();native_health_present();
        timing_research_present(last_rendering_device);
        LONG present=InterlockedIncrement(&presentation_count);
        if(present<=3||present%120==0) log_line("PRESENT blt=%ld",present);
    }
    return original_slot<Fn>(object,5)(object,dest,source,src,flags,fx);
}
static HRESULT WINAPI trace_surface(void* object,LPDDSURFACEDESC2 desc,LPDIRECTDRAWSURFACE7* out,IUnknown* outer) {
    using Fn=HRESULT(WINAPI*)(void*,LPDDSURFACEDESC2,LPDIRECTDRAWSURFACE7*,IUnknown*);
    HRESULT hr=original_slot<Fn>(object,6)(object,desc,out,outer);
    if(SUCCEEDED(hr)&&out&&*out&&desc&&(desc->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)) {
        install_hook(*out,11,reinterpret_cast<void*>(&trace_flip));
        install_hook(*out,5,reinterpret_cast<void*>(&trace_blt));
        log_line("Presentation hooks installed on primary surface=%p",*out);
    }
    return hr;
}
static HRESULT WINAPI trace_query(void* object,REFIID iid,void** out) {
    using Fn=HRESULT(WINAPI*)(void*,REFIID,void**);
    HRESULT hr=original_slot<Fn>(object,0)(object,iid,out);
    log_line("QueryInterface object=%p iid=%08lx-%04x-%04x hr=%08lx result=%p",object,
        iid.Data1,iid.Data2,iid.Data3,hr,SUCCEEDED(hr)&&out?*out:nullptr);
    if(SUCCEEDED(hr)&&out&&*out) {
        if(iid==IID_IDirectDraw7) trace_draw7(*out);
        else if(iid==IID_IDirect3D7) trace_d3d7(*out);
        else if(iid==IID_IDirect3DDevice7) trace_device7(*out);
    }
    return hr;
}
static HRESULT WINAPI trace_caps(void* object,LPDDCAPS hw,LPDDCAPS sw) {
    using Fn=HRESULT(WINAPI*)(void*,LPDDCAPS,LPDDCAPS);
    HRESULT hr=original_slot<Fn>(object,11)(object,hw,sw);
    log_line("GetCaps hr=%08lx",hr); return hr;
}
static HRESULT WINAPI trace_display(void* object,LPDDSURFACEDESC2 desc) {
    using Fn=HRESULT(WINAPI*)(void*,LPDDSURFACEDESC2);
    HRESULT hr=original_slot<Fn>(object,12)(object,desc);
    log_line("GetDisplayMode hr=%08lx width=%lu height=%lu bpp=%lu",hr,
        SUCCEEDED(hr)?desc->dwWidth:0,SUCCEEDED(hr)?desc->dwHeight:0,
        SUCCEEDED(hr)?desc->ddpfPixelFormat.dwRGBBitCount:0); return hr;
}
static HRESULT WINAPI trace_coop(void* object,HWND window,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,HWND,DWORD);
    HRESULT hr=original_slot<Fn>(object,20)(object,window,flags);
    log_line("SetCooperativeLevel flags=%08lx hr=%08lx",flags,hr); return hr;
}
struct DeviceEnumeration {LPD3DENUMDEVICESCALLBACK7 callback; void* context; unsigned count;};
static HRESULT CALLBACK trace_enum_device(LPSTR description,LPSTR name,LPD3DDEVICEDESC7 caps,LPVOID context) {
    auto state=static_cast<DeviceEnumeration*>(context); ++state->count;
    log_line("EnumDevice name=%s description=%s guid=%08lx render_depth=%08lx",name,description,
        caps->deviceGUID.Data1,caps->dwDeviceRenderBitDepth);
    return state->callback(description,name,caps,state->context);
}
static HRESULT WINAPI trace_enum_devices(void* object,LPD3DENUMDEVICESCALLBACK7 callback,void* context) {
    using Fn=HRESULT(WINAPI*)(void*,LPD3DENUMDEVICESCALLBACK7,void*);
    DeviceEnumeration state={callback,context,0};
    HRESULT hr=original_slot<Fn>(object,3)(object,callback?trace_enum_device:nullptr,callback?&state:context);
    log_line("EnumDevices count=%u hr=%08lx",state.count,hr); return hr;
}
static HRESULT WINAPI trace_create_device(void* object,REFCLSID cls,LPDIRECTDRAWSURFACE7 surface,LPDIRECT3DDEVICE7* out) {
    using Fn=HRESULT(WINAPI*)(void*,REFCLSID,LPDIRECTDRAWSURFACE7,LPDIRECT3DDEVICE7*);
    HRESULT hr=original_slot<Fn>(object,4)(object,cls,surface,out);
    log_line("CreateDevice cls=%08lx hr=%08lx device=%p",cls.Data1,hr,SUCCEEDED(hr)&&out?*out:nullptr);
    if(SUCCEEDED(hr)&&out&&*out) trace_device7(*out);
    return hr;
}
static void trace_draw7(void* object) {
    install_hook(object,0,reinterpret_cast<void*>(&trace_query));
    install_hook(object,6,reinterpret_cast<void*>(&trace_surface));
    install_hook(object,11,reinterpret_cast<void*>(&trace_caps));
    install_hook(object,12,reinterpret_cast<void*>(&trace_display));
    install_hook(object,20,reinterpret_cast<void*>(&trace_coop));
}
static void trace_d3d7(void* object) {
    install_hook(object,0,reinterpret_cast<void*>(&trace_query));
    install_hook(object,3,reinterpret_cast<void*>(&trace_enum_devices));
    install_hook(object,4,reinterpret_cast<void*>(&trace_create_device));
}
static volatile LONG scene_count=0,draw_count=0,xyz_count=0,rhw_count=0,transform_count=0;
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
static void* replay_draw_caller=nullptr;
static void* replay_draw_parent=nullptr;
#endif
#include "stereo_preview.inl"
static void record_draw(unsigned api,DWORD fvf,DWORD count,void* vertices,void* caller) {
    InterlockedIncrement(&draw_count);
    DWORD position=fvf&D3DFVF_POSITION_MASK;
    if(position==D3DFVF_XYZRHW) InterlockedIncrement(&rhw_count);
    else if(position) InterlockedIncrement(&xyz_count);
    struct Sample {unsigned api; DWORD fvf; unsigned count;};
    static Sample samples[64]={}; static unsigned used=0;
    static SRWLOCK sample_lock=SRWLOCK_INIT;
    bool output=false;
    AcquireSRWLockExclusive(&sample_lock);
    unsigned i=0; for(;i<used;++i) if(samples[i].api==api && samples[i].fvf==fvf) break;
    if(i==used && used<64) samples[used++]={api,fvf,0};
    if(i<used) output=samples[i].count++<3;
    ReleaseSRWLockExclusive(&sample_lock);
    if(output) {
        log_line("DRAW api_slot=%u fvf=%08lx count=%lu position=%s caller=%p frame=%ld",api,fvf,count,
            position==D3DFVF_XYZRHW?"XYZRHW":position?"XYZ_or_blended":"unknown",caller,scene_count);
        if(vertices&&count&&(position==D3DFVF_XYZ || position==D3DFVF_XYZRHW)) {
            auto v=static_cast<const float*>(vertices);
            log_line("VERTEX0 x=%.6g y=%.6g z=%.6g rhw=%.6g",v[0],v[1],v[2],position==D3DFVF_XYZRHW?v[3]:0.0f);
        }
    }
}
static HRESULT WINAPI trace_begin(void* object) {
    using Fn=HRESULT(WINAPI*)(void*);
    HRESULT hr=original_slot<Fn>(object,5)(object);
    if(SUCCEEDED(hr)) {
        timing_research_begin();
        last_rendering_device=static_cast<IDirect3DDevice7*>(object);begin_xr_frame();
        static LONG cleared_present=-1;
        if(cleared_present!=presentation_count) {clear_eye_frame(last_rendering_device);cleared_present=presentation_count;}
    }
    LONG frame=InterlockedIncrement(&scene_count);
    if(frame<=3 || frame%120==0) log_line("SCENE frame=%ld begin_hr=%08lx draws=%ld xyz=%ld xyzrhw=%ld transforms=%ld",
        frame,hr,draw_count,xyz_count,rhw_count,transform_count);
    return hr;
}
static HRESULT WINAPI trace_end(void* object) {
    using Fn=HRESULT(WINAPI*)(void*);
    draw_controller_aid(static_cast<IDirect3DDevice7*>(object));
    HRESULT hr=original_slot<Fn>(object,6)(object);
    LONG frame=scene_count;
    if(SUCCEEDED(hr)&&(frame==60||frame==300||frame==900||frame==1260||frame==1500||frame==1800)) capture_frame(static_cast<IDirect3DDevice7*>(object),frame);
    if(SUCCEEDED(hr)&&eye_atlas&&(frame==300||frame==1500||frame==1800||frame==3000)) {
        auto device=static_cast<IDirect3DDevice7*>(object);
        SavedGameState saved(device,false,log_line,"eye_capture");
        if(saved.bind(eye_atlas)) capture_frame(device,frame+100000);
    }
    return hr;
}
static HRESULT WINAPI trace_transform(void* object,D3DTRANSFORMSTATETYPE type,LPD3DMATRIX matrix) {
    using Fn=HRESULT(WINAPI*)(void*,D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
    LONG count=InterlockedIncrement(&transform_count);
    if(matrix&&type==D3DTRANSFORMSTATE_PROJECTION) xr_set_game_projection(*matrix);
    if(matrix&&(count<=12 || ((type==D3DTRANSFORMSTATE_VIEW||type==D3DTRANSFORMSTATE_PROJECTION)&&scene_count%120==0))) {
        const float* m=reinterpret_cast<const float*>(matrix);
        log_line("TRANSFORM type=%u caller=%p frame=%ld m=%.6g,%.6g,%.6g,%.6g;%.6g,%.6g,%.6g,%.6g;%.6g,%.6g,%.6g,%.6g;%.6g,%.6g,%.6g,%.6g",
            static_cast<unsigned>(type),_ReturnAddress(),scene_count,
            m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11],m[12],m[13],m[14],m[15]);
    }
    return original_slot<Fn>(object,11)(object,type,matrix);
}
static HRESULT WINAPI trace_primitive(void* object,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID data,DWORD count,DWORD flags) {
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    replay_draw_caller=_ReturnAddress();
    replay_draw_parent=nullptr;
    auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(reinterpret_cast<uintptr_t>(replay_draw_caller)==base+0xA5A1F)
        replay_draw_parent=reinterpret_cast<void**>(_AddressOfReturnAddress())[8];
#endif
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,DWORD,LPVOID,DWORD,DWORD);
    record_draw(25,fvf,count,data,_ReturnAddress());
    auto original=original_slot<Fn>(object,25);
    return stereo_draw(object,fvf,data,count,[&](void* vertices){return original(object,type,fvf,vertices,count,flags);});
}
static HRESULT WINAPI trace_indexed(void* object,D3DPRIMITIVETYPE type,DWORD fvf,LPVOID data,DWORD count,LPWORD indices,DWORD index_count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,DWORD,LPVOID,DWORD,LPWORD,DWORD,DWORD);
    record_draw(26,fvf,count,data,_ReturnAddress());
    auto original=original_slot<Fn>(object,26);
    return stereo_draw(object,fvf,data,count,[&](void* vertices){return original(object,type,fvf,vertices,count,indices,index_count,flags);});
}
static HRESULT WINAPI trace_strided(void* object,D3DPRIMITIVETYPE type,DWORD fvf,LPD3DDRAWPRIMITIVESTRIDEDDATA data,DWORD count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,DWORD);
    record_draw(29,fvf,count,nullptr,_ReturnAddress());
    return original_slot<Fn>(object,29)(object,type,fvf,data,count,flags);
}
static HRESULT WINAPI trace_indexed_strided(void* object,D3DPRIMITIVETYPE type,DWORD fvf,LPD3DDRAWPRIMITIVESTRIDEDDATA data,DWORD count,LPWORD indices,DWORD index_count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,DWORD,LPD3DDRAWPRIMITIVESTRIDEDDATA,DWORD,LPWORD,DWORD,DWORD);
    record_draw(30,fvf,count,nullptr,_ReturnAddress());
    return original_slot<Fn>(object,30)(object,type,fvf,data,count,indices,index_count,flags);
}
static HRESULT WINAPI trace_vb(void* object,D3DPRIMITIVETYPE type,LPDIRECT3DVERTEXBUFFER7 vb,DWORD start,DWORD count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER7,DWORD,DWORD,DWORD);
    D3DVERTEXBUFFERDESC desc={}; desc.dwSize=sizeof(desc); if(vb) vb->GetVertexBufferDesc(&desc);
    record_draw(31,desc.dwFVF,count,nullptr,_ReturnAddress());
    return original_slot<Fn>(object,31)(object,type,vb,start,count,flags);
}
static HRESULT WINAPI trace_indexed_vb(void* object,D3DPRIMITIVETYPE type,LPDIRECT3DVERTEXBUFFER7 vb,DWORD start,DWORD count,LPWORD indices,DWORD index_count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,LPDIRECT3DVERTEXBUFFER7,DWORD,DWORD,LPWORD,DWORD,DWORD);
    D3DVERTEXBUFFERDESC desc={}; desc.dwSize=sizeof(desc); if(vb) vb->GetVertexBufferDesc(&desc);
    record_draw(32,desc.dwFVF,count,nullptr,_ReturnAddress());
    return original_slot<Fn>(object,32)(object,type,vb,start,count,indices,index_count,flags);
}
static HRESULT WINAPI trace_sphere_visibility(void* object,LPD3DVECTOR centers,LPD3DVALUE radii,DWORD count,DWORD flags,LPDWORD visibility) {
    using Fn=HRESULT(WINAPI*)(void*,LPD3DVECTOR,LPD3DVALUE,DWORD,DWORD,LPDWORD);
    HRESULT hr=original_slot<Fn>(object,33)(object,centers,radii,count,flags,visibility);
    static unsigned reports=0;if(reports++<3) log_line("NATIVE_CULL sphere_visibility count=%lu flags=%08lx hr=%08lx first=%08lx frame_ready=%d",count,flags,hr,SUCCEEDED(hr)&&count&&visibility?visibility[0]:0,xr_frame_ready());
    // Scope the override to the observed object-rendering call in this original
    // executable; visibility queries used elsewhere retain their native results.
    auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto renderer=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+0xA7F69;
    if(FAILED(hr)||caller!=renderer||!headset_visibility||!xr_frame_ready()||flags||!centers||!radii||!visibility||count>100000) return hr;
    bool outside=false;
    for(DWORD i=0;i<count;++i) if(hotd2_xr::native_sphere_status(visibility[i])==2) {outside=true;break;}
    if(!outside) return hr;
    auto device=static_cast<IDirect3DDevice7*>(object);
    D3DMATRIX world={},view={},projection={},eye_view[2]={},eye_projection[2]={};
    if(FAILED(device->GetTransform(D3DTRANSFORMSTATE_WORLD,&world))||FAILED(device->GetTransform(D3DTRANSFORMSTATE_VIEW,&view))||
        FAILED(device->GetTransform(D3DTRANSFORMSTATE_PROJECTION,&projection))) return hr;
    if(!xr_eye_matrices(0,view,projection,eye_view[0],eye_projection[0])||
        !xr_eye_matrices(1,view,projection,eye_view[1],eye_projection[1])) return hr;
    using namespace DirectX;
    auto load=[](const D3DMATRIX& matrix){XMFLOAT4X4 copy;memcpy(&copy,&matrix,sizeof(copy));return XMLoadFloat4x4(&copy);};
    auto native=load(world)*load(view)*load(projection);
    auto left=load(world)*load(eye_view[0])*load(eye_projection[0]);
    auto right=load(world)*load(eye_view[1])*load(eye_projection[1]);
    for(DWORD i=0;i<count;++i) {
        if(hotd2_xr::native_sphere_status(visibility[i])!=2||(visibility[i]&0x00fc0fc0u)) continue;
        XMFLOAT3 center={centers[i].x,centers[i].y,centers[i].z};unsigned original=2,l=2,r=2;
        if(!hotd2_xr::sphere_frustum(native,center,radii[i],original)||!hotd2_xr::sphere_frustum(left,center,radii[i],l)||
            !hotd2_xr::sphere_frustum(right,center,radii[i],r)) continue;
        if(hotd2_xr::headset_visible(hotd2_xr::native_sphere_status(visibility[i]),original,l,r)) {
            // Keep clipping enabled: the union may intersect any of the native or eye planes.
            visibility[i]=(visibility[i]&~0x3f03fu)|D3DSTATUS_CLIPUNIONLEFT|D3DSTATUS_CLIPUNIONRIGHT|
                D3DSTATUS_CLIPUNIONTOP|D3DSTATUS_CLIPUNIONBOTTOM|D3DSTATUS_CLIPUNIONFRONT|D3DSTATUS_CLIPUNIONBACK;
            static unsigned expanded=0;if(expanded++<4) log_line("VR_CULL expanded caller=%p native=%u left=%u right=%u",_ReturnAddress(),original,l,r);
        }
    }
    return hr;
}
static void trace_device7(void* object) {
    install_hook(object,5,reinterpret_cast<void*>(&trace_begin));
    install_hook(object,6,reinterpret_cast<void*>(&trace_end));
    install_hook(object,10,reinterpret_cast<void*>(&trace_clear));
    install_hook(object,11,reinterpret_cast<void*>(&trace_transform));
    install_hook(object,25,reinterpret_cast<void*>(&trace_primitive));
    install_hook(object,26,reinterpret_cast<void*>(&trace_indexed));
    install_hook(object,29,reinterpret_cast<void*>(&trace_strided));
    install_hook(object,30,reinterpret_cast<void*>(&trace_indexed_strided));
    install_hook(object,31,reinterpret_cast<void*>(&trace_vb));
    install_hook(object,32,reinterpret_cast<void*>(&trace_indexed_vb));
    install_hook(object,33,reinterpret_cast<void*>(&trace_sphere_visibility));
}
