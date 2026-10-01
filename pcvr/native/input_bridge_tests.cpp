// Exercise the actual game's input hooks against Windows DirectInput in this test process.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "xr_bridge.h"
static XrGameInput simulated;
XrGameInput xr_game_input() {return simulated;}
static void log_line(const char*,...) {}
#include "vtable_hooks.inl"
#include "input_bridge.inl"
static unsigned checks=0;
static void require(bool value,const char* label) {
    ++checks;if(!value) {fprintf(stderr,"FAIL %s\n",label);exit(1);}
}
int main() {
    HWND window=CreateWindowExA(0,"STATIC","HOTD2 input test",WS_OVERLAPPED,100,100,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(window!=nullptr,"create own hidden input window");
    install_input_bridge();
    require(original_cursor!=nullptr,"patch actual cursor import slot");
    auto library=LoadLibraryExW(L"dinput.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(library!=nullptr,"load system legacy DirectInput");
    original_input_create=reinterpret_cast<InputCreateFn>(GetProcAddress(library,"DirectInputCreateEx"));
    require(original_input_create!=nullptr,"resolve real legacy factory");
    IDirectInput7A* input=nullptr;
    require(SUCCEEDED(vr_create_input(GetModuleHandleW(nullptr),0x700,IID_IDirectInput7A,reinterpret_cast<void**>(&input),nullptr)),"create real DirectInput7 through production hook");
    IDirectInputDevice7A *mouse=nullptr,*keyboard=nullptr;
    require(SUCCEEDED(input->CreateDeviceEx(GUID_SysMouse,IID_IDirectInputDevice7A,reinterpret_cast<void**>(&mouse),nullptr)),"create real mouse");
    require(SUCCEEDED(input->CreateDeviceEx(GUID_SysKeyboard,IID_IDirectInputDevice7A,reinterpret_cast<void**>(&keyboard),nullptr)),"create real keyboard");
    require(SUCCEEDED(mouse->SetDataFormat(&c_dfDIMouse)),"standard game mouse format");
    require(SUCCEEDED(keyboard->SetDataFormat(&c_dfDIKeyboard)),"standard game keyboard format");
    mouse->SetCooperativeLevel(window,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    keyboard->SetCooperativeLevel(window,DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    simulated.active=true;simulated.aim_valid=true;simulated.x=160;simulated.y=120;
    POINT cursor={},client={};RECT rect={};GetClientRect(window,&rect);
    require(GetCursorPos(&cursor)!=FALSE,"game cursor override");client=cursor;ScreenToClient(window,&client);
    require(client.x==(rect.right-rect.left)/4&&client.y==(rect.bottom-rect.top)/4,"aim converts to own client coordinates");
    POINT actual={};original_cursor(&actual);
    GetCursorPos(&cursor);POINT after={};original_cursor(&after);
    require(actual.x==after.x&&actual.y==after.y,"override does not move global cursor");
    simulated.fire=true;simulated.reload=true;simulated.start=true;simulated.back=true;simulated.menu_y=1;
    DIMOUSESTATE state={};BYTE keys[256]={};
    require(SUCCEEDED(mouse->GetDeviceState(sizeof(state),&state)),"VR input supplies state without desktop acquisition");
    require(state.rgbButtons[0]==0x80&&state.rgbButtons[1]==0x80,"trigger and reload reach mouse buttons");
    require(SUCCEEDED(keyboard->GetDeviceState(sizeof(keys),keys)),"VR keyboard state delivered");
    require(keys[DIK_RCONTROL]==0x80&&keys[DIK_RETURN]==0x80&&keys[DIK_ESCAPE]==0x80&&keys[DIK_UP]==0x80,"native Start, confirm, back and navigation delivered");
    DIDEVICEOBJECTDATA events[16]={};DWORD count=16;
    require(SUCCEEDED(keyboard->GetDeviceData(sizeof(events[0]),events,&count,DIGDD_PEEK))&&count==4,"buffered press events available");
    count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    require(count==4&&events[0].dwOfs==DIK_RCONTROL&&events[0].dwData==0x80,"peek preserves buffered events until consumed");
    simulated.fire=false;simulated.reload=false;simulated.start=false;simulated.back=false;simulated.menu_y=0;
    mouse->GetDeviceState(sizeof(state),&state);memset(keys,0,sizeof(keys));keyboard->GetDeviceState(sizeof(keys),keys);
    require(state.rgbButtons[0]==0&&state.rgbButtons[1]==0&&keys[DIK_RETURN]==0,"released controls do not stay pressed");
    count=16;keyboard->GetDeviceData(16,events,&count,0);
    require(count==4&&events[0].dwData==0,"legacy 16-byte release events delivered");
    simulated.fire=true;simulated.aim_valid=false;
    // The OpenXR bridge suppresses fire when aim tracking is invalid; this input hook preserves the physical mouse.
    state={};mouse->GetDeviceState(sizeof(state),&state);
    require(state.rgbButtons[0]==0,"invalid tracked aim does not inject a shot");
    simulated.fire=false;simulated.reload=true;state={};mouse->GetDeviceState(sizeof(state),&state);
    require(state.rgbButtons[1]==0x80,"aim-down reload reaches native mouse even with vertical off-screen aim");
    simulated.reload=false;state={};mouse->GetDeviceState(sizeof(state),&state);
    require(state.rgbButtons[1]==0,"aim-down reload pulse releases native mouse button");
    simulated.active=false;
    require(FAILED(mouse->GetDeviceState(sizeof(state),&state)),"inactive VR preserves original unacquired error");
    GetCursorPos(&cursor);original_cursor(&actual);
    require(cursor.x==actual.x&&cursor.y==actual.y,"inactive VR returns actual desktop cursor");
    // Lose focus while the game has peeked at, but not consumed, a new press.
    simulated.active=true;simulated.start=true;simulated.menu_x=1;
    count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,DIGDD_PEEK);
    require(count==3&&events[0].dwData==0x80,"unconsumed Start and navigation presses queued");
    simulated.active=false;count=16;memset(events,0,sizeof(events));
    require(SUCCEEDED(keyboard->GetDeviceData(sizeof(events[0]),events,&count,DIGDD_PEEK))&&count==3,"focus loss replaces pending presses with releases");
    bool only_releases=true;for(DWORD i=0;i<count;++i) only_releases&=events[i].dwData==0;
    require(only_releases,"focus loss never delivers stale controller press");
    count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,DIGDD_PEEK);
    require(count==3,"repeated inactive peek preserves pending releases");
    count=16;keyboard->GetDeviceData(16,events,&count,0);
    require(count==3&&events[0].dwData==0,"focus releases support legacy native event size");
    count=16;require(FAILED(keyboard->GetDeviceData(sizeof(events[0]),events,&count,0)),"empty inactive queue preserves native input error");
    simulated.active=true;simulated.start=false;simulated.menu_x=0;count=16;
    keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    require(count==0,"focus recovery does not replay discarded presses");
    simulated.start=true;count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    require(count==2&&events[0].dwData==0x80,"fresh Start press works after focus recovery");
    // Simulate a temporarily stalled consumer. The final release must survive
    // even when it occurs after all 64 synthetic event slots have filled.
    for(unsigned i=0;i<32;++i) {
        simulated.start=(i%2)!=0;count=0;
        keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    }
    simulated.start=false;count=0;keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    std::vector<DIDEVICEOBJECTDATA> drained;
    for(unsigned i=0;i<6;++i) {
        count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
        drained.insert(drained.end(),events,events+count);
    }
    require(drained.size()==66,"full event queue retries the final release after consumer resumes");
    require(drained[64].dwOfs==DIK_RCONTROL&&drained[64].dwData==0&&
        drained[65].dwOfs==DIK_RETURN&&drained[65].dwData==0,"both native Start keys finish released after queue pressure");
    count=16;keyboard->GetDeviceData(sizeof(events[0]),events,&count,0);
    require(count==0,"retried release is not duplicated after queue drains");
    mouse->Release();keyboard->Release();input->Release();DestroyWindow(window);
    printf("PASS %u actual Windows input-hook checks\n",checks);return 0;
}
