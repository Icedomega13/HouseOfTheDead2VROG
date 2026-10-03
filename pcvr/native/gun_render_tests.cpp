// Actual D3D7 backend test of the production gun pass, including self-occlusion,
// transparent compositing, native state restoration and native depth preservation.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <DirectXMath.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "xr_bridge.h"
static unsigned checks=0;
static void require(bool ok,const char* label){++checks;if(!ok){fprintf(stderr,"FAIL %s hr/error=%lu\n",label,GetLastError());exit(1);}}
static void log_line(const char* format,...){va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");}
template<class Fn> static Fn original_slot(void* object,unsigned slot){return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot]);}
static float yaw=0,pitch=0,roll=0;
unsigned xr_eye_size(){return 400;}
bool xr_frame_ready(){return true;}
XrGameInput xr_game_input(){XrGameInput out;out.active=out.aim_valid=true;return out;}
static bool cursor_visible=false;
static bool dual_wield=false;
bool xr_dual_wield_enabled(){return dual_wield;}
bool xr_aim_cursor_visible(){return cursor_visible;}
bool xr_pointer_vertex(unsigned,float& x,float& y){x=.9f;y=.1f;return true;}
bool xr_gun_matrices(unsigned eye,D3DMATRIX& view,D3DMATRIX& projection){
    using namespace DirectX;XMFLOAT4X4 m;
    XMStoreFloat4x4(&m,XMMatrixTranslation(0,.035f,-.06f)*XMMatrixRotationRollPitchYaw(pitch,yaw,roll)*XMMatrixTranslation(eye==0?.003f:-.003f,0,.5f));
    memcpy(&view,&m,sizeof(m));XMStoreFloat4x4(&m,XMMatrixOrthographicLH(.3f,.3f,.01f,1));memcpy(&projection,&m,sizeof(m));return true;
}
bool xr_hand_gun_matrices(unsigned hand,unsigned eye,D3DMATRIX& view,D3DMATRIX& projection){
    if(!dual_wield||hand!=1)return false;
    bool ok=xr_gun_matrices(eye,view,projection);view._41-=.09f;return ok;
}
bool xr_hand_pointer_vertex(unsigned hand,unsigned eye,float& x,float& y){if(hand!=1||!dual_wield)return false;bool ok=xr_pointer_vertex(eye,x,y);x=.1f;return ok;}
static bool gauges=false;
static int gauge_rounds[2]={6,6};
bool xr_magazine_gauges(int& right,int& left){right=gauge_rounds[0];left=gauge_rounds[1];return gauges;}
static bool health_visible=false;
static int health_current=3,gauge_health_maximum=5;
bool xr_health_gauge(int& current,int& maximum){current=health_current;maximum=gauge_health_maximum;return health_visible;}
bool xr_ammo_gauge_vertex(unsigned eye,float x,float y,float& out_x,float& out_y){out_x=(x+1)*.5f+(eye?-.008f:.008f);out_y=(y+1)*.5f;return true;}
#include "eye_targets.inl"
struct TL {float x,y,z,rhw;DWORD color;};
static void native_quad(IDirect3DDevice7* device,float z,DWORD color){
    device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetRenderState(D3DRENDERSTATE_ZENABLE,D3DZB_TRUE);device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,TRUE);
    device->SetRenderState(D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
    TL p[]={{0,0,z,1,color},{800,0,z,1,color},{0,400,z,1,color},{800,400,z,1,color}};
    require(SUCCEEDED(device->DrawPrimitive(D3DPT_TRIANGLESTRIP,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,p,4,0)),"native depth quad");
}
static std::vector<DWORD> pixels(IDirectDrawSurface7* surface){
    DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);
    require(SUCCEEDED(surface->Lock(nullptr,&desc,DDLOCK_WAIT|DDLOCK_READONLY,nullptr)),"GPU readback");
    require(desc.ddpfPixelFormat.dwRGBBitCount==32&&desc.ddpfPixelFormat.dwRGBAlphaBitMask==0xff000000,"ARGB GPU target format");
    std::vector<DWORD> out(static_cast<size_t>(desc.dwWidth)*desc.dwHeight);
    for(DWORD y=0;y<desc.dwHeight;++y)memcpy(out.data()+y*desc.dwWidth,static_cast<const BYTE*>(desc.lpSurface)+y*desc.lPitch,desc.dwWidth*4);
    require(SUCCEEDED(surface->Unlock(nullptr)),"GPU readback unlock");return out;
}
int wmain(int argc,wchar_t** argv){
    require(argc==2,"backend path required");
    auto module=LoadLibraryW(argv[1]);require(module!=nullptr,"load actual backend");
    using Create=HRESULT(WINAPI*)(GUID*,LPVOID*,REFIID,IUnknown*);
    auto create=reinterpret_cast<Create>(GetProcAddress(module,"DirectDrawCreateEx"));require(create!=nullptr,"backend factory");
    IDirectDraw7* draw=nullptr;require(SUCCEEDED(create(nullptr,reinterpret_cast<void**>(&draw),IID_IDirectDraw7,nullptr)),"actual DirectDraw7");
    auto window=CreateWindowW(L"STATIC",L"HotD2VR gun verification",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(window!=nullptr,"owned hidden test window");require(SUCCEEDED(draw->SetCooperativeLevel(window,DDSCL_NORMAL)),"normal cooperative mode");
    DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
    desc.dwWidth=640;desc.dwHeight=480;desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
    desc.ddpfPixelFormat={sizeof(DDPIXELFORMAT),DDPF_RGB|DDPF_ALPHAPIXELS,0,32,0xff0000,0xff00,0xff,0xff000000};
    IDirectDrawSurface7* desktop=nullptr;require(SUCCEEDED(draw->CreateSurface(&desc,&desktop,nullptr)),"desktop target");
    desc.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;desc.ddpfPixelFormat={};desc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
    desc.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;desc.ddpfPixelFormat.dwZBufferBitDepth=32;
    IDirectDrawSurface7* depth=nullptr;require(SUCCEEDED(draw->CreateSurface(&desc,&depth,nullptr)),"desktop depth");
    require(SUCCEEDED(desktop->AddAttachedSurface(depth)),"attach desktop depth");depth->Release();
    IDirect3D7* d3d=nullptr;require(SUCCEEDED(draw->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d))),"actual Direct3D7");
    IDirect3DDevice7* device=nullptr;require(SUCCEEDED(d3d->CreateDevice(IID_IDirect3DHALDevice,desktop,&device)),"actual hardware device");
    D3DVIEWPORT7 native={11,13,600,440,.1f,.9f};require(SUCCEEDED(device->SetViewport(&native)),"native viewport");
    high_resolution=true;require(ensure_eye_target(device),"production eye target");require(ensure_gun_layer(device),"production transparent gun layer");
    clear_eye_frame(device);require(SUCCEEDED(trace_clear(device,0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0)),"production native clear hook");
    using namespace DirectX;
    const float angles[][3]={{0,0,0},{0,1.57f,0},{0,-1.57f,0},{0,3.14159f,0},{.8f,.7f,.6f},{-.8f,-.7f,-.6f},{1.57f,0,0},{-1.57f,0,0},{0,0,1.57f},{0,0,3.14159f},{.4f,2.7f,1.2f},{-.4f,-2.7f,-1.2f}};
    for(const auto& angle:angles){
        pitch=angle[0];yaw=angle[1];roll=angle[2];
        require(SUCCEEDED(device->BeginScene()),"begin GPU scene");
        {
            SavedGameState saved(device,true,log_line,"test_native_depth");require(saved.bind(eye_atlas),"bind atlas");
            D3DVIEWPORT7 full={0,0,800,400,0,1};device->SetViewport(&full);
            require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)),"clear native atlas");
            native_quad(device,.2f,0xff123456);
        }
        device->SetRenderState(D3DRENDERSTATE_ZENABLE,D3DZB_FALSE);device->SetRenderState(D3DRENDERSTATE_CULLMODE,D3DCULL_CW);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        draw_controller_aid(device);
        DWORD state=0;device->GetRenderState(D3DRENDERSTATE_ZENABLE,&state);require(state==D3DZB_FALSE,"native Z state restored");
        device->GetRenderState(D3DRENDERSTATE_CULLMODE,&state);require(state==D3DCULL_CW,"native cull state restored");
        device->GetTextureStageState(0,D3DTSS_ALPHAOP,&state);require(state==D3DTOP_DISABLE,"native texture stage restored");
        D3DVIEWPORT7 after={};device->GetViewport(&after);require(memcmp(&after,&native,sizeof(native))==0,"native viewport restored");
        IDirectDrawSurface7* target=nullptr;device->GetRenderTarget(&target);require(target==desktop,"native target restored");target->Release();
        {
            SavedGameState saved(device,true,log_line,"test_preserved_depth");require(saved.bind(eye_atlas),"bind native atlas after gun");
            D3DVIEWPORT7 full={0,0,800,400,0,1};device->SetViewport(&full);native_quad(device,.8f,0xffff0000);
        }
        require(SUCCEEDED(device->EndScene()),"end GPU scene");
        auto layer=pixels(gun_layer),atlas=pixels(eye_atlas);
        require(atlas[20*800+20]==0xff123456,"gun pass preserves native world depth and empty background");
        unsigned compared=0,mismatches=0,opaque=0;
        for(unsigned eye=0;eye<2;++eye){
            D3DMATRIX v={},p={};xr_gun_matrices(eye,v,p);XMFLOAT4X4 vm,pm;memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
            auto m=XMLoadFloat4x4(&vm)*XMLoadFloat4x4(&pm);
            struct Projected{float x,y,z;DWORD color;};std::vector<Projected> projected;
            for(auto vertex:hotd2_pistol::mesh()){
                auto point=XMVector3TransformCoord(XMVectorSet(vertex.x,vertex.y,vertex.z,1),m);
                projected.push_back({(XMVectorGetX(point)+1)*200,(1-XMVectorGetY(point))*200,XMVectorGetZ(point),vertex.color});
            }
            for(unsigned y=8;y<392;y+=4)for(unsigned x=8;x<392;x+=4){
                float nearest=2,margin=0;DWORD expected=0;
                for(size_t i=0;i<projected.size();i+=3){
                    auto a=projected[i],b=projected[i+1],c=projected[i+2];
                    float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
                    if(std::fabs(denominator)<1e-6f)continue;
                    float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/denominator;
                    float w=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/denominator,t=1-u-w;
                    float z=u*a.z+w*b.z+t*c.z;
                    if(u>=0&&w>=0&&t>=0&&z<nearest){nearest=z;expected=a.color;margin=std::min(u,std::min(w,t));}
                }
                auto actual=layer[y*800+eye*400+x];
                if(expected&&(actual>>24)>=250)++opaque;
                // Compare interior face samples; omit edges and near-coplanar MSAA joins.
                bool stable=true;
                if(expected&&margin>.08f)for(float dx:{-.75f,.75f})for(float dy:{-.75f,.75f}){
                    float near_sample=2;DWORD sample_color=0;
                    for(size_t i=0;i<projected.size();i+=3){
                        auto a=projected[i],b=projected[i+1],c=projected[i+2];
                        float d=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(std::fabs(d)<1e-6f)continue;
                        float u=((b.y-c.y)*(x+dx-c.x)+(c.x-b.x)*(y+dy-c.y))/d;
                        float w=((c.y-a.y)*(x+dx-c.x)+(a.x-c.x)*(y+dy-c.y))/d,t=1-u-w,z=u*a.z+w*b.z+t*c.z;
                        if(u>=0&&w>=0&&t>=0&&z<near_sample){near_sample=z;sample_color=a.color;}
                    }
                    stable=stable&&sample_color==expected;
                }
                if(expected&&margin>.08f&&stable){
                    ++compared;bool match=(actual>>24)>=250;
                    for(unsigned shift:{0u,8u,16u})match=match&&std::abs(static_cast<int>((actual>>shift)&255)-static_cast<int>((expected>>shift)&255))<=3;
                    if(!match){if(mismatches<4)printf("sample eye=%u x=%u y=%u actual=%08lx expected=%08lx z=%.6g margin=%.6g\n",eye,x,y,actual,expected,nearest,margin);++mismatches;}
                }
            }
        }
        printf("GPU angles pitch=%.3f yaw=%.3f roll=%.3f opaque=%u nearest_face_samples=%u mismatches=%u\n",pitch,yaw,roll,opaque,compared,mismatches);
        require(opaque>30&&compared>20,"solid pistol visible from both eyes at this angle");
        require(mismatches==0,"actual GPU faces agree with closest surface, without see-through parts");
    }
    for(bool shown:{false,true}){
        cursor_visible=shown;require(SUCCEEDED(device->BeginScene()),"begin cursor visibility GPU scene");
        {SavedGameState saved(device,false,log_line,"test_cursor_clear");require(saved.bind(eye_atlas),"bind cursor test atlas");
         require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)),"clear cursor test atlas");}
        draw_controller_aid(device);require(SUCCEEDED(device->EndScene()),"end cursor visibility GPU scene");
        auto atlas=pixels(eye_atlas),layer=pixels(gun_layer);unsigned green=0,solid=0;
        for(unsigned eye=0;eye<2;++eye)for(unsigned y=35;y<46;++y)for(unsigned x=355;x<366;++x)
            if((atlas[y*800+eye*400+x]&0xffffff)==0x00ff40)++green;
        for(auto pixel:layer)if((pixel>>24)>250)++solid;
        require(shown?green>0:green==0,"actual GPU dot respects cursor visibility");
        require(solid>30,"gun remains visible in both cursor modes");
    }
    dual_wield=true;yaw=pitch=roll=0;
    for(bool shown:{false,true}){
        cursor_visible=shown;require(SUCCEEDED(device->BeginScene()),"begin two-gun GPU scene");
        {SavedGameState saved(device,false,log_line,"test_dual_clear");require(saved.bind(eye_atlas),"bind dual test atlas");require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)),"clear dual test atlas");}
        draw_controller_aid(device);require(SUCCEEDED(device->EndScene()),"end two-gun GPU scene");
        auto atlas=pixels(eye_atlas),layer=pixels(gun_layer);
        for(unsigned eye=0;eye<2;++eye){
            unsigned left_solid=0,right_solid=0,cyan=0,green=0;
            for(unsigned y=10;y<390;++y)for(unsigned x=0;x<400;++x){auto pixel=layer[y*800+eye*400+x];if((pixel>>24)>250){if(x<140)++left_solid;else ++right_solid;}}
            for(unsigned y=35;y<46;++y)for(unsigned x=35;x<46;++x)if((atlas[y*800+eye*400+x]&0xffffff)==0x00c8ff)++cyan;
            for(unsigned y=35;y<46;++y)for(unsigned x=355;x<366;++x)if((atlas[y*800+eye*400+x]&0xffffff)==0x00ff40)++green;
            require(left_solid>30&&right_solid>30,"actual GPU shows independent left and right pistols in each eye");
            require(shown?(cyan>0&&green>0):(cyan==0&&green==0),"cursor toggle controls both hand markers on actual GPU");
        }
    }
    // Production gauge artwork and production pass on the actual D3D7 backend.
    // Sample every bullet's interior in both eyes, including empty/full/asymmetric
    // clips and visibility preference. Cursor is intentionally off throughout.
    cursor_visible=false;
    for(bool shown:{false,true})for(int rounds:{0,1,2,3,4,5,6}){
        gauges=shown;gauge_rounds[0]=rounds;gauge_rounds[1]=6-rounds;
        require(SUCCEEDED(device->BeginScene()),"begin gauge GPU scene");
        {SavedGameState saved(device,false,log_line,"gauge_test_clear");require(saved.bind(eye_atlas),"bind gauge atlas");
         require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)),"clear gauge atlas");}
        device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
        draw_controller_aid(device);
        DWORD blend=99;device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,&blend);require(blend==FALSE,"gauge alpha state restored");
        D3DVIEWPORT7 restored={};device->GetViewport(&restored);require(memcmp(&restored,&native,sizeof(native))==0,"gauge viewport restored");
        require(SUCCEEDED(device->EndScene()),"end gauge GPU scene");auto atlas=pixels(eye_atlas);
        for(unsigned eye=0;eye<2;++eye)for(unsigned hand=0;hand<2;++hand){
            int n=gauge_rounds[hand];DWORD accent=n?(hand?0x00c8ff:0x00ff40):0xff7040;
            for(int slot=0;slot<6;++slot){
                float px=0,py=0;xr_ammo_gauge_vertex(eye,(hand?-.75f:.43f)+.0875f+slot*.038f,.43f+.085f,px,py);
                auto pixel=atlas[static_cast<unsigned>(py*400+.5f)*800+eye*400+static_cast<unsigned>(px*400+.5f)]&0xffffff;
                require(pixel==(shown?(slot<n?accent:0x394654):0x123456),"GPU bullet count/hand mapping matches native published rounds");
            }
            if(shown){
                const unsigned masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d};
                const float centers[7][2]={{.016f,.0025f},{.0295f,.014f},{.0295f,.041f},{.016f,.0535f},{.0025f,.041f},{.0025f,.014f},{.016f,.0275f}};
                for(unsigned s=0;s<7;++s){float px=0,py=0;
                    xr_ammo_gauge_vertex(eye,(hand?-.75f:.43f)+.015f+centers[s][0],.43f+.058f+centers[s][1],px,py);
                    auto pixel=atlas[static_cast<unsigned>(py*400+.5f)*800+eye*400+static_cast<unsigned>(px*400+.5f)]&0xffffff;
                    // At the minimum 400px eye size these strokes are one pixel
                    // wide. MSAA gives partial coverage; test lit vs unlit rather
                    // than demanding an unblended full-coverage colour.
                    const unsigned channel=n?(hand?0u:8u):16u;
                    bool lit=((pixel>>channel)&255)>110;
                    if(lit!=((masks[n]&(1u<<s))!=0))printf("glyph eye=%u hand=%u rounds=%d segment=%u pixel=%06lx xy=%.3f,%.3f\n",eye,hand,n,s,pixel,px*400,py*400);
                    require(lit==((masks[n]&(1u<<s))!=0),"actual GPU numeric gauge glyph agrees with count zero through six");
                }
            }
        }
    }
    gauges=true;health_visible=true;cursor_visible=false;
    for(int maximum:{3,5,9})for(int current=0;current<=maximum;++current){
        gauge_health_maximum=maximum;health_current=current;
        require(SUCCEEDED(device->BeginScene()),"begin health meter GPU scene");
        {SavedGameState saved(device,false,log_line,"health_test_clear");require(saved.bind(eye_atlas),"bind health atlas");
         require(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0)),"clear health atlas");}
        draw_controller_aid(device);require(SUCCEEDED(device->EndScene()),"end health meter GPU scene");auto atlas=pixels(eye_atlas);
        DWORD color=current<=1?0xff7040:current*2<=maximum?0xffc04d:0x58e89a;
        for(unsigned eye=0;eye<2;++eye){
            for(int slot=0;slot<maximum;++slot){float x=0,y=0;
                xr_ammo_gauge_vertex(eye,-.75f+.109f+(slot+.5f)*.195f/maximum-.002f,.315f+.0525f,x,y);
                auto pixel=atlas[static_cast<unsigned>(y*400+.5f)*800+eye*400+static_cast<unsigned>(x*400+.5f)]&0xffffff;
                require(pixel==(slot<current?color:0x394654),"GPU health cells agree with native current/capacity in both eyes");
            }
            float x=0,y=0;xr_ammo_gauge_vertex(eye,.43f+.15f,.315f+.05f,x,y);
            require((atlas[static_cast<unsigned>(y*400+.5f)*800+eye*400+static_cast<unsigned>(x*400+.5f)]&0xffffff)==0x123456,"shared health panel appears only above left ammo, not above right");
        }
    }
    gun_layer->Release();eye_atlas->Release();device->Release();d3d->Release();desktop->Release();draw->Release();DestroyWindow(window);FreeLibrary(module);
    printf("PASS %u actual GPU gun render checks\n",checks);return 0;
}
