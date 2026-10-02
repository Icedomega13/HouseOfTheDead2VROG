// A separate stereo atlas gives each eye its own rasterization resolution.
#include "render_state.h"
#include "pistol_mesh.h"
using SavedGameState=hotd2_render::SavedState<IDirect3DDevice7,IDirectDrawSurface7>;
static bool high_resolution=false;
static IDirectDrawSurface7* eye_atlas=nullptr;
static IDirect3DDevice7* atlas_device=nullptr;
static IDirectDrawSurface7* gun_layer=nullptr;
static IDirect3DDevice7* gun_layer_device=nullptr;
static bool ensure_gun_layer(IDirect3DDevice7* device) {
    if(gun_layer&&gun_layer_device==device) return true;
    if(gun_layer){gun_layer->Release();gun_layer=nullptr;}
    if(!eye_atlas) return false;
    IUnknown* owner=nullptr;IDirectDraw7* draw=nullptr;
    if(SUCCEEDED(eye_atlas->GetDDInterface(reinterpret_cast<void**>(&owner)))&&owner){
        owner->QueryInterface(IID_IDirectDraw7,reinterpret_cast<void**>(&draw));owner->Release();
    }
    HRESULT hr=E_FAIL;
    if(draw){
        DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);
        desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
        desc.dwWidth=xr_eye_size()*2;desc.dwHeight=xr_eye_size();
        desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
        desc.ddpfPixelFormat={sizeof(DDPIXELFORMAT),DDPF_RGB|DDPF_ALPHAPIXELS,0,32,0xff0000,0xff00,0xff,0xff000000};
        hr=draw->CreateSurface(&desc,&gun_layer,nullptr);
        if(SUCCEEDED(hr)&&gun_layer){
            DDSCAPS2 caps={};caps.dwCaps=DDSCAPS_ZBUFFER;IDirectDrawSurface7* native_z=nullptr;
            if(SUCCEEDED(eye_atlas->GetAttachedSurface(&caps,&native_z))&&native_z){
                DDSURFACEDESC2 z={};z.dwSize=sizeof(z);hr=native_z->GetSurfaceDesc(&z);native_z->Release();
                if(SUCCEEDED(hr)){
                    desc.ddpfPixelFormat=z.ddpfPixelFormat;desc.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;
                    IDirectDrawSurface7* depth=nullptr;hr=draw->CreateSurface(&desc,&depth,nullptr);
                    if(SUCCEEDED(hr)&&depth){hr=gun_layer->AddAttachedSurface(depth);depth->Release();}
                }
            }else hr=E_FAIL;
            if(FAILED(hr)){gun_layer->Release();gun_layer=nullptr;}
        }
        draw->Release();
    }
    static unsigned reports=0;if(reports++<3)log_line("VR_PISTOL depth_layer_hr=%08lx surface=%p",hr,gun_layer);
    if(SUCCEEDED(hr)) gun_layer_device=device;
    return SUCCEEDED(hr)&&gun_layer;
}
static bool ensure_eye_target(IDirect3DDevice7* device) {
    if(!high_resolution) return false;
    if(eye_atlas&&atlas_device==device) return true;
    if(eye_atlas) {eye_atlas->Release();eye_atlas=nullptr;}
    IDirectDrawSurface7* source=nullptr;
    if(FAILED(device->GetRenderTarget(&source))||!source) return false;
    DDSURFACEDESC2 original={};original.dwSize=sizeof(original);source->GetSurfaceDesc(&original);
    IUnknown* owner=nullptr;IDirectDraw7* draw=nullptr;
    if(SUCCEEDED(source->GetDDInterface(reinterpret_cast<void**>(&owner)))&&owner) {
        owner->QueryInterface(IID_IDirectDraw7,reinterpret_cast<void**>(&draw));owner->Release();
    }
    HRESULT hr=E_FAIL;
    if(draw) {
        DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);
        desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
        desc.dwWidth=xr_eye_size()*2;desc.dwHeight=xr_eye_size();
        desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
        desc.ddpfPixelFormat=original.ddpfPixelFormat;
        hr=draw->CreateSurface(&desc,&eye_atlas,nullptr);
        if(SUCCEEDED(hr)) {
            DDSCAPS2 caps={};caps.dwCaps=DDSCAPS_ZBUFFER;
            IDirectDrawSurface7* original_z=nullptr;
            if(SUCCEEDED(source->GetAttachedSurface(&caps,&original_z))&&original_z) {
                DDSURFACEDESC2 z={};z.dwSize=sizeof(z);original_z->GetSurfaceDesc(&z);original_z->Release();
                desc.ddpfPixelFormat=z.ddpfPixelFormat;desc.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;
                IDirectDrawSurface7* depth=nullptr;
                hr=draw->CreateSurface(&desc,&depth,nullptr);
                if(SUCCEEDED(hr)&&depth) {hr=eye_atlas->AddAttachedSurface(depth);depth->Release();}
            } else hr=E_FAIL;
            if(FAILED(hr)) {eye_atlas->Release();eye_atlas=nullptr;}
        }
        draw->Release();
    }
    source->Release();
    log_line("EYE_TARGET size=%ux%u hr=%08lx surface=%p",xr_eye_size()*2,xr_eye_size(),hr,eye_atlas);
    if(FAILED(hr)) {high_resolution=false;return false;}
    atlas_device=device;return true;
}
static void clear_eye_frame(IDirect3DDevice7* device) {
    // Some native transitions fill/lock the desktop surface rather than calling D3D Clear.
    // Start each independent atlas frame clean so old logos/frames cannot accumulate.
    if(!ensure_eye_target(device)) return;
    SavedGameState saved(device,false,log_line,"frame_clear");
    if(saved.bind(eye_atlas)) {
        D3DVIEWPORT7 full={0,0,xr_eye_size()*2,xr_eye_size(),0,1};device->SetViewport(&full);
        using Fn=HRESULT(WINAPI*)(void*,DWORD,LPD3DRECT,DWORD,D3DCOLOR,D3DVALUE,DWORD);
        original_slot<Fn>(device,10)(device,0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0);
    }
}
static HRESULT WINAPI trace_clear(void* object,DWORD count,LPD3DRECT rects,DWORD flags,D3DCOLOR color,D3DVALUE z,DWORD stencil) {
    using Fn=HRESULT(WINAPI*)(void*,DWORD,LPD3DRECT,DWORD,D3DCOLOR,D3DVALUE,DWORD);
    auto original=original_slot<Fn>(object,10);
    HRESULT hr=original(object,count,rects,flags,color,z,stencil);
    auto device=static_cast<IDirect3DDevice7*>(object);
    if(ensure_eye_target(device)) {
        SavedGameState saved(device,false,log_line,"native_clear");
        if(saved.bind(eye_atlas)) {
            D3DVIEWPORT7 full={0,0,xr_eye_size()*2,xr_eye_size(),0,1};device->SetViewport(&full);
            // Original clears reset the corresponding complete eye images and depth.
            HRESULT cleared=original(object,0,nullptr,flags,color,z,stencil);
            if(FAILED(cleared)) log_line("EYE_TARGET clear_hr=%08lx",cleared);
        }
    }
    return hr;
}
static void draw_controller_aid(IDirect3DDevice7* device) {
    if(!eye_atlas||!xr_frame_ready()||!xr_game_input().active||!ensure_gun_layer(device)) return;
    SavedGameState saved(device,true,log_line,"controller_aid");
    if(saved.bind(gun_layer)) {
        D3DVIEWPORT7 full={0,0,xr_eye_size()*2,xr_eye_size(),0,1};device->SetViewport(&full);
        using Clear=HRESULT(WINAPI*)(void*,DWORD,LPD3DRECT,DWORD,D3DCOLOR,D3DVALUE,DWORD);
        HRESULT cleared=original_slot<Clear>(device,10)(device,0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0);
        if(FAILED(cleared)){log_line("VR_PISTOL clear_hr=%08lx",cleared);return;}
        device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
        device->SetRenderState(D3DRENDERSTATE_ZENABLE,D3DZB_TRUE);device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,TRUE);
        device->SetRenderState(D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRENDERSTATE_STENCILENABLE,FALSE);
        device->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE,FALSE);
        device->SetRenderState(D3DRENDERSTATE_CLIPPLANEENABLE,0);device->SetRenderState(D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID);
        device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE,FALSE);
        device->SetRenderState(D3DRENDERSTATE_FOGENABLE,FALSE);device->SetRenderState(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
        struct Dot {float x,y,z,rhw;DWORD color;};
        using Fn=HRESULT(WINAPI*)(void*,D3DPRIMITIVETYPE,DWORD,LPVOID,DWORD,DWORD);
        auto draw=original_slot<Fn>(device,25);
        using Transform=HRESULT(WINAPI*)(void*,D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
        auto transform=original_slot<Transform>(device,11);
        const auto& pistol=hotd2_pistol::mesh();
        // True depth testing on a private transparent layer handles intersecting
        // details and all face angles without clearing the world's depth buffer.
        static bool reported=false;
        if(!reported){reported=true;log_line("VR_PISTOL model=ams_faceted_v1 triangles=%zu private_depth=1 two_sided=1 aim_calibration_unchanged=1",pistol.size()/3);}
        D3DMATRIX identity={};identity._11=identity._22=identity._33=identity._44=1;
        for(unsigned eye=0;eye<2;++eye) {
            D3DVIEWPORT7 v={eye*xr_eye_size(),0,xr_eye_size(),xr_eye_size(),0,1};device->SetViewport(&v);
            D3DMATRIX gun_view={},gun_projection={};
            if(xr_gun_matrices(eye,gun_view,gun_projection)) {
                device->SetRenderState(D3DRENDERSTATE_LIGHTING,FALSE);
                device->SetRenderState(D3DRENDERSTATE_CLIPPING,TRUE);
                device->SetRenderState(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
                transform(device,D3DTRANSFORMSTATE_WORLD,&identity);transform(device,D3DTRANSFORMSTATE_VIEW,&gun_view);
                transform(device,D3DTRANSFORMSTATE_PROJECTION,&gun_projection);
                HRESULT hr=draw(device,D3DPT_TRIANGLELIST,D3DFVF_XYZ|D3DFVF_DIFFUSE,const_cast<hotd2_pistol::Vertex*>(pistol.data()),static_cast<DWORD>(pistol.size()),0);
                if(FAILED(hr)) {static unsigned errors=0;if(errors++<3) log_line("VR_PISTOL draw_hr=%08lx",hr);}
            }
        }
        if(!saved.bind(eye_atlas)) return;
        device->SetViewport(&full);
        device->SetRenderState(D3DRENDERSTATE_ZENABLE,FALSE);device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,FALSE);
        device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);
        device->SetRenderState(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
        device->SetTexture(0,gun_layer);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTFN_POINT);device->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTFG_POINT);
        device->SetTextureStageState(0,D3DTSS_MIPFILTER,D3DTFP_NONE);
        device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        device->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);device->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
        struct LayerVertex{float x,y,z,rhw,u,v;};float width=static_cast<float>(full.dwWidth),height=static_cast<float>(full.dwHeight);
        LayerVertex quad[]={{-.5f,-.5f,0,1,0,0},{width-.5f,-.5f,0,1,1,0},{-.5f,height-.5f,0,1,0,1},{width-.5f,height-.5f,0,1,1,1}};
        HRESULT composited=draw(device,D3DPT_TRIANGLESTRIP,D3DFVF_XYZRHW|D3DFVF_TEX1,quad,4,0);
        if(FAILED(composited)){static unsigned errors=0;if(errors++<3)log_line("VR_PISTOL composite_hr=%08lx",composited);}
        device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
        for(unsigned eye=0;eye<2;++eye) {
            D3DVIEWPORT7 v={eye*xr_eye_size(),0,xr_eye_size(),xr_eye_size(),0,1};device->SetViewport(&v);
            float x=0,y=0;if(!xr_aim_cursor_visible()||!xr_pointer_vertex(eye,x,y)||x<0||x>1||y<0||y>1) continue;
            float size=static_cast<float>(xr_eye_size());x=(x+eye)*size;y*=size;
            float radius=size*0.004f;
            Dot dot[]={{x-radius,y,0,1,0xff00ff40},{x,y-radius,0,1,0xff00ff40},{x,y+radius,0,1,0xff00ff40},{x+radius,y,0,1,0xff00ff40}};
            draw(device,D3DPT_TRIANGLESTRIP,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,dot,4,0);
        }
    }
}
