// Exercise production cache/dispatch with deterministic surface calls. These
// checks mock COM operations, not an actual headset or graphics driver.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "texture_images.h"
#include "hud_prompt.h"
static unsigned checks=0;
static void require(bool ok,const char* name){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}}
static void log_line(const char*,...){}
static BYTE channel(DWORD p,DWORD mask){if(!mask)return 0;unsigned shift=0;while(!(mask&1)){mask>>=1;++shift;}return static_cast<BYTE>((static_cast<unsigned long long>(p>>shift&mask)*255)/mask);}
struct FakeSurface {
    DWORD unique=2;unsigned refs=1,locks=0;HRESULT lock_hr=S_OK,unique_hr=S_OK;
    std::vector<DWORD> pixels=std::vector<DWORD>(16*32,0xff314159);
    ULONG AddRef(){return ++refs;} ULONG Release(){return --refs;}
    HRESULT GetUniquenessValue(DWORD* out){*out=unique;return unique_hr;}
    HRESULT GetSurfaceDesc(DDSURFACEDESC2* d){
        *d={};d->dwSize=sizeof(*d);d->dwWidth=16;d->dwHeight=32;d->lPitch=64;
        d->lpSurface=pixels.data();d->ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;
        d->ddpfPixelFormat.dwRGBBitCount=32;d->ddpfPixelFormat.dwRBitMask=0xff0000;
        d->ddpfPixelFormat.dwGBitMask=0xff00;d->ddpfPixelFormat.dwBBitMask=0xff;
        d->ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;return S_OK;
    }
    HRESULT Lock(RECT*,DDSURFACEDESC2* d,DWORD,HANDLE){++locks;GetSurfaceDesc(d);return lock_hr;}
    HRESULT Unlock(RECT*){++unique;return S_OK;}
};
struct FakeDevice {
    FakeSurface* source;unsigned gets=0;
    HRESULT GetTexture(DWORD,FakeSurface** out){++gets;*out=source;source->AddRef();return S_OK;}
};
#define IDirectDrawSurface7 FakeSurface
#define IDirect3DDevice7 FakeDevice
#include "hud_prompt.inl"
#undef IDirectDrawSurface7
#undef IDirect3DDevice7
using hotd2_hud::Vertex;
static void quad(Vertex* p,float left,float top,float right,float bottom){
    p[0]={left,bottom,.19994f,1.0001f,0xffffffff,0,0,0};p[1]={right,bottom,.19994f,1.0001f,0xffffffff,0,1,0};
    p[2]={left,top,.19994f,1.0001f,0xffffffff,0,0,1};p[3]={right,top,.19994f,1.0001f,0xffffffff,0,1,1};
}
int main(){
    FakeSurface source;FakeDevice device{&source};D3DVIEWPORT7 viewport={0,0,640,480,0,1};Vertex p[4];
    quad(p,518,427,531.6f,454.2f);
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,1),"unknown first count stays native");
    require(source.locks==1&&source.unique==3,"read-only backend unlock cached after uniqueness increment");
    quad(p,424,427,532.8f,454.2f);unused_prompt_filter.begin(2);
    require(unused_prompt_filter.hide(p,4,128,32,hotd2_hud::Text::Credits),"fresh credit label authenticates row");
    quad(p,518,427,531.6f,454.2f);
    require(hide_player_two_draw(&device,0x1c4,p,4,viewport,2),"new glyph authenticated in credit row");
    require(hide_player_two_draw(&device,0x1c4,p,4,viewport,120),"learned counter stays hidden with label absent");
    require(source.locks==1,"blink gaps do not cause repeated readbacks");
    quad(p,450,427,463.6f,454.2f);
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,121),"learned shared font preserved outside count slot");
    quad(p,518,427,531.6f,454.2f);hide_unused_player_two=false;unsigned gets=device.gets;
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,122)&&device.gets==gets,"disabled filter preserves draw without texture inspection");
    hide_unused_player_two=true;++source.unique;
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,123),"changed counter source returns to native");
    quad(p,424,427,532.8f,454.2f);unused_prompt_filter.begin(124);unused_prompt_filter.hide(p,4,128,32,hotd2_hud::Text::Credits);
    quad(p,518,427,531.6f,454.2f);hide_player_two_draw(&device,0x1c4,p,4,viewport,124);
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,200),"invalidated glyph cannot regain cached identity");
    require(source.locks==1&&source.refs==2,"one bounded cache reference and no repeat reads");
    viewport.dwWidth=800;gets=device.gets;
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,201)&&device.gets==gets,"non-native viewport stays native");
    viewport.dwWidth=640;
    FakeSurface failed;failed.lock_hr=E_FAIL;device.source=&failed;
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,300),"failed read preserves unassociated count");
    quad(p,424,427,532.8f,454.2f);unused_prompt_filter.begin(301);unused_prompt_filter.hide(p,4,128,32,hotd2_hud::Text::Credits);
    quad(p,518,427,531.6f,454.2f);hide_player_two_draw(&device,0x1c4,p,4,viewport,301);
    // A failed read must not be promoted just because the geometry was valid.
    require(!hide_player_two_draw(&device,0x1c4,p,4,viewport,400),"failed source never promoted to stable counter");
    printf("PASS %u production HUD cache checks (mocked surfaces)\n",checks);return 0;
}
