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
static XrAction last_pulse_action=XR_NULL_HANDLE;
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
XRAPI_ATTR XrResult XRAPI_CALL test_apply_haptic(XrSession,const XrHapticActionInfo* info,const XrHapticBaseHeader* feedback) {
    last_pulse_action=info->action;
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
    // Head-relative ammo widgets retain two-metre stereo disparity while the
    // head turns/translates; unlike world HUD they stay on the same view sides.
    float gauge_x[2]={},gauge_y[2]={};
    for(unsigned eye=0;eye<2;++eye)require(xr_ammo_gauge_vertex(eye,.58f,.5f,gauge_x[eye],gauge_y[eye]),"ammo gauge projects to each asymmetric eye");
    for(float angle:{0.0f,1.3f,3.0f}) {
        auto rotation=XMQuaternionRotationAxis(XMVectorSet(0,1,0,0),angle);
        for(unsigned eye=0;eye<2;++eye){
            auto& pose=bridge.views[eye].pose;
            XMFLOAT4 q;XMStoreFloat4(&q,rotation);pose.orientation={q.x,q.y,q.z,q.w};
            XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(eye? .032f:-.032f,0,0,0),rotation));
            pose.position={.45f+offset.x,.23f+offset.y,-.6f+offset.z};
        }
        for(unsigned eye=0;eye<2;++eye){float x=0,y=0;require(xr_ammo_gauge_vertex(eye,.58f,.5f,x,y),"gauge projects after head movement");
            close(x,gauge_x[eye],"gauge stays head relative horizontally");close(y,gauge_y[eye],"gauge stays head relative vertically");}
    }
    bridge.views[0].pose=saved_left;bridge.views[1].pose=saved_right;
    bridge.views[0].fov=bridge.views[1].fov={-.8f,.8f,.8f,-.8f};
    float gl=0,gr=0,gy=0;xr_ammo_gauge_vertex(0,.58f,.5f,gl,gy);xr_ammo_gauge_vertex(1,.58f,.5f,gr,gy);
    require(gl>gr&&gl-gr<.03f,"ammo panel uses real eye disparity at two metres");
    require(!xr_ammo_gauge_vertex(2,0,0,gl,gy),"invalid gauge eye rejected");
    require(!xr_ammo_gauge_vertex(0,std::numeric_limits<float>::quiet_NaN(),0,gl,gy),"nonfinite gauge point rejected");
    bridge.valid_pose=false;require(!xr_ammo_gauge_vertex(0,0,0,gl,gy),"head tracking loss hides gauges");bridge.valid_pose=true;
    bridge.views[0].fov={-.9f,.7f,.8f,-.75f};bridge.views[1].fov={-.7f,.9f,.8f,-.75f};
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
    require(pulse_count==1&&last_pulse.duration==55000000,"one short fire pulse");close(last_pulse.amplitude,.85f,"stronger default fire amplitude");
    controller_feedback();require(pulse_count==1,"held trigger does not repeat vibration");
    bridge.input.fire=false;controller_feedback();bridge.input.reload=true;controller_feedback();
    require(pulse_count==2&&last_pulse.duration==80000000,"distinct reload pulse");
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
    close(last_pulse.amplitude,.425f,"50 percent scales fire strength");
    require(last_pulse.duration==55000000&&bridge.haptic_pulse_pending,"strength preserves pulse timing and successful output latch");
    bridge.input.fire=false;controller_feedback();bridge.input.reload=true;controller_feedback();
    close(last_pulse.amplitude,.2f,"50 percent scales reload strength");
    feedback_reset(2);bridge.input.fire=true;controller_feedback();close(last_pulse.amplitude,1.0f,"200 percent saturates fire strength within OpenXR range");
    feedback_reset(9);close(bridge.haptic_scale,2,"native strength bounded above");
    feedback_reset(-1);close(bridge.haptic_scale,0,"native negative strength bounded to off");
    feedback_reset(std::numeric_limits<float>::quiet_NaN());close(bridge.haptic_scale,1,"nonfinite strength falls back to accepted default");
    feedback_reset(std::numeric_limits<float>::infinity());close(bridge.haptic_scale,1,"infinite strength falls back to accepted default");
    feedback_reset(0);unsigned before_silent=pulse_count;bridge.input.fire=true;controller_feedback();
    require(pulse_count==before_silent&&!bridge.haptic_pulse_pending&&xr_game_input().fire,"zero strength silences feedback while firing input remains available");
    feedback_reset();bridge.input.fire=bridge.input.reload=true;controller_feedback();
    require(last_pulse.duration==80000000,"reload acknowledgement takes priority on simultaneous input edges");
    close(last_pulse.amplitude,.4f,"simultaneous edge uses reload amplitude");
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
    require(pulse_count==before_gesture+1&&last_pulse.duration==80000000,"gesture uses existing reload vibration");
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
    // Dispatch both immutable press-time aim pairs through one native player.
    {
        hotd2_input::DualShotRouter router;
        bool tracked[]={true,true},down[]={true,true};float x[]={100,540},y[]={200,280};
        router.observe(true,tracked,down,x,y,1000);
        require(router.phase==hotd2_input::DualShotRouter::Down&&router.current.hand==0&&router.count==1,"simultaneous edges select right and retain left");
        router.present(1001);require(router.phase==hotd2_input::DualShotRouter::Down,"presentation without native polls cannot consume a shot");
        router.mouse_poll();router.present(1002);require(router.phase==hotd2_input::DualShotRouter::Down,"mouse poll alone cannot acknowledge an aiming pair");
        router.cursor_poll();router.present(1003);require(router.phase==hotd2_input::DualShotRouter::Up&&router.delivered[0]==1,"cursor/state pair inserts an explicit native release");
        x[1]=300;router.observe(true,tracked,down,x,y,1004);
        require(router.count==1&&router.queued==2,"held triggers do not generate extra shots");
        router.present(1005);require(router.phase==hotd2_input::DualShotRouter::Up,"release requires a native mouse poll");
        router.mouse_poll();router.present(1006);
        require(router.phase==hotd2_input::DualShotRouter::Down&&router.current.hand==1&&router.current.x==540,"left shot keeps its original aim while waiting");
        router.cursor_poll();router.mouse_poll();router.present(1007);router.mouse_poll();router.present(1008);
        require(router.phase==hotd2_input::DualShotRouter::Idle&&router.delivered[1]==1,"both hand requests dispatch exactly once");
        down[0]=down[1]=false;router.observe(true,tracked,down,x,y,1010);down[0]=down[1]=true;router.observe(true,tracked,down,x,y,1011);
        require(router.current.hand==1,"next simultaneous tie alternates priority");
        router.observe(false,tracked,down,x,y,1012);require(router.count==0&&router.phase==hotd2_input::DualShotRouter::Idle,"focus loss clears pending native presses");
        router.observe(true,tracked,down,x,y,1013);require(router.phase==hotd2_input::DualShotRouter::Idle,"held triggers on refocus require release");
        down[0]=down[1]=false;router.observe(true,tracked,down,x,y,1014);down[1]=true;router.observe(true,tracked,down,x,y,1015);
        require(router.current.hand==1,"left-only shot works without right trigger");
        tracked[1]=false;router.observe(true,tracked,down,x,y,1016);require(router.phase==hotd2_input::DualShotRouter::Up,"tracking loss releases selected hand");
        router.mouse_poll();router.observe(true,tracked,down,x,y,1017);router.present(1018);
        require(router.phase==hotd2_input::DualShotRouter::Idle,"repeated tracking loss does not strand release phase");
        tracked[1]=true;router.observe(true,tracked,down,x,y,1019);require(router.phase==hotd2_input::DualShotRouter::Idle,"held trigger on tracking return cannot shoot");
        down[1]=false;router.observe(true,tracked,down,x,y,1020);x[1]=-1;down[1]=true;router.observe(true,tracked,down,x,y,1021);
        require(router.phase==hotd2_input::DualShotRouter::Idle,"off-screen dual ray cannot inject native fire/reload");
        x[1]=500;down[1]=false;router.observe(true,tracked,down,x,y,1022);down[1]=true;router.observe(true,tracked,down,x,y,1023);
        router.present(1300);require(router.phase==hotd2_input::DualShotRouter::Up&&router.discarded>0,"unconsumed shot expires after a bounded delay");
    }
    configure_xr(true,10,800,nullptr,0,true,true,55,1,true,true);
    bridge.frame_open=bridge.valid_pose=bridge.focused=bridge.recentered=bridge.input.active=true;
    bridge.aim_valid=bridge.left_aim_valid=true;bridge.origin={};bridge.origin.orientation.w=1;
    bridge.aim_pose={};bridge.aim_pose.orientation.w=1;bridge.aim_pose.position.x=.2f;
    bridge.left_aim_pose={};bridge.left_aim_pose.orientation.w=1;bridge.left_aim_pose.position.x=-.2f;
    bridge.views[0].pose={};bridge.views[0].pose.orientation.w=1;
    auto right_input=hand_game_input(0),left_input=hand_game_input(1);
    require(right_input.aim_valid&&left_input.aim_valid&&right_input.x>left_input.x,"two calibrated controllers project to independent native aims");
    D3DMATRIX right_view={},left_view={},hand_projection={};
    require(xr_hand_gun_matrices(0,0,right_view,hand_projection)&&xr_hand_gun_matrices(1,0,left_view,hand_projection),"two gun meshes have independently tracked matrices");
    require(right_view._41>left_view._41,"left gun is located independently of right");
    bridge.aim_valid=false;require(!xr_hand_gun_matrices(0,0,right_view,hand_projection)&&xr_hand_gun_matrices(1,0,left_view,hand_projection),"right tracking loss does not disable tracked left gun");bridge.aim_valid=true;
    bridge.trigger_down=bridge.left_trigger_down=true;update_dual_shots();auto first=xr_game_input();
    require(first.fire&&first.x==right_input.x,"native fire and cursor refer to same selected right request");
    xr_native_cursor_poll();xr_native_mouse_poll();xr_native_present();require(!xr_game_input().fire,"native release separates hand requests");
    xr_native_mouse_poll();xr_native_present();auto second=xr_game_input();
    require(second.fire&&second.x==left_input.x,"queued left request reaches native cursor/fire pair");
    bridge.input.reload=true;update_dual_shots();require(!xr_game_input().fire&&bridge.shots.count==0,"shared reload cancels queued shooting");
    bridge.input.reload=false;update_dual_shots();require(!xr_game_input().fire,"reload while triggers held cannot synthesize a new press");
    bridge.trigger_down=bridge.left_trigger_down=false;update_dual_shots();bridge.left_trigger_down=true;bridge.left_fire=true;update_dual_shots();
    require(xr_game_input().fire&&bridge.shots.current.hand==1,"fresh left press after reload works");
    update_cursor_toggle(true,true);float lx=0,ly=0;require(!xr_hand_pointer_vertex(1,0,lx,ly),"left Y also hides left aiming marker");
    clear_controls();require(!bridge.left_aim_valid&&bridge.shots.phase==hotd2_input::DualShotRouter::Idle,"focus/lifecycle clear releases both hands and queued shots");
    bridge.session=static_cast<XrSession>(1);bridge.vibration=static_cast<XrAction>(2);bridge.left_vibration=static_cast<XrAction>(4);
    haptic_result=haptic_stop_result=XR_SUCCESS;bridge.haptics_failed=false;
    bridge.focused=bridge.input.active=bridge.valid_pose=bridge.recentered=bridge.aim_valid=bridge.left_aim_valid=true;
    bridge.left_fire=true;unsigned prior_pulses=pulse_count;controller_feedback();
    require(pulse_count==prior_pulses+1&&last_pulse_action==bridge.left_vibration&&bridge.left_haptic_pending,"left trigger acknowledges on left haptic output only");
    controller_feedback();require(pulse_count==prior_pulses+1,"left trigger hold does not repeat haptics");
    bridge.aim_valid=false;controller_feedback();require(bridge.left_haptic_pending,"right tracking loss does not cancel tracked left feedback");
    bridge.aim_valid=true;bridge.input.fire=true;controller_feedback();
    require(bridge.haptic_pulse_pending&&bridge.left_haptic_pending,"both hands can retain independent haptic latches");
    unsigned stops_before=haptic_stop_count;clear_controls();
    require(haptic_stop_count==stops_before+2&&!bridge.haptic_pulse_pending&&!bridge.left_haptic_pending,"focus clear cancels both controller outputs exactly once");
    configure_xr(true,10,800,nullptr,0,true,true,55,1,true,true);
    xr_enable_independent_magazines(true);
    bridge.focused=bridge.input.active=bridge.valid_pose=bridge.recentered=bridge.aim_valid=bridge.left_aim_valid=true;
    bridge.origin={};bridge.origin.orientation.w=1;
    auto magazine_gesture=[&](float right_down,float left_down,int64_t ms,bool b=false){
        auto pose=[](float angle){XrPosef p={};float half=angle*DirectX::XM_PI/360;p.orientation={-std::sin(half),0,0,std::cos(half)};return p;};
        bridge.aim_pose=pose(right_down);bridge.left_aim_pose=pose(left_down);bridge.frame.predictedDisplayTime=ms*1000000;
        bridge.input.reload=b;update_reload_gesture();
    };
    magazine_gesture(0,0,0);magazine_gesture(70,0,100);magazine_gesture(70,0,180);
    require(!bridge.input.reload&&bridge.magazine_reload_pulse[0]&&!bridge.magazine_reload_pulse[1],"right dip never sends global native reload or left haptic pulse");
    require(xr_take_magazine_control().reload_mask==1,"right dip requests only right magazine");
    require(xr_take_magazine_control().reload_mask==0,"reload request consumed once by native update");
    magazine_gesture(70,0,200);require(xr_take_magazine_control().reload_mask==0,"120ms gesture pulse cannot repeatedly refill magazine");
    magazine_gesture(70,70,300);magazine_gesture(70,70,380);
    require(!bridge.input.reload&&xr_take_magazine_control().reload_mask==2,"left dip requests only left magazine");
    magazine_gesture(70,70,1000);require(xr_take_magazine_control().reload_mask==0,"holding both guns down never repeats refill");
    magazine_gesture(0,0,1100,true);require(bridge.input.reload&&xr_take_magazine_control().reload_mask==3,"B keeps native behavior and requests both magazines");
    magazine_gesture(0,0,1150,true);require(xr_take_magazine_control().reload_mask==0,"held B refills once per press");
    magazine_gesture(0,0,1200,false);magazine_gesture(0,0,1300,true);require(xr_take_magazine_control().reload_mask==3,"fresh B press refills again");
    bridge.magazine_reload_mask=3;clear_controls();require(bridge.magazine_reload_mask==0,"focus loss discards deferred reload requests");
    bridge.focused=bridge.input.active=bridge.valid_pose=bridge.recentered=bridge.aim_valid=bridge.left_aim_valid=true;
    magazine_gesture(0,0,2000);xr_set_cinematic_guard(true);magazine_gesture(70,70,2100);magazine_gesture(70,70,2300);
    require(xr_take_magazine_control().reload_mask==0&&!bridge.input.reload,"cutscene guard blocks both gesture refills and skip input");xr_set_cinematic_guard(false);
    magazine_gesture(0,0,2400);bridge.trigger_down=bridge.left_trigger_down=false;update_dual_shots();
    bridge.trigger_down=bridge.left_trigger_down=true;update_dual_shots();xr_publish_magazines(0,6);
    require(xr_game_input().fire&&bridge.shots.current.hand==0,"empty right trigger reaches native dry-fire voice path at zero ammo");
    xr_native_cursor_poll();xr_native_mouse_poll();xr_native_present();xr_native_mouse_poll();xr_native_present();
    require(xr_game_input().fire&&bridge.shots.current.hand==1,"full left magazine still fires after empty right request");
    auto selected=xr_take_magazine_control();require(selected.hand==1,"native ammo update receives routed shooting hand");
    bridge.shots.cancel_hand(0);require(bridge.shots.phase==hotd2_input::DualShotRouter::Down&&bridge.shots.current.hand==1,"reloading idle right preserves active left shot");
    bridge.shots.cancel_hand(1);require(bridge.shots.phase==hotd2_input::DualShotRouter::Up,"reloading active hand releases its trigger");
    hotd2_input::DualShotRouter cancel_router;
    bool both_valid[]={true,true},both_down[]={true,true};float cancel_x[]={200,400},cancel_y[]={240,240};
    cancel_router.observe(true,both_valid,both_down,cancel_x,cancel_y,0);cancel_router.cancel_hand(1);
    require(cancel_router.count==0&&cancel_router.current.hand==0&&cancel_router.phase==hotd2_input::DualShotRouter::Down,"per-hand reload removes its pending shot while preserving other hand");
    configure_xr(true,10,800,nullptr,0,true,true,55,1,true,false);xr_enable_independent_magazines(true);
    require(!bridge.independent_magazines,"single-gun profile cannot enable independent magazines");
    configure_xr(true,10,800,nullptr,0,true,true,55,1,false,true);
    bridge.frame_open=bridge.valid_pose=bridge.recentered=bridge.input.active=true;
    xr_configure_ammo_gauges(true);xr_enable_independent_magazines(true);
    int gauge_right=-9,gauge_left=-9;
    require(!xr_magazine_gauges(gauge_right,gauge_left),"gauges wait for first native weapon update");
    xr_publish_magazines(2,5);require(xr_magazine_gauges(gauge_right,gauge_left)&&gauge_right==2&&gauge_left==5,"both gauges show independently published native counts");
    bridge.shots.current.hand=1;require(xr_magazine_gauges(gauge_right,gauge_left)&&gauge_right==2&&gauge_left==5,"switching firing hand cannot swap displayed counts");
    xr_publish_magazines(0,6);require(xr_magazine_gauges(gauge_right,gauge_left)&&gauge_right==0&&gauge_left==6,"empty right and full left displayed accurately");
    xr_set_cinematic_guard(true);require(!xr_magazine_gauges(gauge_right,gauge_left),"cinematic hides new gauges");xr_set_cinematic_guard(false);
    xr_configure_ammo_gauges(false);require(!xr_magazine_gauges(gauge_right,gauge_left),"gauge preference can disable display");xr_configure_ammo_gauges(true);
    bridge.input.active=false;require(!xr_magazine_gauges(gauge_right,gauge_left),"inactive input hides gauges");bridge.input.active=true;
    xr_publish_magazines(-1,6);require(!xr_magazine_gauges(gauge_right,gauge_left),"invalid native count hides stale gauges");
    xr_publish_magazines(6,7);require(!xr_magazine_gauges(gauge_right,gauge_left),"out of capacity native count hides gauges");
    xr_publish_magazines(6,6);xr_enable_independent_magazines(false);require(!xr_magazine_gauges(gauge_right,gauge_left),"shared-magazine/menu fallback never claims two separate clips");
    xr_enable_independent_magazines(true);require(!xr_magazine_gauges(gauge_right,gauge_left),"new arcade context waits for fresh counts");
    // Both hands use the same stronger pulse and runtime amplitude bounds.
    bridge.focused=bridge.aim_valid=bridge.left_aim_valid=true;bridge.haptics_failed=false;bridge.session=static_cast<XrSession>(1);
    bridge.vibration=static_cast<XrAction>(2);bridge.left_vibration=static_cast<XrAction>(3);
    bridge.input.fire=bridge.input.reload=false;bridge.left_fire=true;bridge.left_feedback_fire=bridge.left_feedback_reload=false;
    controller_feedback();require(last_pulse_action==bridge.left_vibration&&last_pulse.duration==55000000,"left firing uses stronger short recoil");close(last_pulse.amplitude,.85f,"left default kick matches right");
    bridge.left_fire=false;controller_feedback();bridge.input.reload=true;controller_feedback();
    require(last_pulse_action==bridge.left_vibration&&last_pulse.duration==80000000,"left reload has its own softer longer pulse");close(last_pulse.amplitude,.4f,"left reload amplitude matches right");
    bridge.left_fire=true;controller_feedback();require(last_pulse.duration==55000000,"left fresh fire while reload held uses fire rather than held reload pulse");
    int hp=-1,cap=-1;
    xr_configure_health_gauge(true);xr_publish_magazines(3,4);
    require(!xr_health_gauge(hp,cap)&&!xr_replace_native_status(),"unpublished health retains the native HUD");
    xr_publish_health(3,5);require(xr_health_gauge(hp,cap)&&hp==3&&cap==5&&xr_replace_native_status(),"health panel uses independent current health and native maximum");
    xr_publish_health(1,5);require(xr_health_gauge(hp,cap)&&hp==1&&cap==5,"damage cannot shrink the health meter capacity");
    xr_publish_health(0,5);require(xr_health_gauge(hp,cap)&&hp==0,"zero health displayed exactly");
    xr_publish_health(4,5);require(xr_health_gauge(hp,cap)&&hp==4,"native recovery reflected without estimated damage logic");
    xr_set_cinematic_guard(true);require(!xr_health_gauge(hp,cap)&&xr_replace_native_status(),"cinematic hides floating panel without reviving redundant status HUD");xr_set_cinematic_guard(false);
    bridge.input.active=false;require(!xr_replace_native_status()&&!xr_health_gauge(hp,cap),"inactive XR cannot suppress native HUD");bridge.input.active=true;
    bridge.valid_pose=false;require(!xr_replace_native_status(),"lost head tracking retains native HUD");bridge.valid_pose=true;
    xr_publish_health(6,5);require(!xr_health_gauge(hp,cap)&&!xr_replace_native_status(),"out-of-range native health keeps original HUD as fallback");
    xr_publish_health(2,0);require(!xr_replace_native_status(),"invalid native maximum never invents health capacity");
    xr_publish_health(-1,5);require(!xr_replace_native_status(),"negative health fails open");
    xr_publish_health(2,10);require(!xr_replace_native_status(),"unsupported capacity retains original HUD");
    xr_publish_health(3,5);xr_configure_ammo_gauges(false);require(!xr_replace_native_status(),"disabled floating ammo cannot hide native status");xr_configure_ammo_gauges(true);
    xr_configure_health_gauge(false);require(!xr_replace_native_status()&&!xr_health_gauge(hp,cap),"health preview can be disabled while preserving original display");xr_configure_health_gauge(true);
    xr_enable_independent_magazines(false);require(!xr_replace_native_status(),"shared fallback keeps native health and ammo");
    xr_enable_independent_magazines(true);xr_publish_magazines(6,6);require(!xr_replace_native_status(),"arcade re-entry waits for fresh health rather than old life data");
    printf("PASS %u production XR bridge checks (synthetic poses)\n",checks);return 0;
}
