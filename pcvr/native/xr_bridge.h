#pragma once
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

using ProbeLog=void(*)(const char*,...);
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
void configure_xr_replay(float yaw_degrees,bool passive,bool combat=false,bool cursor_toggle=false);
#endif
void configure_xr(bool enabled,float units_per_metre,unsigned eye_size,ProbeLog log,float gun_pitch_degrees=15,bool haptics=true,bool aim_down_reload=true,float down_reload_degrees=55,float haptic_scale=1,bool aiming_cursor=true,bool dual_wield=false);
struct XrGameInput { bool active=false,aim_valid=false,fire=false,reload=false,start=false,back=false; float x=320,y=240,menu_x=0,menu_y=0; };
XrGameInput xr_game_input();
unsigned xr_eye_size();
bool xr_frame_ready();
bool xr_aim_cursor_visible();
bool xr_hud_vertex(unsigned eye,float x,float y,const D3DVIEWPORT7& source,const D3DMATRIX& projection,float& out_x,float& out_y);
bool xr_pointer_vertex(unsigned eye,float& x,float& y);
bool xr_gun_matrices(unsigned eye,D3DMATRIX& view,D3DMATRIX& projection);
bool xr_hand_gun_matrices(unsigned hand,unsigned eye,D3DMATRIX& view,D3DMATRIX& projection);
bool xr_hand_pointer_vertex(unsigned hand,unsigned eye,float& x,float& y);
bool xr_dual_wield_enabled();
struct XrMagazineControl {int hand=-1;unsigned reload_mask=0;};
void xr_enable_independent_magazines(bool enabled);
XrMagazineControl xr_take_magazine_control();
void xr_publish_magazines(int right,int left);
void xr_configure_ammo_gauges(bool enabled);
bool xr_magazine_gauges(int& right,int& left);
void xr_configure_health_gauge(bool enabled);
void xr_publish_health(int current,int maximum);
bool xr_health_gauge(int& current,int& maximum);
bool xr_replace_native_status();
bool xr_ammo_gauge_vertex(unsigned eye,float x,float y,float& out_x,float& out_y);
void xr_native_cursor_poll();
void xr_native_mouse_poll();
void xr_native_present();
void xr_set_game_projection(const D3DMATRIX& projection);
void xr_set_cinematic_guard(bool cinematic);
void begin_xr_frame();
bool xr_eye_matrices(unsigned eye,const D3DMATRIX& game_view,const D3DMATRIX& game_projection,
    D3DMATRIX& view,D3DMATRIX& projection,bool flat_pass=false);
void submit_xr_frame(IDirect3DDevice7* game_device,IDirectDrawSurface7* eye_atlas=nullptr);
