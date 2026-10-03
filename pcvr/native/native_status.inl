// Read-only health source. Native HUD/audio routines are never intercepted:
// their deferred sprites are filtered separately at eye-atlas submission.
static bool status_source_verified=false;
static void native_health_present(){
    if(!status_source_verified)return;
    auto base=ammo_game_base;
    DWORD mode=*reinterpret_cast<DWORD*>(base+0x5C8E98),variant=*reinterpret_cast<DWORD*>(base+0x5CA08C);
    if((mode!=6&&mode!=7)||variant!=0||base[0x5CA0F4]!=2){xr_publish_health(-1,-1);return;}
    int health=*reinterpret_cast<short*>(base+0x5A5C66);
    // This is the arcade cap used by the native health-recovery routine.
    // Player +8 is previous-frame health, not maximum health.
    int maximum=*reinterpret_cast<int*>(base+0x5A2440);
    xr_publish_health(health,maximum);
    static int last=-1,last_max=-1;
    if(health!=last||maximum!=last_max){last=health;last_max=maximum;log_line("VR_HEALTH native=%d maximum=%d health_rva=5A5C66 maximum_rva=5A2440 read_only=1",health,maximum);}
}
static bool supported_status_image(BYTE* base){
    if(!base)return false;
    const BYTE ammo_call[]={0xE8,0x5E,0x38,0,0},health_call[]={0xE8,0x25,0x35,0,0};
    const BYTE health_entry[]={0x8B,0x44,0x24,4,0x56,0x57},ammo_entry[]={0x83,0xEC,0x14,0x8B,0x44,0x24,0x18};
    const BYTE health_read[]={0x0F,0xBF,0x56,6},maximum_read[]={0xA1,0x40,0x24,0x9A,0};
    return memcmp(base+0x13F6D,ammo_call,5)==0&&memcmp(base+0x13F76,health_call,5)==0&&
        memcmp(base+0x174A0,health_entry,sizeof(health_entry))==0&&memcmp(base+0x177D0,ammo_entry,sizeof(ammo_entry))==0&&
        memcmp(base+0x17676,health_read,sizeof(health_read))==0&&memcmp(base+0x34F79,maximum_read,sizeof(maximum_read))==0;
}
static bool install_native_status(bool requested){
    xr_configure_health_gauge(false);
    if(!requested)return false;
    if(status_source_verified){xr_configure_health_gauge(true);return true;}
    // The ammo installer already authenticated the exact original file/PE32 image.
    if(!ammo_hook_installed||!supported_status_image(ammo_game_base)){log_line("VR_STATUS replacement=0 unsupported_image=1 native_fallback=1");return false;}
    status_source_verified=true;xr_configure_health_gauge(true);
    log_line("VR_STATUS replacement=1 verified_image=1 health_source_read_only=1 native_hud_and_audio_unmodified=1");
    return true;
}
