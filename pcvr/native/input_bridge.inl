// Override only this game's imported cursor API and its own DirectInput devices.
// No SendInput, SetCursorPos, global input hooks, or desktop mouse movement.
static HWND input_game_window=nullptr;
struct InputDevice {
    void* object=nullptr;bool mouse=false,keyboard=false;unsigned samples=0;
    bool previous_keys[7]={};std::vector<DIDEVICEOBJECTDATA> events;
};
static InputDevice input_devices[16];
static unsigned input_device_count=0;
using CursorFn=BOOL(WINAPI*)(LPPOINT);
using InputCreateFn=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
static CursorFn original_cursor=nullptr;
static InputCreateFn original_input_create=nullptr;
static BOOL WINAPI vr_cursor(LPPOINT point) {
    auto input=xr_game_input();
    if(point&&input.active&&input.aim_valid&&IsWindow(input_game_window)) {
        static unsigned samples=0;
        if(samples++<3) log_line("INPUT tracked_cursor logical=(%.1f,%.1f) window=%p",input.x,input.y,input_game_window);
        RECT rect={};GetClientRect(input_game_window,&rect);
        point->x=static_cast<LONG>(input.x*(rect.right-rect.left)/640);
        point->y=static_cast<LONG>(input.y*(rect.bottom-rect.top)/480);
        return ClientToScreen(input_game_window,point);
    }
    return original_cursor(point);
}
static HRESULT WINAPI vr_input_state(void* object,DWORD size,LPVOID data) {
    using Fn=HRESULT(WINAPI*)(void*,DWORD,LPVOID);
    HRESULT hr=original_slot<Fn>(object,9)(object,size,data);
    InputDevice* device=nullptr;
    for(unsigned i=0;i<input_device_count;++i) if(input_devices[i].object==object) {device=&input_devices[i];break;}
    if(!device) return hr;
    if(device->samples++<3) log_line("INPUT poll object=%p mouse=%d keyboard=%d bytes=%lu hr=%08lx",object,device->mouse,device->keyboard,size,hr);
    auto input=xr_game_input();
    if(!data||!input.active) return hr;
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    if(device->mouse) {
        static unsigned audit=0;
        if(audit++%120==0) {
            auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
            const BYTE signature[]={0x6a,0x3c,0xc7,0x05,0x9c,0xda,0x7d,0x00,0x01,0,0,0};
            // Owned replay only; the test harness executable is not the game.
            if(memcmp(base+0x9E550,signature,sizeof(signature))==0)
                log_line("INPUT_AUDIT caller=%p mode=%lu fire=%d reload=%d",_ReturnAddress(),*reinterpret_cast<const DWORD*>(base+0x5C8E98),input.fire,input.reload);
        }
    }
#endif
    // Only the standard formats observed in the game are supported.
    bool mouse=device->mouse&&(size==sizeof(DIMOUSESTATE)||size==sizeof(DIMOUSESTATE2));
    bool keyboard=device->keyboard&&size==256;
    if(!mouse&&!keyboard) return hr;
    if(FAILED(hr)) memset(data,0,size);
    if(mouse) {
        auto state=static_cast<DIMOUSESTATE*>(data);
        if(input.aim_valid) {state->lX=0;state->lY=0;state->rgbButtons[0]=input.fire?0x80:0;}
        state->rgbButtons[1]=input.reload?0x80:0;
    } else {
        auto keys=static_cast<BYTE*>(data);
        // Native player-one Start is right Ctrl; Enter confirms menu selections.
        if(input.start) {keys[DIK_RCONTROL]=0x80;keys[DIK_RETURN]=0x80;}
        if(input.back) keys[DIK_ESCAPE]=0x80;
        if(input.menu_y>0.5f) keys[DIK_UP]=0x80;
        if(input.menu_y<-0.5f) keys[DIK_DOWN]=0x80;
        if(input.menu_x>0.5f) keys[DIK_RIGHT]=0x80;
        if(input.menu_x<-0.5f) keys[DIK_LEFT]=0x80;
    }
    static bool was_fire=false,was_reload=false,was_start=false;
    if(mouse&&((input.fire&&!was_fire)||(input.reload&&!was_reload))) log_line("INPUT controller fire=%d reload=%d aim=(%.1f,%.1f)",input.fire,input.reload,input.x,input.y);
    if(keyboard&&input.start&&!was_start) log_line("INPUT controller start=1");
    if(mouse) {was_fire=input.fire;was_reload=input.reload;}if(keyboard) was_start=input.start;
    return DI_OK;
}
static HRESULT WINAPI vr_input_coop(void* object,HWND window,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,HWND,DWORD);
    input_game_window=window;
    return original_slot<Fn>(object,13)(object,window,flags);
}
static HRESULT WINAPI vr_input_data(void* object,DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags) {
    using Fn=HRESULT(WINAPI*)(void*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
    DWORD capacity=count?*count:0;
    HRESULT hr=original_slot<Fn>(object,10)(object,size,data,count,flags);
    InputDevice* device=nullptr;
    for(unsigned i=0;i<input_device_count;++i) if(input_devices[i].object==object) {device=&input_devices[i];break;}
    if(!device||!device->keyboard||!count||(size!=16&&size!=sizeof(DIDEVICEOBJECTDATA))) return hr;
    auto input=xr_game_input();
    // A queued Start/navigation press must not reach the game after XR focus
    // loss. Keep releases so controls already consumed by the game can unwind.
    if(!input.active) device->events.erase(std::remove_if(device->events.begin(),device->events.end(),
        [](const DIDEVICEOBJECTDATA& event) {return (event.dwData&0x80)!=0;}),device->events.end());
    bool keys[]={input.active&&input.start,input.active&&input.start,input.active&&input.back,
        input.active&&input.menu_y>.5f,input.active&&input.menu_y<-.5f,input.active&&input.menu_x<-.5f,input.active&&input.menu_x>.5f};
    DWORD offsets[]={DIK_RCONTROL,DIK_RETURN,DIK_ESCAPE,DIK_UP,DIK_DOWN,DIK_LEFT,DIK_RIGHT};
    static DWORD sequence=0;
    for(unsigned i=0;i<7;++i) if(keys[i]!=device->previous_keys[i]) {
        DIDEVICEOBJECTDATA event={};event.dwOfs=offsets[i];event.dwData=keys[i]?0x80:0;
        event.dwTimeStamp=GetTickCount();event.dwSequence=++sequence;
        // The game requests buffered keyboard events every frame. Bound the queue on focus loss.
        if(device->events.size()<64) {
            device->events.push_back(event);device->previous_keys[i]=keys[i];
        }
        // If full, keep the last queued key state so an undelivered release is
        // retried on the next poll with space, instead of leaving native keys held.
    }
    if(!input.active&&device->events.empty()) return hr;
    DWORD native=SUCCEEDED(hr)?std::min(*count,capacity):0;
    DWORD added=std::min(capacity-native,static_cast<DWORD>(device->events.size()));
    if(data) for(DWORD i=0;i<added;++i) memcpy(reinterpret_cast<BYTE*>(data)+(native+i)*size,&device->events[i],size);
    if(!(flags&DIGDD_PEEK)) device->events.erase(device->events.begin(),device->events.begin()+added);
    *count=native+added;return DI_OK;
}
static void attach_input(void* device,REFGUID guid) {
    bool mouse=guid==GUID_SysMouse,keyboard=guid==GUID_SysKeyboard;
    log_line("INPUT device=%p guid=%08lx mouse=%d keyboard=%d",device,guid.Data1,mouse,keyboard);
    if((!mouse&&!keyboard)||input_device_count>=16) return;
    for(unsigned i=0;i<input_device_count;++i) if(input_devices[i].object==device) return;
    // Keep these two small input objects alive with the process-lifetime hook tables.
    static_cast<IUnknown*>(device)->AddRef();
    auto& entry=input_devices[input_device_count++];entry.object=device;entry.mouse=mouse;entry.keyboard=keyboard;
    install_hook(device,9,reinterpret_cast<void*>(&vr_input_state));
    install_hook(device,13,reinterpret_cast<void*>(&vr_input_coop));
    if(keyboard) install_hook(device,10,reinterpret_cast<void*>(&vr_input_data));
}
static HRESULT WINAPI vr_create_device(void* object,REFGUID guid,LPDIRECTINPUTDEVICEA* out,LPUNKNOWN outer) {
    using Fn=HRESULT(WINAPI*)(void*,REFGUID,LPDIRECTINPUTDEVICEA*,LPUNKNOWN);
    HRESULT hr=original_slot<Fn>(object,3)(object,guid,out,outer);
    if(SUCCEEDED(hr)&&out&&*out) attach_input(*out,guid);return hr;
}
static HRESULT WINAPI vr_create_device_ex(void* object,REFGUID guid,REFIID iid,LPVOID* out,LPUNKNOWN outer) {
    using Fn=HRESULT(WINAPI*)(void*,REFGUID,REFIID,LPVOID*,LPUNKNOWN);
    HRESULT hr=original_slot<Fn>(object,9)(object,guid,iid,out,outer);
    if(SUCCEEDED(hr)&&out&&*out) attach_input(*out,guid);return hr;
}
static HRESULT WINAPI vr_create_input(HINSTANCE instance,DWORD version,REFIID iid,LPVOID* out,LPUNKNOWN outer) {
    HRESULT hr=original_input_create(instance,version,iid,out,outer);
    log_line("INPUT DirectInputCreateEx version=%08lx iid=%08lx hr=%08lx",version,iid.Data1,hr);
    if(SUCCEEDED(hr)&&out&&*out&&(iid==IID_IDirectInput7A||iid==IID_IDirectInput7W)) {
        install_hook(*out,3,reinterpret_cast<void*>(&vr_create_device));
        install_hook(*out,9,reinterpret_cast<void*>(&vr_create_device_ex));
    }
    return hr;
}
static bool replace_game_import(const char* name,void* replacement,void** original) {
    auto base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva) return false;
    auto descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);
    for(;descriptor->Name;++descriptor) {
        if(!descriptor->OriginalFirstThunk) continue;
        auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto imported=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(strcmp(reinterpret_cast<char*>(imported->Name),name)) continue;
            DWORD old=0,ignored=0;
            if(!VirtualProtect(&slots->u1.Function,sizeof(void*),PAGE_READWRITE,&old)) return false;
            *original=reinterpret_cast<void*>(slots->u1.Function);
            InterlockedExchangePointer(reinterpret_cast<void**>(&slots->u1.Function),replacement);
            VirtualProtect(&slots->u1.Function,sizeof(void*),old,&ignored);return true;
        }
    }
    return false;
}
static void install_input_bridge() {
    bool cursor=replace_game_import("GetCursorPos",reinterpret_cast<void*>(&vr_cursor),reinterpret_cast<void**>(&original_cursor));
    bool input=replace_game_import("DirectInputCreateEx",reinterpret_cast<void*>(&vr_create_input),reinterpret_cast<void**>(&original_input_create));
    log_line("INPUT game_import_hooks cursor=%d directinput=%d",cursor,input);
}
