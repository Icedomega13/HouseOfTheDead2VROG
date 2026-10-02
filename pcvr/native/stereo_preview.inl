// Desktop preview or independent high-resolution OpenXR eye images.
// Only the two vertex layouts observed in HOTD2 are transformed here.
static bool stereo_enabled=false;
static float stereo_separation=6.4f; // Provisional game units, not calibrated metres.
static float preview_yaw=0,preview_pitch=0,preview_horizontal_fov=0;
static bool suppress_letterbox=true;
static bool headset_visibility=true;
static bool effect_audit=false;
static bool cinematic_seen=false;
static unsigned cinematic_tail=0;
static void update_cinematic_guard() {
    // Latch across native sub-scenes, then retire at presentation boundaries.
    // Two quiet frames cover the bar's final animation without delaying input.
    cinematic_tail=cinematic_seen?2:cinematic_tail?cinematic_tail-1:0;
    xr_set_cinematic_guard(cinematic_tail!=0);cinematic_seen=false;
}
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
static bool overlay_audit=false;
#endif
static void configure_stereo() {
    wchar_t path[32768]={}; DWORD n=GetModuleFileNameW(self,path,32768);
    auto slash=wcsrchr(path,L'\\');
    if(!n||n>=32768||!slash||slash-path+24>=32768) return;
    wcscpy_s(slash+1,32768-(slash+1-path),L"pcvr-probe.ini");
    configure_timing_research(path);
    configure_texture_pack(path);
    stereo_enabled=GetPrivateProfileIntW(L"Stereo",L"Enabled",0,path)!=0;
    UINT value=GetPrivateProfileIntW(L"Stereo",L"SeparationMilliunits",6400,path);
    if(value>100000) value=100000;
    stereo_separation=static_cast<float>(value)/1000.0f;
    bool xr=GetPrivateProfileIntW(L"OpenXR",L"Enabled",0,path)!=0;
    UINT units=GetPrivateProfileIntW(L"OpenXR",L"UnitsPerMetre",10,path);
    if(units<1||units>10000) units=10;
    UINT eye_size=GetPrivateProfileIntW(L"OpenXR",L"EyeSize",800,path);
    eye_size=std::clamp(eye_size,400u,1600u);
    high_resolution=xr||GetPrivateProfileIntW(L"Stereo",L"HighResolution",0,path)!=0;
    suppress_letterbox=GetPrivateProfileIntW(L"OpenXR",L"SuppressLetterbox",1,path)!=0;
    headset_visibility=GetPrivateProfileIntW(L"OpenXR",L"HeadsetVisibility",1,path)!=0;
    effect_audit=GetPrivateProfileIntW(L"OpenXR",L"EffectAudit",0,path)!=0;
    hide_unused_player_two=GetPrivateProfileIntW(L"OpenXR",L"HideUnusedPlayerTwo",1,path)!=0;
    log_line("VR_HUD hide_unused_player_two=%d ammo_placement_unchanged=1",hide_unused_player_two);
    int pitch_milli=static_cast<int>(GetPrivateProfileIntW(L"OpenXR",L"GunPitchMilliDegrees",15000,path));
    float gun_pitch=static_cast<float>(std::clamp(pitch_milli,-45000,45000))/1000;
    bool haptics=GetPrivateProfileIntW(L"OpenXR",L"Haptics",1,path)!=0;
    int haptic_percent=static_cast<int>(GetPrivateProfileIntW(L"OpenXR",L"HapticStrength",100,path));
    if(haptic_percent<0||haptic_percent>200) haptic_percent=100;
    bool down_reload=GetPrivateProfileIntW(L"OpenXR",L"AimDownReload",1,path)!=0;
    UINT down_degrees=GetPrivateProfileIntW(L"OpenXR",L"DownReloadDegrees",55,path);
    down_degrees=std::clamp(down_degrees,35u,85u);
    bool aiming_cursor=GetPrivateProfileIntW(L"OpenXR",L"AimingCursor",1,path)!=0;
    configure_xr(xr,static_cast<float>(units),eye_size,log_line,gun_pitch,haptics,down_reload,static_cast<float>(down_degrees),haptic_percent/100.0f,aiming_cursor);
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    overlay_audit=GetPrivateProfileIntW(L"OpenXR",L"ReplayOverlayAudit",0,path)!=0;
    int replay_yaw=static_cast<int>(GetPrivateProfileIntW(L"OpenXR",L"ReplayYawDegrees",0,path));
    bool passive=GetPrivateProfileIntW(L"OpenXR",L"ReplayPassive",0,path)!=0;
    bool combat=GetPrivateProfileIntW(L"OpenXR",L"ReplayCombat",0,path)!=0;
    bool cursor_toggle=GetPrivateProfileIntW(L"OpenXR",L"ReplayCursorToggle",0,path)!=0;
    configure_xr_replay(static_cast<float>(replay_yaw),passive,combat,cursor_toggle);
    log_line("REPLAY_TEST yaw_degrees=%d passive=%d",replay_yaw,passive);
#endif
    log_line("VR_CALIBRATION units_per_metre=%u gun_pitch_down_degrees=%.4g",units,gun_pitch);
    log_line("VR_RELOAD aim_down_enabled=%d down_degrees=%u rearm_degrees=%u",down_reload,down_degrees,down_degrees-20);
    if(xr) install_input_bridge();
    if(xr) stereo_enabled=true;
    int yaw=static_cast<int>(GetPrivateProfileIntW(L"Stereo",L"PreviewYawDegrees",0,path));
    int pitch=static_cast<int>(GetPrivateProfileIntW(L"Stereo",L"PreviewPitchDegrees",0,path));
    int fov=static_cast<int>(GetPrivateProfileIntW(L"Stereo",L"PreviewHorizontalFovDegrees",0,path));
    preview_yaw=static_cast<float>(std::clamp(yaw,-80,80))*DirectX::XM_PI/180;
    preview_pitch=static_cast<float>(std::clamp(pitch,-80,80))*DirectX::XM_PI/180;
    preview_horizontal_fov=fov>=30&&fov<=140?static_cast<float>(fov)*DirectX::XM_PI/180:0;
    log_line("STEREO enabled=%d separation_game_units=%.6g",stereo_enabled,stereo_separation);
    log_line("PREVIEW yaw=%d pitch=%d horizontal_fov=%d",yaw,pitch,fov);
}
template<class Draw> static HRESULT stereo_draw(void* object,DWORD fvf,void* vertices,DWORD count,Draw draw) {
    if(!stereo_enabled) return draw(vertices);
    if((fvf!=0x112 && fvf!=0x1c4)||!vertices||!count||count>1000000) {
        static LONG warnings=0;
        if(InterlockedIncrement(&warnings)<=3) log_line("STEREO unsupported layout=%08lx count=%lu; mono fallback",fvf,count);
        return draw(vertices);
    }
    auto device=static_cast<IDirect3DDevice7*>(object);
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    if(overlay_audit&&fvf==0x1c4) {
        auto p=static_cast<const float*>(vertices);
        IDirectDrawSurface7* texture=nullptr;DDSURFACEDESC2 td={};td.dwSize=sizeof(td);std::string texture_key="none";
        if(SUCCEEDED(device->GetTexture(0,&texture))&&texture){texture->GetSurfaceDesc(&td);if(texture_dump_enabled)replacement_texture(texture);texture_key=cached_texture_key(texture);texture->Release();}
        static std::vector<std::string> seen;
        std::string key=std::to_string(count)+":"+std::to_string(td.dwWidth)+":"+std::to_string(td.dwHeight)+":"+texture_key+":"+
            std::to_string(static_cast<int>(std::clamp(p[2],-10.0f,10.0f)*10000))+":"+std::to_string(static_cast<int>(std::clamp(p[3],-10.0f,10.0f)*10000));
        if(seen.size()<256&&std::find(seen.begin(),seen.end(),key)==seen.end()) {
            seen.push_back(key);
            D3DMATRIX projection={};device->GetTransform(D3DTRANSFORMSTATE_PROJECTION,&projection);
            log_line("RHW_AUDIT frame=%ld caller=%p parent=%p count=%lu texture=%lux%lu xyzrhw=%.6g,%.6g,%.6g,%.6g projection_xy=%.6g,%.6g projection_z=%.6g,%.6g,%.6g,%.6g texture_key=%s uv=%.5g,%.5g color=%08lx",scene_count,replay_draw_caller,replay_draw_parent,count,td.dwWidth,td.dwHeight,p[0],p[1],p[2],p[3],projection._11,projection._22,projection._33,projection._34,projection._43,projection._44,texture_key.c_str(),p[6],p[7],reinterpret_cast<const DWORD*>(p)[4]);
        }
    }
    if(overlay_audit&&fvf==0x112&&count==4){
        IDirectDrawSurface7* texture=nullptr;DDSURFACEDESC2 td={};td.dwSize=sizeof(td);std::string texture_key="none";
        if(SUCCEEDED(device->GetTexture(0,&texture))&&texture){texture->GetSurfaceDesc(&td);if(texture_dump_enabled)replacement_texture(texture);texture_key=cached_texture_key(texture);texture->Release();}
        static std::vector<std::string> seen_xyz;
        std::string key=std::to_string(td.dwWidth)+":"+std::to_string(td.dwHeight)+":"+texture_key;
        if(seen_xyz.size()<128&&std::find(seen_xyz.begin(),seen_xyz.end(),key)==seen_xyz.end()){
            seen_xyz.push_back(key);auto p=static_cast<const float*>(vertices);D3DMATRIX world={},view={};
            device->GetTransform(D3DTRANSFORMSTATE_WORLD,&world);device->GetTransform(D3DTRANSFORMSTATE_VIEW,&view);
            log_line("XYZ_QUAD_AUDIT frame=%ld caller=%p texture=%lux%lu xyz=%.6g,%.6g,%.6g world_translation=%.6g,%.6g,%.6g view_translation=%.6g,%.6g,%.6g texture_key=%s",scene_count,replay_draw_caller,td.dwWidth,td.dwHeight,p[0],p[1],p[2],world._41,world._42,world._43,view._41,view._42,view._43,texture_key.c_str());
        }
    }
#endif
    D3DVIEWPORT7 viewport={}; D3DMATRIX view={},projection={};
    if(FAILED(device->GetViewport(&viewport)) || FAILED(device->GetTransform(D3DTRANSFORMSTATE_VIEW,&view)) ||
        FAILED(device->GetTransform(D3DTRANSFORMSTATE_PROJECTION,&projection)) ||
        viewport.dwWidth<2 || viewport.dwHeight<2) return draw(vertices);
    using Transform=HRESULT(WINAPI*)(void*,D3DTRANSFORMSTATETYPE,LPD3DMATRIX);
    auto set_transform=original_slot<Transform>(object,11);
    bool audit_effect=false;DDSURFACEDESC2 effect_texture={};effect_texture.dwSize=sizeof(effect_texture);std::string effect_key="uncached";
    static unsigned effect_samples=0;
    static std::vector<std::array<int,6>> effect_positions;
    if(effect_audit&&fvf==0x1c4&&count==4&&effect_samples<96&&xr_game_input().fire){
        IDirectDrawSurface7* texture=nullptr;
        if(SUCCEEDED(device->GetTexture(0,&texture))&&texture){
            if(SUCCEEDED(texture->GetSurfaceDesc(&effect_texture)))
                audit_effect=(effect_texture.dwWidth==32&&effect_texture.dwHeight==32)||
                    (effect_texture.dwWidth==64&&effect_texture.dwHeight==64)||
                    (effect_texture.dwWidth==512&&effect_texture.dwHeight==64);
            if(audit_effect) effect_key=cached_texture_key(texture);
            texture->Release();
        }
        if(audit_effect){
            float low_x=1e20f,high_x=-1e20f,low_y=1e20f,high_y=-1e20f;
            for(unsigned i=0;i<4;++i){auto p=reinterpret_cast<const float*>(static_cast<const BYTE*>(vertices)+i*32);
                if(!std::isfinite(p[0])||!std::isfinite(p[1])||std::fabs(p[0])>100000||std::fabs(p[1])>100000){audit_effect=false;break;}
                low_x=std::min(low_x,p[0]);high_x=std::max(high_x,p[0]);low_y=std::min(low_y,p[1]);high_y=std::max(high_y,p[1]);
            }
            if(audit_effect){
                std::array<int,6> key={static_cast<int>(effect_texture.dwWidth),static_cast<int>(effect_texture.dwHeight),
                    static_cast<int>((low_x+high_x)/32),static_cast<int>((low_y+high_y)/32),static_cast<int>((high_x-low_x)/16),static_cast<int>((high_y-low_y)/16)};
                // Repeated ammo icons must not exhaust the trace before an impact.
                if(std::find(effect_positions.begin(),effect_positions.end(),key)!=effect_positions.end())audit_effect=false;
                else effect_positions.push_back(key);
            }
        }
    }
    bool flat_xyz=false,letterbox=false;
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    unsigned shot_capture=0;
#endif
    // Native cinematic bars are four-vertex, camera-aligned quads at Z=1,
    // using the ordinary scene projection rather than a special near plane.
    if(fvf==0x112&&count==4&&projection._34>0&&projection._43<0) {
        using namespace DirectX;
        D3DMATRIX world={};
        if(SUCCEEDED(device->GetTransform(D3DTRANSFORMSTATE_WORLD,&world))) {
            XMFLOAT4X4 w,v;memcpy(&w,&world,sizeof(w));memcpy(&v,&view,sizeof(v));
            bool near_effect=hotd2_xr::near_screen_effect(XMLoadFloat4x4(&w),XMLoadFloat4x4(&v),vertices,count);
            flat_xyz=near_effect||hotd2_xr::unit_depth_plane(XMLoadFloat4x4(&w)*XMLoadFloat4x4(&v),vertices,count);
            if(near_effect) {
                static unsigned reports=0;
                if(reports++<4) log_line("VR_SCREEN_EFFECT rotated native shot quad recognized for HUD-distance eye pass; desktop preserved");
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
                static unsigned captures=0;
                if(overlay_audit&&captures<3) shot_capture=++captures;
#endif
            }
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
            if(flat_xyz&&overlay_audit) {
                float bounds[]={1,-1,1,-1};
                for(unsigned i=0;i<4;++i) {
                    XMFLOAT3 p;memcpy(&p,static_cast<const BYTE*>(vertices)+i*32,sizeof(p));
                    auto camera=XMVector3TransformCoord(XMLoadFloat3(&p),XMLoadFloat4x4(&w)*XMLoadFloat4x4(&v));
                    float x=XMVectorGetX(camera),y=XMVectorGetY(camera);
                    bounds[0]=std::min(bounds[0],x);bounds[1]=std::max(bounds[1],x);bounds[2]=std::min(bounds[2],y);bounds[3]=std::max(bounds[3],y);
                }
                static std::vector<std::array<int,4>> observed;
                std::array<int,4> quantized;for(unsigned i=0;i<4;++i) quantized[i]=static_cast<int>(std::round(bounds[i]*1000));
                if(bounds[0]<-.48f&&bounds[1]>.48f&&observed.size()<128&&std::find(observed.begin(),observed.end(),quantized)==observed.end()) {
                    observed.push_back(quantized);IDirectDrawSurface7* texture=nullptr;DDSURFACEDESC2 td={};td.dwSize=sizeof(td);
                    if(SUCCEEDED(device->GetTexture(0,&texture))&&texture) {texture->GetSurfaceDesc(&td);texture->Release();}
                    log_line("OVERLAY_AUDIT frame=%ld x=%.5g..%.5g y=%.5g..%.5g native_bar=%d texture=%lux%lu",scene_count,bounds[0],bounds[1],bounds[2],bounds[3],hotd2_xr::cinematic_bar(XMLoadFloat4x4(&w)*XMLoadFloat4x4(&v),vertices,count),td.dwWidth,td.dwHeight);
                }
            }
#endif
            letterbox=hotd2_xr::cinematic_bar(XMLoadFloat4x4(&w)*XMLoadFloat4x4(&v),vertices,count);
            if(letterbox) {
                IDirectDrawSurface7* texture=nullptr;
                HRESULT hr=device->GetTexture(0,&texture);
                if(FAILED(hr)||texture) letterbox=false;
                if(texture) texture->Release();
            }
            if(letterbox) cinematic_seen=true;
            letterbox=letterbox&&suppress_letterbox;
            static unsigned reports=0;
            if(flat_xyz&&!near_effect&&reports++<3) log_line("SCREEN_PLANE XYZ overlay at native Z=1 moved to 2 metres");
        }
    }
    if(fvf==0x112&&(scene_count==1500||scene_count==3000)) {
        static LONG sampled_scene=-1;static unsigned samples=0;
        if(sampled_scene!=scene_count) {sampled_scene=scene_count;samples=0;}
        if(samples++<12) {
            using namespace DirectX;
            D3DMATRIX world={};device->GetTransform(D3DTRANSFORMSTATE_WORLD,&world);
            XMFLOAT4X4 w,v;memcpy(&w,&world,sizeof(w));memcpy(&v,&view,sizeof(v));
            auto camera=XMLoadFloat4x4(&w)*XMLoadFloat4x4(&v);
            XMFLOAT3 low={1e30f,1e30f,1e30f},high={-1e30f,-1e30f,-1e30f};
            for(DWORD i=0;i<count;++i) {
                auto point=reinterpret_cast<const XMFLOAT3*>(static_cast<const BYTE*>(vertices)+i*32);
                XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(point),camera));
                low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
                high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);
            }
            log_line("NATIVE_SCALE scene=%ld vertices=%lu camera_bounds=(%.5g,%.5g,%.5g)-(%.5g,%.5g,%.5g) world_basis=(%.5g,%.5g,%.5g)",scene_count,count,
                low.x,low.y,low.z,high.x,high.y,high.z,XMVectorGetX(XMVector3Length(camera.r[0])),XMVectorGetX(XMVector3Length(camera.r[1])),XMVectorGetX(XMVector3Length(camera.r[2])));
        }
    }
    std::vector<BYTE> adjusted;
    // Both observed layouts have a 32-byte stride; validate through the FVF gate above.
    if(fvf==0x1c4) adjusted.resize(static_cast<size_t>(count)*32);
    HRESULT result=S_OK;
    IDirectDrawSurface7* desktop=nullptr;
    bool atlas=ensure_eye_target(device);
    if(atlas) {
        // Preserve the original desktop image for game input and diagnostics.
        result=draw(vertices);
        if(xr_frame_ready()&&hide_player_two_draw(device,fvf,vertices,count,viewport,static_cast<uint32_t>(presentation_count))) return result;
        if(xr_frame_ready()&&hide_aim_cursor_draw(device,fvf,vertices,count,viewport)) return result;
        if(letterbox) {
            static unsigned reports=0;if(reports++<4) log_line("VR_LETTERBOX native bar preserved on desktop, suppressed in eye atlas");
            return result;
        }
        if(FAILED(device->GetRenderTarget(&desktop))||!desktop) return result;
        if(FAILED(device->SetRenderTarget(eye_atlas,0))) {desktop->Release();return result;}
    }
    EyeTextureOverride texture_override(device,atlas&&fvf==0x112&&!flat_xyz);
    for(unsigned eye=0;eye<2;++eye) {
        D3DVIEWPORT7 eye_viewport=viewport;
        eye_viewport.dwX=viewport.dwX+eye*(viewport.dwWidth/2);
        eye_viewport.dwY=viewport.dwY+viewport.dwHeight/4;
        eye_viewport.dwWidth=viewport.dwWidth/2;
        eye_viewport.dwHeight=viewport.dwHeight/2;
        if(atlas) {eye_viewport={eye*xr_eye_size(),0,xr_eye_size(),xr_eye_size(),viewport.dvMinZ,viewport.dvMaxZ};}
        HRESULT state=device->SetViewport(&eye_viewport);
        void* data=vertices;
        bool visible=true;
        if(fvf==0x112) {
            D3DMATRIX eye_view=view,eye_projection=projection;
            bool tracked=xr_eye_matrices(eye,view,projection,eye_view,eye_projection,flat_xyz);
            if(!tracked) {
                using namespace DirectX;
                XMFLOAT4X4 base,rotated;memcpy(&base,&view,sizeof(base));
                auto rotation=XMMatrixRotationRollPitchYaw(preview_pitch,preview_yaw,0);
                XMStoreFloat4x4(&rotated,XMLoadFloat4x4(&base)*XMMatrixInverse(nullptr,rotation));
                memcpy(&eye_view,&rotated,sizeof(eye_view));
                eye_view._41+=(eye==0?0.5f:-0.5f)*stereo_separation;
                if(preview_horizontal_fov>0&&projection._11!=0) {
                    eye_projection._11=1/std::tan(preview_horizontal_fov/2);
                    eye_projection._22=eye_projection._11*projection._22/projection._11;
                }
            }
            if(SUCCEEDED(state)) state=set_transform(object,D3DTRANSFORMSTATE_VIEW,&eye_view);
            if(SUCCEEDED(state)) state=set_transform(object,D3DTRANSFORMSTATE_PROJECTION,&eye_projection);
        } else {
            memcpy(adjusted.data(),vertices,adjusted.size());
            for(DWORD i=0;i<count;++i) {
                auto p=reinterpret_cast<float*>(adjusted.data()+i*32);
                float x=(p[0]-viewport.dwX)/viewport.dwWidth,y=(p[1]-viewport.dwY)/viewport.dwHeight;
                if(!xr_hud_vertex(eye,p[0],p[1],viewport,projection,x,y)&&xr_frame_ready()) visible=false;
                p[0]=x*eye_viewport.dwWidth+eye_viewport.dwX;
                p[1]=y*eye_viewport.dwHeight+eye_viewport.dwY;
            }
            data=adjusted.data();
            if(audit_effect){
                const auto p=static_cast<const float*>(vertices);auto out=reinterpret_cast<const float*>(data);auto aim=xr_game_input();
                log_line("EFFECT_AUDIT frame=%ld sample=%u eye=%u texture=%lux%lu native_xy_z_rhw=%.6g,%.6g,%.6g,%.6g eye_xy=%.6g,%.6g aim=%.6g,%.6g visible=%d viewport=%lu,%lu,%lu,%lu projection_xy=%.6g,%.6g texture_key=%s uv=%.5g,%.5g color=%08lx",scene_count,effect_samples,eye,effect_texture.dwWidth,effect_texture.dwHeight,p[0],p[1],p[2],p[3],out[0],out[1],aim.x,aim.y,visible,viewport.dwX,viewport.dwY,viewport.dwWidth,viewport.dwHeight,projection._11,projection._22,effect_key.c_str(),p[6],p[7],reinterpret_cast<const DWORD*>(p)[4]);
                if(eye==1)++effect_samples;
            }
        }
        HRESULT hr=FAILED(state)?state:visible?draw(data):S_OK;
        if(FAILED(hr)) result=hr;
    }
    HRESULT restored_view=set_transform(object,D3DTRANSFORMSTATE_VIEW,&view);
    HRESULT restored_projection=set_transform(object,D3DTRANSFORMSTATE_PROJECTION,&projection);
    HRESULT restored_target=S_OK;
    if(desktop) {restored_target=device->SetRenderTarget(desktop,0);desktop->Release();}
    HRESULT restored_viewport=device->SetViewport(&viewport);
    if(FAILED(restored_view)||FAILED(restored_projection)||FAILED(restored_viewport)||FAILED(restored_target)) log_line("STEREO restore error view=%08lx projection=%08lx viewport=%08lx target=%08lx",restored_view,restored_projection,restored_viewport,restored_target);
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    if(shot_capture&&atlas) {
        // At most three pairs, from owned targets only. Include the shot draw
        // immediately; an arbitrary end-of-scene capture can miss a short flash.
        capture_frame(device,220000+shot_capture*10+1);
        SavedGameState saved(device,false,log_line,"shot_capture");
        if(saved.bind(eye_atlas)) capture_frame(device,220000+shot_capture*10+2);
    }
#endif
    return result;
}
