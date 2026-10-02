// Exercise production bridge transforms with synthetic poses; does not create an XR session.
#define xrApplyHapticFeedback test_apply_haptic
#define xrStopHapticFeedback test_stop_haptic
#define xrAcquireSwapchainImage test_acquire_image
#define xrWaitSwapchainImage test_wait_image
#define xrReleaseSwapchainImage test_release_image
#define xrEndFrame test_end_frame
#define xrPollEvent test_poll_event
#define xrWaitFrame test_wait_frame
#define xrBeginFrame test_begin_frame
#include "xr_bridge.cpp"
#undef xrApplyHapticFeedback
#undef xrStopHapticFeedback
#undef xrAcquireSwapchainImage
#undef xrWaitSwapchainImage
#undef xrReleaseSwapchainImage
#undef xrEndFrame
#undef xrPollEvent
#undef xrWaitFrame
#undef xrBeginFrame
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static unsigned pulse_count=0;
static XrHapticVibration last_pulse={};
static XrResult haptic_result=XR_SUCCESS;
static unsigned haptic_stop_count=0;
static XrResult haptic_stop_result=XR_SUCCESS;
static XrAction last_stop_action=XR_NULL_HANDLE;
static XrResult acquire_result=XR_SUCCESS,release_result=XR_SUCCESS,end_result=XR_SUCCESS;
static std::vector<XrResult> wait_results;
static std::vector<XrSwapchain> waited_handles;
static unsigned wait_count=0,release_count=0,end_count=0;
static uint32_t ended_layers=99;
static XrDuration waited_timeout=0;
static XrResult frame_wait_result=XR_SUCCESS,frame_begin_result=XR_SUCCESS;
static unsigned frame_wait_count=0,frame_begin_count=0;
XRAPI_ATTR XrResult XRAPI_CALL test_poll_event(XrInstance,XrEventDataBuffer*) {return XR_EVENT_UNAVAILABLE;}
XRAPI_ATTR XrResult XRAPI_CALL test_wait_frame(XrSession,const XrFrameWaitInfo*,XrFrameState* frame) {
    ++frame_wait_count;frame->shouldRender=XR_FALSE;frame->predictedDisplayTime=100000000;
    return frame_wait_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_begin_frame(XrSession,const XrFrameBeginInfo*) {
    ++frame_begin_count;return frame_begin_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_acquire_image(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    *index=1;return acquire_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_wait_image(XrSwapchain handle,const XrSwapchainImageWaitInfo* info) {
    waited_handles.push_back(handle);waited_timeout=info->timeout;
    auto index=wait_count++;return wait_results.empty()?XR_SUCCESS:wait_results[std::min(index,static_cast<unsigned>(wait_results.size()-1))];
}
XRAPI_ATTR XrResult XRAPI_CALL test_release_image(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    ++release_count;return release_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_end_frame(XrSession,const XrFrameEndInfo* info) {
    ++end_count;ended_layers=info->layerCount;return end_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_apply_haptic(XrSession,const XrHapticActionInfo*,const XrHapticBaseHeader* feedback) {
    ++pulse_count;last_pulse=*reinterpret_cast<const XrHapticVibration*>(feedback);return haptic_result;
}
XRAPI_ATTR XrResult XRAPI_CALL test_stop_haptic(XrSession,const XrHapticActionInfo* info) {
    ++haptic_stop_count;last_stop_action=info->action;return haptic_stop_result;
}
static void require(bool ok,const char* label) {++checks;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}}
static void close(float a,float b,const char* label) {require(std::fabs(a-b)<0.0001f,label);}
int main() {
    using namespace DirectX;
    configure_xr(true,100,800,nullptr,0);
    bridge.frame_open=true;bridge.valid_pose=true;bridge.recentered=true;
    bridge.origin.orientation.w=1;
    bridge.views[0].pose.orientation.w=bridge.views[1].pose.orientation.w=1;
    bridge.views[0].pose.position.x=-.032f;bridge.views[1].pose.position.x=.032f;
    bridge.views[0].fov={-.9f,.7f,.8f,-.75f};bridge.views[1].fov={-.7f,.9f,.8f,-.75f};
    D3DMATRIX gv={};gv._11=gv._22=gv._44=1;gv._33=-1;
    D3DMATRIX gp={};gp._11=2;gp._22=2.667f;gp._33=1.0001f;gp._34=1;gp._43=-.80008f;
    xr_set_game_projection(gp);
    D3DVIEWPORT7 source={0,0,640,480,0,1};
    for(unsigned eye=0;eye<2;++eye) {
        D3DMATRIX v={},p={};require(xr_eye_matrices(eye,gv,gp,v,p),"tracked matrices available");
        XMFLOAT4X4 vm,pm;memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
        auto projected=XMVector3TransformCoord(XMVectorSet(0,0,-200,1),XMLoadFloat4x4(&vm)*XMLoadFloat4x4(&pm));
        float x=0,y=0;require(xr_hud_vertex(eye,320,240,source,gp,x,y),"HUD projects to both eyes");
        close(x,(XMVectorGetX(projected)+1)/2,"HUD and world agree horizontally with asymmetric FOV");
        close(y,(1-XMVectorGetY(projected))/2,"HUD and world agree vertically");
    }
    bridge.input.active=true;bridge.input.fire=true;bridge.aim_valid=true;bridge.aim_pose.orientation.w=1;
    bridge.aim_pose.position={.2f,-.1f,-.3f};
    auto input=xr_game_input();require(input.aim_valid&&input.fire,"tracked controller can fire");
    close(input.x,352,"controller translation maps to native hit coordinates");close(input.y,256.002f,"controller vertical aim mapping");
    for(unsigned eye=0;eye<2;++eye) {
        float x=0,y=0;require(xr_pointer_vertex(eye,x,y),"aim dot projects to both eyes");
        D3DMATRIX v={},p={};xr_eye_matrices(eye,gv,gp,v,p);XMFLOAT4X4 vm,pm;
        memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
        auto projected=XMVector3TransformCoord(XMVectorSet(10,-5,-200,1),XMLoadFloat4x4(&vm)*XMLoadFloat4x4(&pm));
        close(x,(XMVectorGetX(projected)+1)/2,"dot matches native screen aim on the HUD plane in each eye");
        close(y,(1-XMVectorGetY(projected))/2,"dot vertical firing point");
        float hud_x=0,hud_y=0;xr_hud_vertex(eye,input.x,input.y,source,gp,hud_x,hud_y);
        close(x,hud_x,"native cursor and aim dot share horizontal stereo depth");
        close(y,hud_y,"native cursor and aim dot share vertical stereo depth");
        require(xr_gun_matrices(eye,v,p),"tracked gun view available");memcpy(&vm,&v,sizeof(vm));
        auto gun=XMVector3TransformCoord(XMVectorSet(0,0,0,1),XMLoadFloat4x4(&vm));
        close(XMVectorGetX(gun),eye==0?.232f:.168f,"gun uses physical metres and runtime eye separation");
        close(XMVectorGetY(gun),-.1f,"gun controller height");
    }
    bridge.input.active=false;require(!xr_game_input().fire,"unfocused input cannot fire");
    bridge.input.active=true;bridge.aim_valid=false;require(!xr_game_input().fire,"lost aim tracking cannot fire");
    bridge.valid_pose=false;D3DMATRIX v={},p={};require(!xr_eye_matrices(0,gv,gp,v,p),"lost head tracking rejects rendering");
    require(!xr_gun_matrices(0,v,p),"lost head tracking hides gun");
    bridge.valid_pose=true;bridge.aim_valid=true;bridge.input.active=true;bridge.input.fire=true;
    // Correct world units without changing the menu's physical placement or runtime IPD.
    float hud_before[2]={};
    for(unsigned eye=0;eye<2;++eye) {float y=0;xr_hud_vertex(eye,320,240,source,gp,hud_before[eye],y);}
    configure_xr(true,10,800,nullptr,0);
    for(unsigned eye=0;eye<2;++eye) {
        xr_eye_matrices(eye,gv,gp,v,p);close(v._41,eye==0?.32f:-.32f,"world scale converts actual IPD at ten units per metre");
        float x=0,y=0;xr_hud_vertex(eye,320,240,source,gp,x,y);
        close(x,hud_before[eye],"world scale does not change physical menu depth");
        auto flat=gp;flat._33=1;flat._43=-.001f;
        xr_eye_matrices(eye,gv,flat,v,p,true);XMFLOAT4X4 vm,pm;memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
        auto camera=XMVector3TransformCoord(XMVectorSet(0,0,-1,1),XMLoadFloat4x4(&vm));
        close(XMVectorGetZ(camera),20,"flat XYZ cinematic pass moves to two metres");
        auto projected=XMVector3TransformCoord(camera,XMLoadFloat4x4(&pm));
        close((XMVectorGetX(projected)+1)/2,x,"flat XYZ and HUD have the same stereo placement");
        xr_eye_matrices(eye,gv,flat,v,p,false);memcpy(&vm,&v,sizeof(vm));
        camera=XMVector3TransformCoord(XMVectorSet(0,0,-1,1),XMLoadFloat4x4(&vm));
        close(XMVectorGetZ(camera),1,"ordinary geometry with a tiny near plane retains its world scale");
    }
    // Native near-screen shot origin from the capture. Check the corrected
    // depth against the existing cursor plane while moving and turning the head.
    auto saved_left=bridge.views[0].pose,saved_right=bridge.views[1].pose;
    for(unsigned step=0;step<3;++step) {
        for(unsigned eye=0;eye<2;++eye) {
            auto& pose=bridge.views[eye].pose;
            pose.position={.15f*step+(eye==0?-.032f:.032f),.08f*step,-.04f*step};
            pose.orientation={0,std::sin(.08f*step),0,std::cos(.08f*step)};
            float x=0,y=0;require(xr_hud_vertex(eye,(1+.1343314f*gp._11)*320,(1-.07809962f*gp._22)*240,source,gp,x,y),"shot origin HUD available after head motion");
            xr_eye_matrices(eye,gv,gp,v,p,true);XMFLOAT4X4 vm,pm;memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
            auto projected=XMVector3TransformCoord(XMVectorSet(.1343314f,.07809962f,-1,1),XMLoadFloat4x4(&vm)*XMLoadFloat4x4(&pm));
            close((XMVectorGetX(projected)+1)/2,x,"shot flash horizontal origin stays with cursor after head motion");
            close((1-XMVectorGetY(projected))/2,y,"shot flash vertical origin stays with cursor after head motion");
            if(step==1) {
                xr_eye_matrices(eye,gv,gp,v,p,false);memcpy(&vm,&v,sizeof(vm));memcpy(&pm,&p,sizeof(pm));
                projected=XMVector3TransformCoord(XMVectorSet(.1343314f,.07809962f,-1,1),XMLoadFloat4x4(&vm)*XMLoadFloat4x4(&pm));
                require(std::fabs((XMVectorGetX(projected)+1)/2-x)>.1f,"old near-depth path reproduces large flash displacement");
            }
        }
    }
    bridge.views[0].pose=saved_left;bridge.views[1].pose=saved_right;
    // Lowering the barrel also lowers the aiming ray; the controller pivot stays fixed.
    configure_xr(true,10,800,nullptr,15);
    auto calibrated=calibrated_aim(1);XMFLOAT3 direction;
    XMStoreFloat3(&direction,XMVector3TransformNormal(XMVectorSet(0,0,1,0),calibrated));
    close(direction.y,-std::sin(XM_PI/12),"positive gun pitch lowers barrel");
    close(direction.z,std::cos(XM_PI/12),"gun forward after pitch correction");
    close(XMVectorGetX(calibrated.r[3]),.2f,"gun correction preserves controller pivot");
    auto adjusted=xr_game_input();
    require(adjusted.aim_valid&&adjusted.y>input.y,"lower barrel changes native shooting coordinates");
    XMFLOAT3 hit;require(aim_hit(hit),"corrected shooting ray hits aiming plane");
    float travel=(4-.3f)/direction.z;
    close(hit.y/10,-.1f+direction.y*travel,"gun and shooting ray share pitch correction");
    for(unsigned eye=0;eye<2;++eye) {
        float x=0,y=0,hx=0,hy=0;xr_pointer_vertex(eye,x,y);xr_hud_vertex(eye,adjusted.x,adjusted.y,source,gp,hx,hy);
        close(x,hx,"corrected gun dot agrees with native cursor horizontally");
        close(y,hy,"corrected gun dot agrees with native cursor vertically");
    }
    for(unsigned eye=0;eye<2;++eye) {
        xr_gun_matrices(eye,v,p);XMFLOAT4X4 vm;memcpy(&vm,&v,sizeof(vm));
        auto muzzle=XMVector3TransformCoord(XMVectorSet(0,0,.18f,1),XMLoadFloat4x4(&vm));
        close(XMVectorGetY(muzzle),-.1f+.18f*direction.y,"rendered muzzle follows calibrated ray");
    }
    configure_xr(true,10,800,nullptr,-15);
    calibrated=calibrated_aim(1);close(XMVectorGetY(calibrated.r[2]),std::sin(XM_PI/12),"negative pitch raises barrel");
    // Haptics acknowledge input edges, not native ammunition or successful hits.
    bridge.focused=true;bridge.session=static_cast<XrSession>(1);bridge.vibration=static_cast<XrAction>(2);
    bridge.input.fire=true;bridge.input.reload=false;controller_feedback();
    require(pulse_count==1&&last_pulse.duration==35000000,"one short fire pulse");close(last_pulse.amplitude,.3f,"gentle fire amplitude");
    controller_feedback();require(pulse_count==1,"held trigger does not repeat vibration");
    bridge.input.fire=false;controller_feedback();bridge.input.reload=true;controller_feedback();
    require(pulse_count==2&&last_pulse.duration==65000000,"distinct reload pulse");
    bridge.input.reload=false;controller_feedback();bridge.aim_valid=false;bridge.input.fire=true;controller_feedback();
    require(pulse_count==2,"lost controller tracking suppresses feedback");
    bridge.aim_valid=true;bridge.valid_pose=false;controller_feedback();require(pulse_count==2,"lost head tracking suppresses feedback");
    bridge.valid_pose=true;bridge.focused=false;controller_feedback();require(pulse_count==2,"unfocused session suppresses feedback");
    bridge.focused=true;bridge.input.fire=false;controller_feedback();bridge.haptics=false;bridge.input.fire=true;controller_feedback();
    require(pulse_count==2,"disabled haptics suppress feedback");
    bridge.haptics=true;bridge.input.fire=false;controller_feedback();haptic_result=XR_ERROR_RUNTIME_FAILURE;
    bridge.input.fire=true;controller_feedback();require(pulse_count==3&&bridge.haptics_failed,"haptic failure disables further vibration");
    bridge.input.fire=false;controller_feedback();bridge.input.fire=true;controller_feedback();require(pulse_count==3,"haptic failure is not retried every frame");
    require(xr_game_input().fire,"haptic failure leaves game input available");
    // Strength and cancellation affect feedback only, never native input state.
    haptic_result=XR_SUCCESS;bridge.haptics_failed=false;
    auto feedback_reset=[&](float strength=1) {
        configure_xr(true,10,800,nullptr,15,true,true,55,strength);
        clear_controls();bridge.focused=bridge.valid_pose=bridge.aim_valid=bridge.recentered=true;
        bridge.input.active=true;bridge.haptics_failed=false;
    };
    feedback_reset(.5f);bridge.input.fire=true;controller_feedback();
    close(last_pulse.amplitude,.15f,"50 percent scales fire strength");
    require(last_pulse.duration==35000000&&bridge.haptic_pulse_pending,"strength preserves pulse timing and successful output latch");
    bridge.input.fire=false;controller_feedback();bridge.input.reload=true;controller_feedback();
    close(last_pulse.amplitude,.075f,"50 percent scales reload strength");
    feedback_reset(2);bridge.input.fire=true;controller_feedback();close(last_pulse.amplitude,.6f,"200 percent doubles fire strength within OpenXR range");
    feedback_reset(9);close(bridge.haptic_scale,2,"native strength bounded above");
    feedback_reset(-1);close(bridge.haptic_scale,0,"native negative strength bounded to off");
    feedback_reset(std::numeric_limits<float>::quiet_NaN());close(bridge.haptic_scale,1,"nonfinite strength falls back to accepted default");
    feedback_reset(std::numeric_limits<float>::infinity());close(bridge.haptic_scale,1,"infinite strength falls back to accepted default");
    feedback_reset(0);unsigned before_silent=pulse_count;bridge.input.fire=true;controller_feedback();
    require(pulse_count==before_silent&&!bridge.haptic_pulse_pending&&xr_game_input().fire,"zero strength silences feedback while firing input remains available");
    feedback_reset();bridge.input.fire=bridge.input.reload=true;controller_feedback();
    require(last_pulse.duration==65000000,"reload acknowledgement takes priority on simultaneous input edges");
    close(last_pulse.amplitude,.15f,"simultaneous edge uses reload amplitude");
    unsigned before_stop=haptic_stop_count;bridge.focused=false;controller_feedback();
    require(haptic_stop_count==before_stop+1&&last_stop_action==bridge.vibration&&!bridge.haptic_pulse_pending,"focus loss requests one stop on the output action");
    controller_feedback();clear_controls();require(haptic_stop_count==before_stop+1,"empty unfocused frames do not repeat cancellation");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    bridge.aim_valid=false;controller_feedback();require(haptic_stop_count==before_stop+1,"controller tracking loss stops pending feedback");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    bridge.valid_pose=false;controller_feedback();require(haptic_stop_count==before_stop+1,"head tracking loss stops pending feedback");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    bridge.recentered=false;controller_feedback();require(haptic_stop_count==before_stop+1,"invalid reference origin stops pending feedback");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    clear_controls();require(haptic_stop_count==before_stop+1&&!bridge.input.active,"clearing controller state stops pending feedback");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    configure_xr(true,10,800,nullptr,15,false);require(haptic_stop_count==before_stop+1&&!bridge.haptic_pulse_pending,"disabling feedback cancels a pending pulse");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    configure_xr(true,10,800,nullptr,15,true,true,55,0);require(haptic_stop_count==before_stop+1,"setting strength to zero cancels pending feedback");
    feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    haptic_stop_result=XR_SESSION_NOT_FOCUSED;bridge.focused=false;clear_controls();
    require(haptic_stop_count==before_stop+1&&!bridge.haptics_failed,"not-focused cancellation status is expected, not a fatal error");
    haptic_stop_result=XR_SUCCESS;feedback_reset();haptic_result=XR_SESSION_NOT_FOCUSED;bridge.input.fire=true;controller_feedback();
    require(!bridge.haptic_pulse_pending&&!bridge.haptics_failed,"not-focused apply status does not record a delivered pulse");
    before_stop=haptic_stop_count;clear_controls();require(haptic_stop_count==before_stop,"undelivered feedback is not cancelled as if it played");
    haptic_result=XR_SUCCESS;feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    haptic_stop_result=XR_ERROR_RUNTIME_FAILURE;stop_controller_feedback();
    require(haptic_stop_count==before_stop+1&&bridge.haptics_failed&&xr_game_input().fire,"stop error disables feedback while preserving controller input");
    stop_controller_feedback();require(haptic_stop_count==before_stop+1,"stop error is not retried every frame");
    haptic_stop_result=XR_SUCCESS;feedback_reset();bridge.input.fire=true;controller_feedback();before_stop=haptic_stop_count;
    require(!frame_check(XR_ERROR_RUNTIME_FAILURE,"test lifecycle failure")&&haptic_stop_count==before_stop+1&&!bridge.input.active,"lifecycle failure cancels vibration and controller latches");
    bridge.focused=bridge.valid_pose=bridge.aim_valid=bridge.recentered=true;bridge.input.active=true;
    configure_xr(true,10,800,nullptr,15,true,true,55);
    bridge.haptics_failed=false;haptic_result=XR_SUCCESS;bridge.feedback_fire=bridge.feedback_reload=false;
    auto gesture_step=[&](float raw_down_degrees,int64_t milliseconds,bool button_reload=false) {
        float half=raw_down_degrees*XM_PI/360;
        bridge.aim_pose.orientation={-std::sin(half),0,0,std::cos(half)};
        bridge.frame.predictedDisplayTime=milliseconds*1000000;
        bridge.input={};bridge.input.active=true;bridge.input.fire=true;bridge.input.reload=button_reload;
        update_reload_gesture();
    };
    gesture_step(0,0);require(!bridge.input.reload&&bridge.reload_gesture.armed,"level calibrated gun arms reload gesture");
    gesture_step(25,100);require(!bridge.input.reload,"40-degree calibrated down aim does not reload");
    gesture_step(45,200);require(!bridge.input.reload,"gesture waits for dwell using calibrated barrel");
    gesture_step(45,280);require(bridge.input.reload&&!bridge.input.fire,"calibrated 60-degree dip sends reload and suppresses firing");
    unsigned before_gesture=pulse_count;controller_feedback();
    require(pulse_count==before_gesture+1&&last_pulse.duration==65000000,"gesture uses existing reload vibration");
    gesture_step(75,300);auto vertical=xr_game_input();
    require(vertical.reload&&!vertical.fire&&!vertical.aim_valid,"vertical aim still reloads when firing-plane intersection is unavailable");
    controller_feedback();require(pulse_count==before_gesture+1,"held gesture pulse does not repeat vibration");
    gesture_step(75,600);require(!bridge.input.reload,"holding gun down does not repeat native reload");
    gesture_step(0,1000);gesture_step(45,1100);bridge.focused=false;gesture_step(45,1200);
    require(!bridge.input.reload&&!bridge.reload_gesture.armed,"session focus loss cancels gesture");
    bridge.focused=true;gesture_step(45,1400);require(!bridge.input.reload,"focus recovery while down does not trigger reload");
    gesture_step(0,1500);bridge.aim_valid=false;gesture_step(45,1600);
    require(!bridge.input.reload&&!bridge.reload_gesture.armed,"controller tracking loss cancels pending gesture");
    gesture_step(75,1700,true);require(bridge.input.reload,"B-button reload remains available independently of the gesture");
    bridge.aim_valid=true;configure_xr(true,10,800,nullptr,15,true,false,55);
    gesture_step(0,2000);gesture_step(45,2100);gesture_step(45,2300);
    require(!bridge.input.reload,"profile can disable aim-down reload");
    gesture_step(45,2400,true);require(bridge.input.reload,"disabled gesture still permits B-button reload");
    configure_xr(true,10,800,nullptr,15,true,true,65);
    gesture_step(0,3000);gesture_step(45,3100);gesture_step(45,3300);
    require(!bridge.input.reload,"custom steeper reload angle respected");
    gesture_step(60,3400);gesture_step(60,3480);require(bridge.input.reload,"custom reload angle triggers deliberate dip");
    xr_set_cinematic_guard(true);
    require(!bridge.reload_gesture.armed&&!bridge.reload_gesture.has_reloaded,"entering cinematic cancels gesture latches");
    gesture_step(0,3500);gesture_step(60,3600);gesture_step(60,3700);
    require(!bridge.input.reload,"lowering gun in guarded cutscene does not send native skip/reload");
    gesture_step(60,3800,true);require(bridge.input.reload,"B remains deliberate native input during cutscenes");
    xr_set_cinematic_guard(false);gesture_step(60,3900);gesture_step(60,4000);
    require(!bridge.input.reload,"holding gun down when cinematic ends does not trigger reload");
    gesture_step(0,4100);gesture_step(60,4200);gesture_step(60,4280);
    require(bridge.input.reload,"raising and lowering after cinematic restores gesture");
    bridge.enabled=false;begin_xr_frame();require(!bridge.input.active&&!bridge.input.reload,"disabled XR releases pending gesture input");
    uint32_t image=0;auto handle=static_cast<XrSwapchain>(3);
    bridge.enabled=true;require(acquire_eye_image(handle,image)&&image==1&&bridge.enabled,"swapchain acquire succeeds normally");
    acquire_result=XR_ERROR_RUNTIME_FAILURE;require(!acquire_eye_image(handle,image)&&!bridge.enabled,"acquire failure stops further XR uploads");
    acquire_result=XR_SUCCESS;bridge.enabled=true;wait_results={XR_TIMEOUT_EXPIRED,XR_SUCCESS};wait_count=0;waited_handles.clear();
    require(wait_eye_image(handle)&&wait_count==2&&bridge.enabled,"positive timeout retried before image is ready");
    require(waited_handles.size()==2&&waited_handles[0]==handle&&waited_handles[1]==handle&&waited_timeout==XR_INFINITE_DURATION,"retry waits on same acquired image");
    bridge.enabled=true;wait_results={XR_TIMEOUT_EXPIRED};wait_count=0;unsigned releases_before=release_count;
    require(!wait_eye_image(handle)&&wait_count==3&&!bridge.enabled,"repeated timeout stops without treating image as ready");
    require(release_count==releases_before,"unready image not released by failed wait");
    bridge.enabled=true;wait_results={XR_ERROR_RUNTIME_FAILURE};wait_count=0;
    require(!wait_eye_image(handle)&&wait_count==1&&!bridge.enabled,"wait error stops XR immediately");
    bridge.enabled=true;wait_results={XR_SESSION_LOSS_PENDING};wait_count=0;
    require(wait_eye_image(handle)&&bridge.enabled,"other success-qualified wait result remains supported");
    require(release_eye_image(handle)&&bridge.enabled,"normal image release preserved");
    release_result=XR_ERROR_RUNTIME_FAILURE;
    require(!release_eye_image(handle)&&!bridge.enabled,"release failure stops acquiring with uncertain ownership");
    auto latch_controls=[&]() {
        bridge.frame_open=true;bridge.valid_pose=true;bridge.frame.shouldRender=true;
        bridge.input={};bridge.input.active=bridge.input.fire=bridge.input.reload=true;
        bridge.aim_valid=bridge.trigger_down=bridge.feedback_fire=bridge.feedback_reload=bridge.reload_gesture.armed=true;
    };
    latch_controls();submit_xr_frame(nullptr);
    require(end_count==1&&ended_layers==0&&!bridge.frame_open,"missing eye target closes empty frame once");
    require(!bridge.input.active&&!bridge.input.fire&&!bridge.input.reload&&!bridge.aim_valid,"empty frame releases all native controller input");
    require(!bridge.trigger_down&&!bridge.feedback_fire&&!bridge.feedback_reload&&!bridge.reload_gesture.armed,"empty frame resets trigger, feedback and gesture latches");
    submit_xr_frame(nullptr);require(end_count==1,"already closed frame is not submitted twice");
    latch_controls();end_result=XR_ERROR_RUNTIME_FAILURE;submit_xr_frame(nullptr);
    require(end_count==2&&!bridge.frame_open&&!xr_game_input().fire&&!bridge.input.reload,"failed frame end releases input and closes local frame");
    require(!bridge.enabled,"failed frame end stops further XR submission");
    begin_xr_frame();require(frame_wait_count==0&&end_count==2,"end failure is not retried on subsequent native frame");
    latch_controls();end_result=XR_SUCCESS;bridge.frame.shouldRender=false;submit_xr_frame(nullptr);
    require(end_count==3&&ended_layers==0&&!bridge.input.active,"non-rendering frame closes with no input held");
    latch_controls();bridge.valid_pose=false;submit_xr_frame(nullptr);
    require(end_count==4&&ended_layers==0&&!bridge.input.active,"lost-head-pose frame closes with no input held");
    auto ready_session=[&]() {
        latch_controls();bridge.enabled=true;bridge.attempted=true;bridge.running=true;
        bridge.frame_open=false;bridge.focused=false;
    };
    ready_session();frame_wait_result=XR_ERROR_RUNTIME_FAILURE;begin_xr_frame();
    require(frame_wait_count==1&&frame_begin_count==0&&!bridge.enabled&&!bridge.frame_open,"wait failure stops before beginning a frame");
    require(!bridge.valid_pose&&!bridge.input.active&&!bridge.trigger_down&&!bridge.reload_gesture.armed,"wait failure clears pose and controller latches");
    begin_xr_frame();require(frame_wait_count==1,"failed wait is not retried every native frame");
    ready_session();frame_wait_result=XR_SUCCESS;frame_begin_result=XR_ERROR_CALL_ORDER_INVALID;begin_xr_frame();
    require(frame_wait_count==2&&frame_begin_count==1&&!bridge.enabled&&!bridge.frame_open,"begin failure stops frame lifecycle");
    require(!bridge.valid_pose&&!bridge.input.active&&!bridge.input.reload,"begin failure releases controls and pose");
    begin_xr_frame();require(frame_wait_count==2&&frame_begin_count==1,"failed begin does not issue another wait or begin");
    ready_session();frame_begin_result=XR_FRAME_DISCARDED;begin_xr_frame();
    require(bridge.enabled&&bridge.frame_open&&!bridge.input.active,"success-qualified discarded begin remains a valid open frame");
    submit_xr_frame(nullptr);require(end_count==5&&!bridge.frame_open&&bridge.enabled,"non-rendering discarded frame closes normally without disabling VR");
    require(frame_check(XR_SESSION_LOSS_PENDING,"test success")&&bridge.enabled,"success-qualified lifecycle result remains supported");
    configure_xr(true,10,800,nullptr,0);
    require(xr_aim_cursor_visible(),"cursor visible by default");
    update_cursor_toggle(true,true);require(!xr_aim_cursor_visible(),"left Y first press hides cursor");
    update_cursor_toggle(true,true);require(!xr_aim_cursor_visible(),"holding Y cannot repeat toggle");
    bridge.frame_open=bridge.valid_pose=bridge.recentered=bridge.aim_valid=bridge.input.active=true;
    bridge.input.fire=true;bridge.aim_pose={};bridge.aim_pose.orientation.w=1;bridge.origin={};bridge.origin.orientation.w=1;
    float cursor_x=0,cursor_y=0;require(!xr_pointer_vertex(0,cursor_x,cursor_y),"hidden cursor produces no dot vertex");
    require(xr_game_input().aim_valid&&xr_game_input().fire,"cursor hidden preserves aiming and firing");
    clear_controls();require(!xr_aim_cursor_visible(),"focus loss retains visibility preference");
    update_cursor_toggle(true,true);require(!xr_aim_cursor_visible(),"held Y after refocus cannot toggle");
    update_cursor_toggle(true,false);update_cursor_toggle(true,true);
    require(xr_aim_cursor_visible(),"release and fresh press restores cursor");
    update_cursor_toggle(false,true);update_cursor_toggle(true,true);
    require(xr_aim_cursor_visible(),"inactive or failed action cannot create an edge on return");
    update_cursor_toggle(true,false);update_cursor_toggle(true,true);require(!xr_aim_cursor_visible(),"next intentional press toggles once");
    configure_xr(true,10,800,nullptr,0,true,true,55,1,false);
    require(!xr_aim_cursor_visible(),"cursor-off profile applied at launch");
    update_cursor_toggle(true,true);require(xr_aim_cursor_visible(),"cursor-off profile can toggle back on");
    printf("PASS %u production XR bridge checks (synthetic poses)\n",checks);return 0;
}
