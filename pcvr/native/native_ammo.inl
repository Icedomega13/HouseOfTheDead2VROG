// Opt-in arcade prototype. Wrap one verified player-update CALL in process memory;
// the on-disk EXE, native hit/damage routines and player-two state remain untouched.
#include <bcrypt.h>
#include "independent_magazines.h"
static BYTE* ammo_game_base=nullptr;
using NativeAmmoUpdate=void(__cdecl*)(void*);
static NativeAmmoUpdate original_ammo_update=nullptr;
static NativeAmmoUpdate original_ammo_reload=nullptr;
static hotd2_input::IndependentMagazines native_magazines;
static bool ammo_hook_installed=false;
static bool ammo_context_reported=false;
static bool ammo_context_enabled=false;
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
using NativeSound=void(__cdecl*)(DWORD);
static NativeSound audited_native_sound=nullptr;
static void __cdecl native_reload_audio_audit(DWORD key){
    log_line("REPLAY_AUDIO native_dispatch=1 kind=%s key=%08lx",key==0x3E16A9?"reload_sound":"reload_voice",key);
    audited_native_sound(key);
}
static void install_reload_audio_audit(BYTE* base){
    const DWORD offsets[]={0x14B83,0x18122};
    const BYTE signatures[][5]={{0xE8,0x48,0x84,0,0},{0xE8,0xA9,0x4E,0,0}};
    for(unsigned i=0;i<2;++i)if(memcmp(base+offsets[i],signatures[i],5)!=0){log_line("REPLAY_AUDIO audit_disabled=1 signature_mismatch=1");return;}
    audited_native_sound=reinterpret_cast<NativeSound>(base+0x1CFD0);
    for(unsigned i=0;i<2;++i){
        BYTE* call=base+offsets[i];DWORD old=0,ignored=0;
        if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)){log_line("REPLAY_AUDIO audit_disabled=1 protection_failed=1");return;}
        DWORD displacement=static_cast<DWORD>(reinterpret_cast<uintptr_t>(&native_reload_audio_audit)-reinterpret_cast<uintptr_t>(call+5));
        memcpy(call+1,&displacement,4);VirtualProtect(call,5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),call,5);
    }
    log_line("REPLAY_AUDIO audit_installed=1 production_build_unchanged=1");
}
#endif
static void native_ammo_present() {
    if(!ammo_hook_installed)return;
    DWORD mode=*reinterpret_cast<DWORD*>(ammo_game_base+0x5C8E98),variant=*reinterpret_cast<DWORD*>(ammo_game_base+0x5CA08C);
    bool enabled=xr_dual_wield_enabled()&&(mode==6||mode==7)&&variant==0;
    if(enabled!=ammo_context_enabled){
        ammo_context_enabled=enabled;native_magazines.invalidate();xr_publish_magazines(6,6);
        xr_enable_independent_magazines(enabled);
        log_line("VR_AMMO context_independent=%d native_mode=%lu variant=%lu original_mode_shared_fallback=%d",enabled,mode,variant,variant==1);
    }
}
static bool supported_ammo_image(BYTE* base) {
    IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS32 nt={};SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),base,&dos,sizeof(dos),&got)||got!=sizeof(dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>4096)return false;
    if(!ReadProcessMemory(GetCurrentProcess(),base+dos.e_lfanew,&nt,sizeof(nt),&got)||got!=sizeof(nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||nt.OptionalHeader.ImageBase!=0x400000||nt.OptionalHeader.SizeOfImage<0x5CA090)return false;
    const BYTE call[]={0xE8,0x4D,0x0A,0,0},routine[]={0x83,0xEC,0x08,0x53,0x55,0x8B,0x6C,0x24,0x14};
    const BYTE decrement[]={0x48,0x66,0x89,0x46,0x1C},reload[]={0x66,0xC7,0x41,0x1C,0x06,0};
    const BYTE reload_entry[]={0x8B,0x44,0x24,0x04,0x8B,0x40,0x34};
    return memcmp(base+0x13EEE,call,sizeof(call))==0&&memcmp(base+0x14940,routine,sizeof(routine))==0&&
        memcmp(base+0x149F3,decrement,sizeof(decrement))==0&&memcmp(base+0x14B69,reload,sizeof(reload))==0&&
        memcmp(base+0x14B30,reload_entry,sizeof(reload_entry))==0;
}
static bool supported_ammo_file() {
    wchar_t path[32768]={};if(!GetModuleFileNameW(nullptr,path,32768))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size={};bool ok=GetFileSizeEx(file,&size)&&size.QuadPart==1699840;
    std::vector<BYTE> bytes(ok?1699840:0);DWORD read=0;
    if(ok)ok=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)&&read==bytes.size();
    CloseHandle(file);if(!ok)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BYTE digest[32]={};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    ok=BCryptHash(algorithm,nullptr,0,bytes.data(),static_cast<ULONG>(bytes.size()),digest,sizeof(digest))>=0;
    BCryptCloseAlgorithmProvider(algorithm,0);
    const BYTE expected[]={0xC6,0xB4,0x11,0x67,0x88,0xB7,0xF6,0x8C,0x56,0x86,0x0F,0xB9,0xCC,0x8A,0x94,0xBF,0x98,0x4E,0x62,0x09,0x07,0x03,0x1E,0x4B,0xC3,0xDB,0x43,0x62,0x3D,0xBE,0x57,0x9A};
    return ok&&memcmp(digest,expected,sizeof(digest))==0;
}
static void __cdecl independent_ammo_update(void* actor) {
    auto base=ammo_game_base;
    DWORD player=actor?*reinterpret_cast<DWORD*>(static_cast<BYTE*>(actor)+0x34):99;
    DWORD mode=*reinterpret_cast<DWORD*>(base+0x5C8E98),variant=*reinterpret_cast<DWORD*>(base+0x5CA08C);
    // This CALL is the ordinary arcade weapon update. Original-mode items use a
    // different routine and are deliberately left alone; demo/menu never writes.
    if(player!=0||(mode!=6&&mode!=7)||variant!=0||!xr_dual_wield_enabled()) {
        if(player==0){native_magazines.invalidate();xr_publish_magazines(-1,-1);xr_take_magazine_control();}
        original_ammo_update(actor);return;
    }
    auto state=base+0x5A5C60;auto ammo=reinterpret_cast<short*>(state+0x1C);
    int lives=*reinterpret_cast<short*>(base+0x5C8E80);
    auto control=xr_take_magazine_control();
    bool play_reload=false;
    if(native_magazines.ready&&native_magazines.last_lives==lives&&*ammo==native_magazines.last_native){
        // The native HUD advances this timer after the weapon update. Capture it
        // on the next update, before switching to the other hand's magazine.
        int previous=native_magazines.selected;
        native_magazines.empty_prompt[previous]=*reinterpret_cast<short*>(state+0x1E);
        native_magazines.prompt_ticks[previous]=*reinterpret_cast<short*>(state+0x20);
        for(unsigned h=0;h<2;++h)if((control.reload_mask&(1u<<h))&&native_magazines.rounds[h]<6)play_reload=true;
    }
    if(!native_magazines.begin(*ammo,lives,control.hand,control.reload_mask)){xr_publish_magazines(-1,-1);original_ammo_update(actor);return;}
    int hand=native_magazines.selected,before=native_magazines.rounds[hand];
    unsigned consumed=native_magazines.consumed[hand],blocked=native_magazines.prevented_refills;
    // Use the game's own refill/sound routine once for a deliberate non-full
    // reload. Restore the selected gun below if the other gun was reloaded.
    if(play_reload){original_ammo_reload(actor);log_line("VR_AMMO native_reload_sound=1 reload_mask=%u",control.reload_mask);}
    *ammo=static_cast<short>(before);
    *reinterpret_cast<short*>(state+0x1E)=native_magazines.empty_prompt[hand];
    *reinterpret_cast<short*>(state+0x20)=native_magazines.prompt_ticks[hand];
    original_ammo_update(actor);
    int engine=*ammo;*ammo=static_cast<short>(native_magazines.finish(engine));
    if(*ammo){*reinterpret_cast<short*>(state+0x1E)=0;*reinterpret_cast<short*>(state+0x20)=0;}
    else if(engine>0){*reinterpret_cast<short*>(state+0x1E)=1;*reinterpret_cast<short*>(state+0x20)=native_magazines.prompt_ticks[hand];}
    xr_publish_magazines(native_magazines.rounds[0],native_magazines.rounds[1]);
    if(!ammo_context_reported||control.reload_mask||consumed!=native_magazines.consumed[hand]||blocked!=native_magazines.prevented_refills) {
        ammo_context_reported=true;
        log_line("VR_AMMO hand=%s right=%d left=%d engine_before=%d engine_after=%d consumed_right=%u consumed_left=%u reload_mask=%u prevented_refills=%u native_mode=%lu",
            hand?"left":"right",native_magazines.rounds[0],native_magazines.rounds[1],before,engine,
            native_magazines.consumed[0],native_magazines.consumed[1],control.reload_mask,native_magazines.prevented_refills,mode);
    }
}
static bool install_independent_ammo(bool requested) {
    if(!requested)return false;
    if(ammo_hook_installed)return true;
    auto base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    if(!xr_dual_wield_enabled()||!supported_ammo_file()||!supported_ammo_image(base)) {
        log_line("VR_AMMO independent=0 reason=unsupported_executable_or_profile shared_fallback=1");return false;
    }
    BYTE* call=base+0x13EEE;DWORD old=0;
    if(!VirtualProtect(call,5,PAGE_EXECUTE_READWRITE,&old)){log_line("VR_AMMO independent=0 protection_failed=%lu",GetLastError());return false;}
    ammo_game_base=base;original_ammo_update=reinterpret_cast<NativeAmmoUpdate>(base+0x14940);
    original_ammo_reload=reinterpret_cast<NativeAmmoUpdate>(base+0x14B30);
    BYTE patch[5]={0xE8};auto displacement=static_cast<DWORD>(reinterpret_cast<uintptr_t>(&independent_ammo_update)-reinterpret_cast<uintptr_t>(call+5));
    memcpy(patch+1,&displacement,4);memcpy(call,patch,5);
    DWORD ignored=0;bool restored=VirtualProtect(call,5,old,&ignored)!=FALSE;
    FlushInstructionCache(GetCurrentProcess(),call,5);ammo_hook_installed=true;
    log_line("VR_AMMO independent=1 capacity_per_hand=6 call_rva=13EEE verified_sha256=1 code_protection_restored=%d disk_executable_unchanged=1",restored);
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    install_reload_audio_audit(base);
#endif
    return true;
}
