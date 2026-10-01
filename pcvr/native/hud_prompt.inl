#include "hud_prompt.h"
static bool hide_unused_player_two=true;
static hotd2_hud::PromptFilter unused_prompt_filter;
struct PromptSource {IDirectDrawSurface7* source;DWORD unique;hotd2_hud::Text text;};
static std::vector<PromptSource> prompt_sources;
static hotd2_hud::Text prompt_texture_identity(IDirectDrawSurface7* source,const DDSURFACEDESC2& desc) {
    DWORD unique=0;
    if(FAILED(source->GetUniquenessValue(&unique))) return hotd2_hud::Text::Other;
    for(auto& entry:prompt_sources) if(entry.source==source) {
        if(unique!=entry.unique) entry.text=hotd2_hud::Text::Other;
        return entry.text; // Changed or failed sources stay native; no repeated readbacks.
    }
    if(prompt_sources.size()>=64) return hotd2_hud::Text::Other;
    hotd2_hud::Text text=hotd2_hud::Text::Other;
    DWORD bits=desc.ddpfPixelFormat.dwRGBBitCount;
    if((bits==16||bits==32)&&(desc.ddpfPixelFormat.dwFlags&DDPF_RGB)&&
        desc.ddpfPixelFormat.dwRBitMask&&desc.ddpfPixelFormat.dwGBitMask&&desc.ddpfPixelFormat.dwBBitMask) {
        DDSURFACEDESC2 locked={};locked.dwSize=sizeof(locked);
        HRESULT lock_hr=source->Lock(nullptr,&locked,DDLOCK_READONLY|DDLOCK_WAIT,nullptr);
        if(SUCCEEDED(lock_hr)) {
            hotd2_texture::Image image;image.width=desc.dwWidth;image.height=desc.dwHeight;
            if(locked.dwWidth==desc.dwWidth&&locked.dwHeight==desc.dwHeight&&locked.ddpfPixelFormat.dwRGBBitCount==bits&&
                hotd2_texture::valid_rows(locked.lpSurface,locked.lPitch,image.width,image.height,bits/8)) {
                image.bgra.resize(static_cast<size_t>(image.width)*image.height*4);
                for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x) {
                    DWORD pixel=0;memcpy(&pixel,static_cast<const BYTE*>(locked.lpSurface)+static_cast<ptrdiff_t>(y)*locked.lPitch+x*(bits/8),bits/8);
                    auto out=image.bgra.data()+(static_cast<size_t>(y)*image.width+x)*4;
                    out[0]=channel(pixel,desc.ddpfPixelFormat.dwBBitMask);out[1]=channel(pixel,desc.ddpfPixelFormat.dwGBitMask);out[2]=channel(pixel,desc.ddpfPixelFormat.dwRBitMask);
                    out[3]=(desc.ddpfPixelFormat.dwFlags&DDPF_ALPHAPIXELS)?channel(pixel,desc.ddpfPixelFormat.dwRGBAlphaBitMask):255;
                }
            }
            DWORD after=0;
            // This backend increments uniqueness even on a read-only unlock.
            // Cache the post-read value, then reject any subsequent change.
            if(SUCCEEDED(source->Unlock(nullptr))&&SUCCEEDED(source->GetUniquenessValue(&after))) {
                unique=after;
                auto key=hotd2_texture::content_key(image);text=hotd2_hud::text_identity(key.c_str());
                if(prompt_sources.size()<8) log_line("VR_HUD identity size=%lux%lu key=%s matched=%d",desc.dwWidth,desc.dwHeight,key.c_str(),static_cast<int>(text));
            }
            else if(prompt_sources.size()<8) log_line("VR_HUD identity invalidated before=%lu after=%lu",unique,after);
        }
        else if(prompt_sources.size()<8) log_line("VR_HUD identity lock_hr=%08lx",lock_hr);
    }
    source->AddRef();prompt_sources.push_back({source,unique,text});
    return text;
}
static bool hide_player_two_draw(IDirect3DDevice7* device,DWORD fvf,const void* vertices,DWORD count,const D3DVIEWPORT7& viewport,uint32_t frame) {
    unused_prompt_filter.begin(frame);
    if(!hide_unused_player_two||fvf!=0x1c4||viewport.dwX||viewport.dwY||viewport.dwWidth!=640||viewport.dwHeight!=480) return false;
    hotd2_hud::Bounds bounds;if(!hotd2_hud::footer_quad(vertices,count,bounds)) return false;
    IDirectDrawSurface7* texture=nullptr;DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);
    if(FAILED(device->GetTexture(0,&texture))||!texture) return false;
    bool hide=false;
    if(SUCCEEDED(texture->GetSurfaceDesc(&desc))) {
        auto text=hotd2_hud::Text::Other;
        if(desc.dwWidth==128&&(desc.dwHeight==16||desc.dwHeight==32)) text=prompt_texture_identity(texture,desc);
        hide=unused_prompt_filter.hide(vertices,count,desc.dwWidth,desc.dwHeight,text);
    }
    texture->Release();
    if(hide){static unsigned reports=0;if(reports++<6) log_line("VR_HUD unused player-two footer suppressed in eye atlas; desktop preserved");}
    return hide;
}
