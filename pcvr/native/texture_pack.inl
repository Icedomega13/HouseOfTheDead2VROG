#include "texture_images.h"
static bool texture_pack_enabled=false,texture_dump_enabled=false;
static std::wstring texture_folder;
struct TextureEntry {IDirectDrawSurface7* original=nullptr;IDirectDrawSurface7* replacement=nullptr;DWORD uniqueness=0;bool dynamic=false;std::string content_key;};
static std::vector<TextureEntry> texture_entries;
static size_t texture_source_bytes=0,texture_replacement_bytes=0;
static const char* cached_texture_key(IDirectDrawSurface7* source) {
    for(const auto& entry:texture_entries) if(entry.original==source) return entry.dynamic?"changing":entry.content_key.c_str();
    return "uncached"; // Trace only: never add GPU readbacks to normal gameplay.
}
static void configure_texture_pack(const wchar_t* config) {
    texture_pack_enabled=GetPrivateProfileIntW(L"Textures",L"Enabled",0,config)!=0;
    texture_dump_enabled=GetPrivateProfileIntW(L"Textures",L"Dump",0,config)!=0;
    if(!texture_pack_enabled&&!texture_dump_enabled) return;
    texture_folder=config;texture_folder.resize(texture_folder.find_last_of(L"\\/")+1);
    if(texture_dump_enabled) CreateDirectoryW((texture_folder+L"texture-dump").c_str(),nullptr);
    log_line("TEXTURE_PACK enabled=%d dump=%d source_cache_limit=256 source_budget_mb=32 replacement_budget_mb=128",texture_pack_enabled,texture_dump_enabled);
}
static IDirectDrawSurface7* replacement_texture(IDirectDrawSurface7* source) {
    if(!source||(!texture_pack_enabled&&!texture_dump_enabled)) return nullptr;
    DWORD unique=0;HRESULT unique_hr=source->GetUniquenessValue(&unique);
    if(texture_dump_enabled) {
        static IDirectDrawSurface7* observed[16]={};static unsigned observed_count=0;
        unsigned i=0;for(;i<observed_count;++i) if(observed[i]==source) break;
        if(i==observed_count&&observed_count<16) {
            observed[observed_count++]=source;DDSURFACEDESC2 sample={};sample.dwSize=sizeof(sample);HRESULT sample_hr=source->GetSurfaceDesc(&sample);
            DDCOLORKEY sample_key={};HRESULT key_hr=source->GetColorKey(DDCKEY_SRCBLT,&sample_key);
            log_line("TEXTURE_DUMP inspect uniqueness_hr=%08lx desc_hr=%08lx size=%lux%lu caps=%08lx bits=%lu flags=%08lx key_hr=%08lx",unique_hr,sample_hr,sample.dwWidth,sample.dwHeight,sample.ddsCaps.dwCaps,sample.ddpfPixelFormat.dwRGBBitCount,sample.ddpfPixelFormat.dwFlags,key_hr);
        }
    }
    if(FAILED(unique_hr)) return nullptr;
    for(auto& entry:texture_entries) if(entry.original==source) {
        if(unique!=entry.uniqueness) {entry.dynamic=true;if(entry.replacement){entry.replacement->Release();entry.replacement=nullptr;log_line("TEXTURE_PACK changing source retained as native");}}
        return entry.dynamic?nullptr:entry.replacement;
    }
    if(texture_entries.size()>=256||texture_source_bytes>=32*1024*1024) return nullptr;
    DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);
    if(FAILED(source->GetSurfaceDesc(&desc))||!(desc.ddsCaps.dwCaps&DDSCAPS_TEXTURE)||(desc.ddsCaps.dwCaps&DDSCAPS_MIPMAP)||!hotd2_texture::valid_size(desc.dwWidth,desc.dwHeight)) return nullptr;
    DWORD bits=desc.ddpfPixelFormat.dwRGBBitCount;
    if((bits!=16&&bits!=32)||!(desc.ddpfPixelFormat.dwFlags&DDPF_RGB)||!desc.ddpfPixelFormat.dwRBitMask||!desc.ddpfPixelFormat.dwGBitMask||!desc.ddpfPixelFormat.dwBBitMask) return nullptr;
    if(static_cast<size_t>(desc.dwWidth)*desc.dwHeight*(bits/8)>32*1024*1024-texture_source_bytes) return nullptr;
    DDSURFACEDESC2 locked={};locked.dwSize=sizeof(locked);
    if(FAILED(source->Lock(nullptr,&locked,DDLOCK_READONLY|DDLOCK_WAIT,nullptr))) return nullptr;
    if(locked.dwWidth!=desc.dwWidth||locked.dwHeight!=desc.dwHeight||locked.ddpfPixelFormat.dwRGBBitCount!=bits||
        !hotd2_texture::valid_rows(locked.lpSurface,locked.lPitch,desc.dwWidth,desc.dwHeight,bits/8)) {source->Unlock(nullptr);return nullptr;}
    hotd2_texture::Image original;original.width=desc.dwWidth;original.height=desc.dwHeight;
    original.bgra.resize(static_cast<size_t>(original.width)*original.height*4);
    for(unsigned y=0;y<original.height;++y) for(unsigned x=0;x<original.width;++x) {
        DWORD pixel=0;memcpy(&pixel,static_cast<BYTE*>(locked.lpSurface)+static_cast<ptrdiff_t>(y)*locked.lPitch+x*(bits/8),bits/8);
        auto out=original.bgra.data()+(static_cast<size_t>(y)*original.width+x)*4;
        out[0]=channel(pixel,desc.ddpfPixelFormat.dwBBitMask);out[1]=channel(pixel,desc.ddpfPixelFormat.dwGBitMask);out[2]=channel(pixel,desc.ddpfPixelFormat.dwRBitMask);
        out[3]=(desc.ddpfPixelFormat.dwFlags&DDPF_ALPHAPIXELS)&&desc.ddpfPixelFormat.dwRGBAlphaBitMask?channel(pixel,desc.ddpfPixelFormat.dwRGBAlphaBitMask):255;
    }
    if(FAILED(source->Unlock(nullptr))) return nullptr;
    if(FAILED(source->GetUniquenessValue(&unique))) return nullptr;
    std::string hash=hotd2_texture::content_key(original);if(hash.empty()) return nullptr;
    std::wstring filename(hash.begin(),hash.end());filename+=L".png";
    if(texture_dump_enabled) {
        auto destination=texture_folder+L"texture-dump\\"+filename;
        if(GetFileAttributesW(destination.c_str())==INVALID_FILE_ATTRIBUTES) {
            bool ok=hotd2_texture::write_png(destination.c_str(),original);
            log_line("TEXTURE_DUMP key=%s size=%ux%u bits=%lu alpha_mask=%08lx saved=%d",hash.c_str(),original.width,original.height,bits,desc.ddpfPixelFormat.dwRGBAlphaBitMask,ok);
        }
    }
    TextureEntry entry;entry.original=source;entry.uniqueness=unique;entry.content_key=hash;source->AddRef();
    texture_source_bytes+=static_cast<size_t>(desc.dwWidth)*desc.dwHeight*(bits/8);
    hotd2_texture::Image replacement;
    if(texture_pack_enabled&&GetFileAttributesW((texture_folder+L"hd-textures\\"+filename).c_str())!=INVALID_FILE_ATTRIBUTES) {
        if(hotd2_texture::read_replacement((texture_folder+L"hd-textures\\"+filename).c_str(),original.width,original.height,replacement)&&
            hotd2_texture::replacement_size(replacement.width,replacement.height,original.width,original.height)&&
            replacement.bgra.size()<=128*1024*1024-texture_replacement_bytes) {
            IUnknown* owner=nullptr;IDirectDraw7* draw=nullptr;
            if(SUCCEEDED(source->GetDDInterface(reinterpret_cast<void**>(&owner)))&&owner) {owner->QueryInterface(IID_IDirectDraw7,reinterpret_cast<void**>(&draw));owner->Release();}
            if(draw) {
                DDSURFACEDESC2 hd={};hd.dwSize=sizeof(hd);hd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
                hd.dwWidth=replacement.width;hd.dwHeight=replacement.height;hd.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
                hd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);hd.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;hd.ddpfPixelFormat.dwRGBBitCount=32;
                hd.ddpfPixelFormat.dwRBitMask=0xff0000;hd.ddpfPixelFormat.dwGBitMask=0xff00;hd.ddpfPixelFormat.dwBBitMask=0xff;hd.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
                HRESULT hr=draw->CreateSurface(&hd,&entry.replacement,nullptr);draw->Release();
                DDSURFACEDESC2 target={};target.dwSize=sizeof(target);
                if(SUCCEEDED(hr)&&entry.replacement) {
                    hr=entry.replacement->Lock(nullptr,&target,DDLOCK_WAIT,nullptr);
                    if(SUCCEEDED(hr)) {
                        bool valid=target.dwWidth==replacement.width&&target.dwHeight==replacement.height&&target.ddpfPixelFormat.dwRGBBitCount==32&&
                            target.ddpfPixelFormat.dwRBitMask==0xff0000&&target.ddpfPixelFormat.dwGBitMask==0xff00&&target.ddpfPixelFormat.dwBBitMask==0xff&&
                            target.ddpfPixelFormat.dwRGBAlphaBitMask==0xff000000&&hotd2_texture::valid_rows(target.lpSurface,target.lPitch,replacement.width,replacement.height,4);
                        if(valid) for(unsigned y=0;y<replacement.height;++y) memcpy(static_cast<BYTE*>(target.lpSurface)+static_cast<ptrdiff_t>(y)*target.lPitch,
                            replacement.bgra.data()+static_cast<size_t>(y)*replacement.width*4,replacement.width*4);
                        hr=entry.replacement->Unlock(nullptr);if(!valid) hr=DDERR_INVALIDPIXELFORMAT;
                    }
                    if(FAILED(hr)) {entry.replacement->Release();entry.replacement=nullptr;}
                }
                if(entry.replacement) {texture_replacement_bytes+=replacement.bgra.size();log_line("TEXTURE_PACK loaded key=%s native=%ux%u replacement=%ux%u",hash.c_str(),original.width,original.height,replacement.width,replacement.height);}
                else log_line("TEXTURE_PACK rejected key=%s upload_hr=%08lx",hash.c_str(),hr);
            }
        } else log_line("TEXTURE_PACK rejected key=%s image_or_size_or_budget_invalid=1",hash.c_str());
    }
    texture_entries.push_back(entry);return entry.replacement;
}
class EyeTextureOverride {
    IDirect3DDevice7* device;
    IDirectDrawSurface7* original=nullptr;
    bool bound=false;
public:
    EyeTextureOverride(IDirect3DDevice7* source,bool world):device(source) {
        if(!world||(!texture_pack_enabled&&!texture_dump_enabled)||FAILED(device->GetTexture(0,&original))||!original) return;
        DWORD color_key=1;HRESULT color_hr=device->GetRenderState(D3DRENDERSTATE_COLORKEYENABLE,&color_key);
        if(FAILED(color_hr)||color_key) return; // Per-draw colour keys have no equivalent in arbitrary PNG art yet.
        auto replacement=replacement_texture(original);
        if(replacement) {HRESULT hr=device->SetTexture(0,replacement);bound=SUCCEEDED(hr);if(FAILED(hr)) log_line("TEXTURE_PACK bind_hr=%08lx",hr);}
    }
    ~EyeTextureOverride() {
        if(bound) {HRESULT hr=device->SetTexture(0,original);if(FAILED(hr)) log_line("TEXTURE_PACK restore_hr=%08lx",hr);}
        if(original) original->Release();
    }
};
