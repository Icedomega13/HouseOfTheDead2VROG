#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "xr_bridge.h"
static bool dual=true,fire=false,auto_reload=false,independent=false;
static XrMagazineControl pending;
static int published[2]={};
bool xr_dual_wield_enabled(){return dual;}
void xr_enable_independent_magazines(bool enabled){independent=enabled;}
XrMagazineControl xr_take_magazine_control(){auto out=pending;pending.reload_mask=0;return out;}
void xr_publish_magazines(int right,int left){published[0]=right;published[1]=left;}
static bool health_configured=false;
static int published_health=-1,published_maximum=-1;
void xr_configure_health_gauge(bool enabled){health_configured=enabled;}
void xr_publish_health(int current,int maximum){published_health=current;published_maximum=maximum;}
bool xr_replace_native_status(){return health_configured&&independent&&published_health>=0&&published_health<=published_maximum&&published_maximum>0&&published_maximum<=9;}
static void log_line(const char*,...){}
#include "native_ammo.inl"
#include "native_status.inl"
static unsigned checks=0,calls=0,reload_sounds=0;
static void require(bool ok,const char* label){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}}
static short& field(unsigned offset){return *reinterpret_cast<short*>(ammo_game_base+offset);}
static void __cdecl fake_engine(void*) {
    ++calls;if(fire&&field(0x5A5C7C)>0){--field(0x5A5C7C);if(!field(0x5A5C7C)){field(0x5A5C7E)=1;field(0x5A5C80)=0;}}
    if(auto_reload)field(0x5A5C7C)=6;
}
static void __cdecl fake_reload(void*){++reload_sounds;field(0x5A5C7C)=6;field(0x5A5C7E)=0;}
int main(){
    auto image=static_cast<BYTE*>(VirtualAlloc(nullptr,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(image!=nullptr,"allocate owned game image fixture");ammo_game_base=image;original_ammo_update=fake_engine;original_ammo_reload=fake_reload;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x80);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->OptionalHeader.ImageBase=0x400000;nt->OptionalHeader.SizeOfImage=0x600000;
    BYTE call[]={0xE8,0x4D,0x0A,0,0},routine[]={0x83,0xEC,8,0x53,0x55,0x8B,0x6C,0x24,0x14},dec[]={0x48,0x66,0x89,0x46,0x1C},reload[]={0x66,0xC7,0x41,0x1C,6,0};
    memcpy(image+0x13EEE,call,5);memcpy(image+0x14940,routine,9);memcpy(image+0x149F3,dec,5);memcpy(image+0x14B69,reload,6);
    BYTE reload_entry[]={0x8B,0x44,0x24,0x04,0x8B,0x40,0x34};memcpy(image+0x14B30,reload_entry,sizeof(reload_entry));
    require(supported_ammo_image(image),"exact call/routine/decrement/reload signatures accepted");
    image[0x149F3]^=1;require(!supported_ammo_image(image),"changed native decrement rejected");image[0x149F3]^=1;
    nt->OptionalHeader.SizeOfImage=0x1000;require(!supported_ammo_image(image),"undersized image rejected before RVA access");nt->OptionalHeader.SizeOfImage=0x600000;
    require(!supported_ammo_image(nullptr),"invalid image safely rejected");
    require(!supported_ammo_file(),"test executable cannot qualify as supported game");
    BYTE actor[64]={};auto player=reinterpret_cast<DWORD*>(actor+0x34);
    auto mode=reinterpret_cast<DWORD*>(image+0x5C8E98);auto variant=reinterpret_cast<DWORD*>(image+0x5CA08C);
    *mode=6;*variant=0;field(0x5A5C7C)=6;field(0x5C8E80)=3;field(0x5A5C66)=4;field(0x5A5DAC)=123;
    auto update=[&](int hand,bool shoot,unsigned mask=0){pending={hand,mask};fire=shoot;independent_ammo_update(actor);};
    update(0,false);require(published[0]==6&&published[1]==6,"both magazines start full");
    for(int i=0;i<6;++i)update(0,true);
    require(published[0]==0&&published[1]==6&&native_magazines.consumed[0]==6,"six engine shots empty right only");
    update(0,true);require(native_magazines.consumed[0]==6,"empty trigger spends no round");
    auto_reload=true;update(0,false);auto_reload=false;
    require(published[0]==0&&field(0x5A5C7C)==0&&native_magazines.prevented_refills==1,"native automatic refill cannot bypass independent reload");
    update(1,true);require(published[0]==0&&published[1]==5,"left fires while right remains empty");
    update(1,false,1);require(published[0]==6&&published[1]==5,"lower right reloads only right while left stays selected");
    update(0,true);require(published[0]==5&&published[1]==5,"reloaded right can shoot");
    update(0,false,2);require(published[0]==5&&published[1]==6,"lower left reloads only left");
    update(0,false,3);require(published[0]==6&&published[1]==6,"B refills both magazines");
    update(1,false);require(published[1]==6,"ignored native input does not spend a virtual round");
    for(int i=0;i<12;++i)update(i%2,true);
    require(published[0]==0&&published[1]==0,"twelve alternating actual shots exhaust both six-round magazines");
    require(field(0x5A5C66)==4&&field(0x5A5DAC)==123&&field(0x5C8E80)==3,"health/player-two/credit state remains untouched");
    field(0x5C8E80)=2;field(0x5A5C7C)=6;update(0,false);
    require(published[0]==6&&published[1]==6,"native new-life reset replenishes both");
    update(1,true);field(0x5A5C7C)=6;update(1,false);
    require(published[1]==6,"native out-of-update reset reconciles selected hand");
    *player=1;int saved=published[0];update(0,true);
    require(published[0]==saved,"player-two actor is delegated without virtual ammo processing");*player=0;
    *mode=5;update(0,false);require(!native_magazines.ready,"attract/demo invalidates state and delegates");
    *mode=6;field(0x5A5C7C)=6;update(0,false);
    *variant=1;update(0,false);require(!native_magazines.ready,"original-mode weapon items stay native");*variant=0;
    dual=false;update(0,false);require(!native_magazines.ready,"single-gun path delegates");dual=true;
    field(0x5A5C7C)=99;update(0,false);require(!native_magazines.ready&&field(0x5A5C7C)==99,"unexpected ammo value is not overwritten");
    require(calls>25,"native function is invoked on every supported and fallback update");
    *mode=6;*variant=0;field(0x5A5C7C)=6;native_magazines={};reload_sounds=0;
    update(0,false);for(unsigned i=0;i<6;++i)update(0,true);
    field(0x5A5C80)=40;update(1,true);
    require(published[0]==0&&published[1]==5&&field(0x5A5C7E)==0,"loaded left clears active prompt while right stays empty");
    update(0,true);require(field(0x5A5C7E)==1&&field(0x5A5C80)==40,"switching back restores right empty prompt and native voice timer");
    require(native_magazines.consumed[0]==6,"dry-fire input at zero ammo creates no extra consumed rounds");
    ++field(0x5A5C80);update(1,false);update(0,true);
    require(field(0x5A5C80)==41,"native HUD timer advancement survives alternating guns");
    auto_reload=true;update(0,false);auto_reload=false;
    require(field(0x5A5C7C)==0&&field(0x5A5C80)==41,"blocked automatic refill preserves pending native voice timing");
    update(1,false,1);require(reload_sounds==1&&published[0]==6&&published[1]==5,"right-only reload invokes native sound once and preserves active left ammo");
    require(field(0x5A5C7E)==0&&field(0x5A5C80)==0,"other-hand reload cannot attach right prompt to loaded left");
    update(0,false);require(field(0x5A5C7E)==0&&field(0x5A5C80)==0,"reloaded gun returns with cleared empty/voice state");
    update(0,false,1);require(reload_sounds==1,"reload gesture on full gun does not play false refill sound");
    update(0,true);update(1,false,3);require(reload_sounds==2&&published[0]==6&&published[1]==6,"B refills two non-full guns with one native reload sound");
    update(1,false);require(reload_sounds==2,"no repeated sound after reload request is consumed");
    for(unsigned i=0;i<6;++i)update(1,true);field(0x5A5C80)=130;update(0,false);update(1,true);
    require(field(0x5A5C80)==130&&field(0x5A5C7E)==1,"left empty prompt retains later native voice phase independently");
    field(0x5C8E80)=2;field(0x5A5C7C)=6;update(0,false);update(1,false);
    require(field(0x5A5C80)==0&&native_magazines.prompt_ticks[0]==0,"new life clears stale per-hand voice counters");
    ammo_hook_installed=true;*mode=6;*variant=0;native_ammo_present();require(independent,"arcade presentation enables independent input handling");
    *variant=1;native_ammo_present();require(!independent&&!native_magazines.ready,"original mode restores shared reload controls");
    *variant=0;*mode=5;native_ammo_present();require(!independent,"attract mode retains normal native controls");
    *mode=7;native_ammo_present();require(independent,"supported active mode restores independent handling");ammo_hook_installed=false;
    require(!install_independent_ammo(true)&&memcmp(image+0x13EEE,call,5)==0,"unsupported host cannot install call patch or modify owned fixture");
    BYTE ac[]={0xE8,0x5E,0x38,0,0},hc[]={0xE8,0x25,0x35,0,0};
    BYTE he[]={0x8B,0x44,0x24,4,0x56,0x57},ae[]={0x83,0xEC,0x14,0x8B,0x44,0x24,0x18};
    BYTE hr[]={0x0F,0xBF,0x56,6},mr[]={0xA1,0x40,0x24,0x9A,0};
    memcpy(image+0x13F6D,ac,5);memcpy(image+0x13F76,hc,5);memcpy(image+0x174A0,he,sizeof(he));memcpy(image+0x177D0,ae,sizeof(ae));
    memcpy(image+0x17676,hr,sizeof(hr));memcpy(image+0x34F79,mr,sizeof(mr));
    require(supported_status_image(image),"exact HUD calls, entrypoints and health/cap reads authenticated");
    for(unsigned rva:{0x13F6Du,0x13F76u,0x174A0u,0x177D0u,0x17676u,0x34F79u}){
        image[rva]^=1;require(!supported_status_image(image),"changed HUD/read signature rejects replacement");image[rva]^=1;
    }
    require(!install_native_status(true)&&!health_configured&&memcmp(image+0x13F6D,ac,5)==0,"health source requires authenticated installed ammo hook");
    ammo_hook_installed=true;
    require(install_native_status(true)&&health_configured,"verified health source enables floating health");
    require(memcmp(image+0x13F6D,ac,5)==0&&memcmp(image+0x13F76,hc,5)==0,"native HUD calls remain byte-for-byte unchanged");
    health_configured=independent=true;*mode=6;*variant=0;*player=0;image[0x5CA0F4]=2;
    field(0x5A5C66)=3;field(0x5A5C68)=1;*reinterpret_cast<int*>(image+0x5A2440)=5;
    native_health_present();
    require(published_health==3&&published_maximum==5,"health comes from current health and native recovery cap, not previous health or lives");
    require(field(0x5A5C66)==3&&field(0x5A5C68)==1&&field(0x5C8E80)==2,"health presentation never modifies health, history or lives");
    field(0x5A5C66)=2;native_health_present();require(published_health==2&&published_maximum==5,"damage updates exact displayed health");
    field(0x5A5C66)=4;native_health_present();require(published_health==4&&published_maximum==5,"native health reward updates displayed health");
    field(0x5A5C66)=0;native_health_present();require(published_health==0,"death can publish zero even without a native HUD call");
    *variant=1;native_health_present();require(published_health==-1,"Original-mode health and item HUD stay native");*variant=0;
    image[0x5CA0F4]=4;native_health_present();require(published_health==-1,"inactive/join HUD state invalidates floating status");image[0x5CA0F4]=2;
    *mode=5;native_health_present();require(published_health==-1,"attract/menu context cannot hide original HUD");*mode=6;
    require(!install_native_status(false)&&!health_configured,"disabled preference leaves native status visible");
    published_health=-1;status_source_verified=false;native_health_present();require(published_health==-1,"unverified source is never read");
    VirtualFree(image,0,MEM_RELEASE);printf("PASS %u production native ammo/status checks (owned fixture, simulated engine)\n",checks);
}
