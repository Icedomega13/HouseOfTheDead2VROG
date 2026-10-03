#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <dxgi1_2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "xr_bridge.h"
#include "xr_math.h"
#include "xr_pixels.h"
#include "reload_gesture.h"
#include "dual_wield.h"
#include <vector>
#include <cstring>
#include <cstddef>
static void update_dual_shots();

namespace {
struct EyeSwapchain {
    XrSwapchain handle=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
};
struct Bridge {
    bool enabled=false,attempted=false,running=false,frame_open=false,valid_pose=false,recentered=false;
    float units=10,gun_pitch=0;
    unsigned eye_size=800;
    bool focused=false,recenter_request=false,aim_valid=false;
    bool cursor_visible=true,cursor_toggle_held=false,cursor_toggle_armed=true;
    bool trigger_down=false,feedback_fire=false,feedback_reload=false,haptics=true,haptics_failed=false;
    bool haptic_pulse_pending=false;
    float haptic_scale=1;
    bool aim_down_reload=true;
    bool cinematic_guard=false;
    float reload_down_y=-0.819152f,reload_rearm_y=-0.573576f;
    hotd2_xr::DownReloadGesture reload_gesture;
    XrTime space_change=0;
    XrSpace head_space=XR_NULL_HANDLE,aim_space=XR_NULL_HANDLE;
    XrActionSet actions=XR_NULL_HANDLE;
    XrAction recenter=XR_NULL_HANDLE,aim=XR_NULL_HANDLE,fire=XR_NULL_HANDLE,reload=XR_NULL_HANDLE,start=XR_NULL_HANDLE,back=XR_NULL_HANDLE,menu=XR_NULL_HANDLE;
    XrAction vibration=XR_NULL_HANDLE;
    XrAction cursor_toggle=XR_NULL_HANDLE;
    XrPosef aim_pose={};
    bool dual_wield=false,left_aim_valid=false,left_trigger_down=false,left_fire=false;
    bool left_feedback_fire=false,left_feedback_reload=false,left_haptic_pending=false;
    XrSpace left_aim_space=XR_NULL_HANDLE;
    XrAction left_aim=XR_NULL_HANDLE,left_trigger=XR_NULL_HANDLE,left_vibration=XR_NULL_HANDLE;
    XrPosef left_aim_pose={};
    hotd2_xr::DownReloadGesture left_reload_gesture;
    hotd2_input::DualShotRouter shots;
    bool independent_magazines=false,magazine_button_held=false;
    unsigned magazine_reload_mask=0;
    int magazine_rounds[2]={6,6};
    bool ammo_gauges=true,magazine_counts_valid=false;
    bool health_gauge=false,health_valid=false;
    int health=0,health_maximum=0;
    bool magazine_reload_pulse[2]={};
    D3DMATRIX game_projection={};
    XrGameInput input;
    ProbeLog log=nullptr;
    XrInstance instance=XR_NULL_HANDLE;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrSession session=XR_NULL_HANDLE;
    XrSpace space=XR_NULL_HANDLE;
    XrFrameState frame={XR_TYPE_FRAME_STATE};
    XrView views[2]={{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
    XrPosef origin={};
    ID3D11Device* device=nullptr;
    ID3D11DeviceContext* context=nullptr;
    EyeSwapchain eyes[2];
    unsigned submitted=0;
    std::vector<uint8_t> rgba[2];
    hotd2_xr::EyeAtlasConverter pixel_converter;
    unsigned transfer_count=0;
    double transfer_total_ms=0,transfer_max_ms=0;
} bridge;
void stop_left_controller_feedback() {
    if(bridge.left_haptic_pending) {
        bridge.left_haptic_pending=false;
        if(bridge.session&&bridge.left_vibration) {
            XrHapticActionInfo info={XR_TYPE_HAPTIC_ACTION_INFO};info.action=bridge.left_vibration;
            auto result=xrStopHapticFeedback(bridge.session,&info);
            if(XR_FAILED(result)){bridge.haptics_failed=true;if(bridge.log)bridge.log("XR left haptics stop failed result=%d",result);}
        }
    }
}
void stop_controller_feedback(bool include_left=true) {
    if(include_left)stop_left_controller_feedback();
    if(!bridge.haptic_pulse_pending) return;
    bridge.haptic_pulse_pending=false;
    if(!bridge.session||!bridge.vibration) return;
    XrHapticActionInfo info={XR_TYPE_HAPTIC_ACTION_INFO};info.action=bridge.vibration;
    auto result=xrStopHapticFeedback(bridge.session,&info);
    // SESSION_NOT_FOCUSED is an expected positive status during focus changes.
    // Consume the latch once; do not spam the runtime on subsequent empty frames.
    if(XR_FAILED(result)) {
        bridge.haptics_failed=true;
        if(bridge.log) bridge.log("XR haptics stop failed result=%d; vibration disabled, input remains available",result);
    }
}
void clear_controls() {
    stop_controller_feedback();
    bridge.input={};bridge.aim_valid=false;bridge.trigger_down=false;
    bridge.feedback_fire=bridge.feedback_reload=false;bridge.reload_gesture.reset();
    bridge.cursor_toggle_held=false;bridge.cursor_toggle_armed=false;
    bridge.left_aim_valid=bridge.left_trigger_down=bridge.left_fire=false;
    bridge.left_feedback_fire=bridge.left_feedback_reload=false;bridge.left_reload_gesture.reset();bridge.shots.reset();
    bridge.magazine_reload_mask=0;bridge.magazine_button_held=false;
    bridge.magazine_reload_pulse[0]=bridge.magazine_reload_pulse[1]=false;
}
void update_cursor_toggle(bool active,bool down) {
    if(!active){bridge.cursor_toggle_held=false;bridge.cursor_toggle_armed=false;return;}
    if(!down){bridge.cursor_toggle_held=false;bridge.cursor_toggle_armed=true;return;}
    if(bridge.cursor_toggle_armed&&!bridge.cursor_toggle_held) {
        bridge.cursor_visible=!bridge.cursor_visible;
        if(bridge.log) bridge.log("VR_AIM_CURSOR visible=%d toggle=left_Y",bridge.cursor_visible);
    }
    bridge.cursor_toggle_held=true;
}
static DirectX::XMMATRIX calibrated_aim(float units,unsigned hand=0) {
    // Positive pitch lowers +Z-forward about the controller's local X axis.
    // Apply the same correction to the mesh, shooting ray and reload gesture.
    return DirectX::XMMatrixRotationX(bridge.gun_pitch)*hotd2_xr::pose_lh(hand?bridge.left_aim_pose:bridge.aim_pose,units);
}
void update_reload_gesture() {
    bool tracked=bridge.aim_down_reload&&!bridge.cinematic_guard&&bridge.focused&&bridge.input.active&&bridge.valid_pose&&bridge.aim_valid&&bridge.recentered;
    float y=tracked?DirectX::XMVectorGetY(calibrated_aim(1).r[2]):0;
    bool pulse=bridge.reload_gesture.update(tracked,y,bridge.frame.predictedDisplayTime,bridge.reload_down_y,bridge.reload_rearm_y);
    bool left_tracked=bridge.dual_wield&&bridge.aim_down_reload&&!bridge.cinematic_guard&&bridge.focused&&bridge.input.active&&bridge.valid_pose&&bridge.left_aim_valid&&bridge.recentered;
    float left_y=left_tracked?DirectX::XMVectorGetY(calibrated_aim(1,1).r[2]):0;
    bool left_pulse=bridge.left_reload_gesture.update(left_tracked,left_y,bridge.frame.predictedDisplayTime,bridge.reload_down_y,bridge.reload_rearm_y);
    if(bridge.independent_magazines) {
        if(bridge.input.reload&&!bridge.magazine_button_held)bridge.magazine_reload_mask|=3;
        bridge.magazine_button_held=bridge.input.reload;
        bool triggered[]={bridge.reload_gesture.triggered,bridge.left_reload_gesture.triggered};
        for(unsigned h=0;h<2;++h)if(triggered[h]) {
            bridge.magazine_reload_mask|=1u<<h;bridge.shots.cancel_hand(h);
            if(bridge.log)bridge.log("INPUT aim_down_reload hand=%s independent_magazine=1",h?"left":"right");
        }
        bridge.magazine_reload_pulse[0]=pulse;bridge.magazine_reload_pulse[1]=left_pulse;
    } else {
        if(pulse||left_pulse) {bridge.input.reload=true;bridge.input.fire=false;bridge.left_fire=false;}
        if(left_pulse&&bridge.log)bridge.log("INPUT aim_down_reload hand=left shared_magazine=1");
    }
    if(bridge.reload_gesture.triggered&&bridge.log)
        bridge.log("INPUT aim_down_reload pitch_down_degrees=%.3f",-std::asin(std::clamp(y,-1.0f,1.0f))*180/DirectX::XM_PI);
}
bool check(XrResult result,const char* call) {
    if(XR_SUCCEEDED(result)) return true;
    if(bridge.log) bridge.log("XR failure call=%s result=%d",call,result);
    return false;
}
bool frame_check(XrResult result,const char* call) {
    if(check(result,call)) return true;
    // A failed lifecycle call leaves the runtime's frame sequence uncertain.
    // Stop submitting until restart, matching the swapchain error policy.
    bridge.enabled=false;bridge.valid_pose=false;clear_controls();
    if(bridge.log) bridge.log("XR frame lifecycle stopped call=%s; restart game to resume VR",call);
    return false;
}
void cleanup_failed_init() {
    for(auto& eye:bridge.eyes) if(eye.handle) {xrDestroySwapchain(eye.handle);eye.handle=XR_NULL_HANDLE;eye.images.clear();}
    if(bridge.space) {xrDestroySpace(bridge.space);bridge.space=XR_NULL_HANDLE;}
    if(bridge.head_space) {xrDestroySpace(bridge.head_space);bridge.head_space=XR_NULL_HANDLE;}
    if(bridge.aim_space) {xrDestroySpace(bridge.aim_space);bridge.aim_space=XR_NULL_HANDLE;}
    if(bridge.left_aim_space) {xrDestroySpace(bridge.left_aim_space);bridge.left_aim_space=XR_NULL_HANDLE;}
    if(bridge.session) {xrDestroySession(bridge.session);bridge.session=XR_NULL_HANDLE;}
    if(bridge.actions) {xrDestroyActionSet(bridge.actions);bridge.actions=XR_NULL_HANDLE;}
    if(bridge.instance) {xrDestroyInstance(bridge.instance);bridge.instance=XR_NULL_HANDLE;}
    if(bridge.context) {bridge.context->Release();bridge.context=nullptr;}
    if(bridge.device) {bridge.device->Release();bridge.device=nullptr;}
}
bool initialize_actions() {
    XrActionSetCreateInfo set={XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(set.actionSetName,"game");strcpy_s(set.localizedActionSetName,"HOTD2 controls");
    if(!check(xrCreateActionSet(bridge.instance,&set,&bridge.actions),"xrCreateActionSet")) return false;
    struct Binding {const char* name;const char* label;XrActionType type;const char* path;XrAction* action;};
    Binding bindings[]={
        {"recenter","Recenter",XR_ACTION_TYPE_BOOLEAN_INPUT,"/user/hand/left/input/thumbstick/click",&bridge.recenter},
        {"aim","Aim",XR_ACTION_TYPE_POSE_INPUT,"/user/hand/right/input/aim/pose",&bridge.aim},
        {"fire","Shoot",XR_ACTION_TYPE_FLOAT_INPUT,"/user/hand/right/input/trigger/value",&bridge.fire},
        {"reload","Reload",XR_ACTION_TYPE_BOOLEAN_INPUT,"/user/hand/right/input/b/click",&bridge.reload},
        {"start","Start or confirm",XR_ACTION_TYPE_BOOLEAN_INPUT,"/user/hand/right/input/a/click",&bridge.start},
        {"back","Back",XR_ACTION_TYPE_BOOLEAN_INPUT,"/user/hand/left/input/x/click",&bridge.back},
        {"cursor_toggle","Toggle aiming cursor",XR_ACTION_TYPE_BOOLEAN_INPUT,"/user/hand/left/input/y/click",&bridge.cursor_toggle},
        {"menu_move","Menu navigation",XR_ACTION_TYPE_VECTOR2F_INPUT,"/user/hand/left/input/thumbstick",&bridge.menu},
        {"feedback","Controller feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT,"/user/hand/right/output/haptic",&bridge.vibration},
        {"left_aim","Left gun aim",XR_ACTION_TYPE_POSE_INPUT,"/user/hand/left/input/aim/pose",&bridge.left_aim},
        {"left_fire","Left gun fire",XR_ACTION_TYPE_FLOAT_INPUT,"/user/hand/left/input/trigger/value",&bridge.left_trigger},
        {"left_feedback","Left gun feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT,"/user/hand/left/output/haptic",&bridge.left_vibration}};
    std::vector<XrActionSuggestedBinding> suggestions;
    for(const auto& b:bindings) {
        if(!bridge.dual_wield&&(b.action==&bridge.left_aim||b.action==&bridge.left_trigger||b.action==&bridge.left_vibration))continue;
        XrActionCreateInfo info={XR_TYPE_ACTION_CREATE_INFO};info.actionType=b.type;
        strcpy_s(info.actionName,b.name);strcpy_s(info.localizedActionName,b.label);
        if(!check(xrCreateAction(bridge.actions,&info,b.action),"xrCreateAction")) return false;
        XrPath path=XR_NULL_PATH;
        if(!check(xrStringToPath(bridge.instance,b.path,&path),"controller binding path")) return false;
        suggestions.push_back({*b.action,path});
    }
    XrInteractionProfileSuggestedBinding suggested={XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    if(!check(xrStringToPath(bridge.instance,"/interaction_profiles/oculus/touch_controller",&suggested.interactionProfile),"Touch profile")) return false;
    suggested.countSuggestedBindings=static_cast<uint32_t>(suggestions.size());suggested.suggestedBindings=suggestions.data();
    if(!check(xrSuggestInteractionProfileBindings(bridge.instance,&suggested),"Touch bindings")) return false;
    XrSessionActionSetsAttachInfo attach={XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&bridge.actions;
    if(!check(xrAttachSessionActionSets(bridge.session,&attach),"xrAttachSessionActionSets")) return false;
    XrActionSpaceCreateInfo space={XR_TYPE_ACTION_SPACE_CREATE_INFO};space.action=bridge.aim;space.poseInActionSpace.orientation.w=1;
    if(!check(xrCreateActionSpace(bridge.session,&space,&bridge.aim_space),"xrCreateActionSpace"))return false;
    if(bridge.dual_wield){space.action=bridge.left_aim;return check(xrCreateActionSpace(bridge.session,&space,&bridge.left_aim_space),"left xrCreateActionSpace");}return true;
}
bool button(XrAction action,bool edge=false) {
    XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;
    XrActionStateBoolean state={XR_TYPE_ACTION_STATE_BOOLEAN};
    return XR_SUCCEEDED(xrGetActionStateBoolean(bridge.session,&get,&state))&&state.isActive&&state.currentState&&(!edge||state.changedSinceLastSync);
}
void sync_input() {
    bridge.input={};bridge.aim_valid=false;bridge.left_aim_valid=bridge.left_fire=false;
    if(!bridge.focused) {clear_controls();return;}
    XrActiveActionSet active={bridge.actions,XR_NULL_PATH};
    XrActionsSyncInfo sync={XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
    if(xrSyncActions(bridge.session,&sync)!=XR_SUCCESS) {clear_controls();return;}
    bridge.input.active=true;
    XrActionStateGetInfo cursor_get={XR_TYPE_ACTION_STATE_GET_INFO};cursor_get.action=bridge.cursor_toggle;
    XrActionStateBoolean cursor_state={XR_TYPE_ACTION_STATE_BOOLEAN};
    auto cursor_result=xrGetActionStateBoolean(bridge.session,&cursor_get,&cursor_state);
    update_cursor_toggle(cursor_result==XR_SUCCESS&&cursor_state.isActive,cursor_state.currentState!=XR_FALSE);
    bridge.recenter_request|=button(bridge.recenter,true);
    bridge.input.reload=button(bridge.reload);bridge.input.start=button(bridge.start);bridge.input.back=button(bridge.back);
    XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=bridge.fire;
    get.action=bridge.menu;XrActionStateVector2f stick={XR_TYPE_ACTION_STATE_VECTOR2F};
    if(XR_SUCCEEDED(xrGetActionStateVector2f(bridge.session,&get,&stick))&&stick.isActive) {bridge.input.menu_x=stick.currentState.x;bridge.input.menu_y=stick.currentState.y;}
    get.action=bridge.fire;
    XrActionStateFloat trigger={XR_TYPE_ACTION_STATE_FLOAT};
    if(XR_SUCCEEDED(xrGetActionStateFloat(bridge.session,&get,&trigger))&&trigger.isActive) {
        bridge.trigger_down=hotd2_xr::trigger_pressed(trigger.currentState,bridge.trigger_down);
        bridge.input.fire=bridge.trigger_down;
    } else bridge.trigger_down=false;
    get.action=bridge.aim;XrActionStatePose pose={XR_TYPE_ACTION_STATE_POSE};
    if(XR_SUCCEEDED(xrGetActionStatePose(bridge.session,&get,&pose))&&pose.isActive) {
        XrSpaceLocation location={XR_TYPE_SPACE_LOCATION};
        const auto required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        if(XR_SUCCEEDED(xrLocateSpace(bridge.aim_space,bridge.space,bridge.frame.predictedDisplayTime,&location))&&(location.locationFlags&required)==required) {
            bridge.aim_pose=location.pose;bridge.aim_valid=true;
        }
    }
    if(!bridge.aim_valid) {bridge.input.fire=false;bridge.trigger_down=false;}
    if(bridge.dual_wield) {
        get.action=bridge.left_trigger;trigger={XR_TYPE_ACTION_STATE_FLOAT};
        if(xrGetActionStateFloat(bridge.session,&get,&trigger)==XR_SUCCESS&&trigger.isActive){bridge.left_trigger_down=hotd2_xr::trigger_pressed(trigger.currentState,bridge.left_trigger_down);bridge.left_fire=bridge.left_trigger_down;}else bridge.left_trigger_down=false;
        get.action=bridge.left_aim;pose={XR_TYPE_ACTION_STATE_POSE};
        if(xrGetActionStatePose(bridge.session,&get,&pose)==XR_SUCCESS&&pose.isActive) {
            XrSpaceLocation location={XR_TYPE_SPACE_LOCATION};const auto required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
            if(xrLocateSpace(bridge.left_aim_space,bridge.space,bridge.frame.predictedDisplayTime,&location)==XR_SUCCESS&&(location.locationFlags&required)==required){bridge.left_aim_pose=location.pose;bridge.left_aim_valid=true;}
        }
        if(!bridge.left_aim_valid){bridge.left_fire=false;bridge.left_trigger_down=false;}
    }
}
void right_controller_feedback() {
    bool active=bridge.focused&&bridge.input.active&&bridge.valid_pose&&bridge.aim_valid&&bridge.recentered;
    bool fire=active&&bridge.input.fire,reload=active&&(bridge.input.reload||bridge.magazine_reload_pulse[0]);
    bool fire_edge=fire&&!bridge.feedback_fire,reload_edge=reload&&!bridge.feedback_reload;
    bridge.feedback_fire=fire;bridge.feedback_reload=reload;
    if(!active||!bridge.haptics||bridge.haptic_scale==0||bridge.haptics_failed) {stop_controller_feedback(false);return;}
    if(!bridge.session||!bridge.vibration||(!fire_edge&&!reload_edge)) return;
    XrHapticActionInfo info={XR_TYPE_HAPTIC_ACTION_INFO};info.action=bridge.vibration;
    XrHapticVibration pulse={XR_TYPE_HAPTIC_VIBRATION};
    pulse.frequency=XR_FREQUENCY_UNSPECIFIED;
    pulse.duration=reload_edge?80000000:55000000; // nanoseconds; input acknowledgement only.
    pulse.amplitude=std::min(1.0f,(reload_edge?0.4f:0.85f)*bridge.haptic_scale);
    auto result=xrApplyHapticFeedback(bridge.session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&pulse));
    if(result==XR_SUCCESS) bridge.haptic_pulse_pending=true;
    if(XR_FAILED(result)) {
        bridge.haptics_failed=true;
        if(bridge.log) bridge.log("XR haptics disabled result=%d; input remains available",result);
    }
}
void controller_feedback() {
    right_controller_feedback();
    bool active=bridge.dual_wield&&bridge.focused&&bridge.input.active&&bridge.valid_pose&&bridge.left_aim_valid&&bridge.recentered;
    bool fire=active&&bridge.left_fire,reload=active&&(bridge.input.reload||bridge.magazine_reload_pulse[1]);
    const bool reload_edge=reload&&!bridge.left_feedback_reload;
    bool edge=(fire&&!bridge.left_feedback_fire)||(reload&&!bridge.left_feedback_reload);
    bridge.left_feedback_fire=fire;bridge.left_feedback_reload=reload;
    if(!active||!bridge.haptics||bridge.haptic_scale==0||bridge.haptics_failed){stop_left_controller_feedback();return;}
    if(!edge||!bridge.session||!bridge.left_vibration)return;
    XrHapticActionInfo info={XR_TYPE_HAPTIC_ACTION_INFO};info.action=bridge.left_vibration;
    XrHapticVibration pulse={XR_TYPE_HAPTIC_VIBRATION};pulse.duration=reload_edge?80000000:55000000;pulse.amplitude=std::min(1.0f,(reload_edge?.4f:.85f)*bridge.haptic_scale);pulse.frequency=XR_FREQUENCY_UNSPECIFIED;
    auto result=xrApplyHapticFeedback(bridge.session,&info,reinterpret_cast<const XrHapticBaseHeader*>(&pulse));
    if(result==XR_SUCCESS)bridge.left_haptic_pending=true;
    else if(XR_FAILED(result)){bridge.haptics_failed=true;if(bridge.log)bridge.log("XR left haptics disabled result=%d",result);}
}
bool initialize() {
    const char* extensions[]={XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo ci={XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(ci.applicationInfo.applicationName,"HOTD2 original PCVR experiment");
    ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    ci.enabledExtensionCount=1; ci.enabledExtensionNames=extensions;
    if(!check(xrCreateInstance(&ci,&bridge.instance),"xrCreateInstance")) return false;
    XrSystemGetInfo si={XR_TYPE_SYSTEM_GET_INFO}; si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if(!check(xrGetSystem(bridge.instance,&si,&bridge.system),"xrGetSystem (headset must be connected)")) return false;
    PFN_xrGetD3D11GraphicsRequirementsKHR requirements_fn=nullptr;
    if(!check(xrGetInstanceProcAddr(bridge.instance,"xrGetD3D11GraphicsRequirementsKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&requirements_fn)),"graphics requirements function")) return false;
    XrGraphicsRequirementsD3D11KHR requirements={XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    if(!check(requirements_fn(bridge.instance,bridge.system,&requirements),"D3D11 graphics requirements")) return false;
    IDXGIFactory1* factory=nullptr;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))) return false;
    IDXGIAdapter1* adapter=nullptr;
    for(UINT i=0;;++i) {
        IDXGIAdapter1* candidate=nullptr;
        if(factory->EnumAdapters1(i,&candidate)==DXGI_ERROR_NOT_FOUND) break;
        if(!candidate) break;
        DXGI_ADAPTER_DESC1 desc={}; candidate->GetDesc1(&desc);
        if(desc.AdapterLuid.HighPart==requirements.adapterLuid.HighPart&&desc.AdapterLuid.LowPart==requirements.adapterLuid.LowPart) {
            adapter=candidate;break;
        }
        candidate->Release();
    }
    factory->Release();
    if(!adapter) {bridge.log("XR required GPU adapter not found");return false;}
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL actual={};
    HRESULT hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,levels,2,D3D11_SDK_VERSION,
        &bridge.device,&actual,&bridge.context);
    adapter->Release();
    if(FAILED(hr)||actual<requirements.minFeatureLevel) {bridge.log("XR D3D11 device failure hr=%08lx",hr);return false;}
    XrGraphicsBindingD3D11KHR binding={XR_TYPE_GRAPHICS_BINDING_D3D11_KHR}; binding.device=bridge.device;
    XrSessionCreateInfo session_info={XR_TYPE_SESSION_CREATE_INFO};
    session_info.next=&binding; session_info.systemId=bridge.system;
    if(!check(xrCreateSession(bridge.instance,&session_info,&bridge.session),"xrCreateSession")) return false;
    XrReferenceSpaceCreateInfo reference={XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL; reference.poseInReferenceSpace.orientation.w=1;
    if(!check(xrCreateReferenceSpace(bridge.session,&reference,&bridge.space),"xrCreateReferenceSpace")) return false;
    reference.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
    if(!check(xrCreateReferenceSpace(bridge.session,&reference,&bridge.head_space),"head reference space")||!initialize_actions()) return false;
    uint32_t count=0;
    if(!check(xrEnumerateSwapchainFormats(bridge.session,0,&count,nullptr),"swapchain formats")) return false;
    std::vector<int64_t> formats(count);
    if(!check(xrEnumerateSwapchainFormats(bridge.session,count,&count,formats.data()),"swapchain formats")) return false;
    // The source is already gamma-encoded. An sRGB swapchain tells the compositor how to interpret it.
    int64_t format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    bool supported=false; for(auto f:formats) supported|=f==format;
    if(!supported) {bridge.log("XR RGBA8 sRGB swapchain unavailable");return false;}
    for(auto& eye:bridge.eyes) {
        XrSwapchainCreateInfo sci={XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sci.format=format;sci.sampleCount=1;sci.width=bridge.eye_size;sci.height=bridge.eye_size;sci.faceCount=1;sci.arraySize=1;sci.mipCount=1;
        if(!check(xrCreateSwapchain(bridge.session,&sci,&eye.handle),"xrCreateSwapchain")) return false;
        if(!check(xrEnumerateSwapchainImages(eye.handle,0,&count,nullptr),"swapchain image count")) return false;
        eye.images.resize(count);
        for(auto& image:eye.images) image.type=XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        if(!check(xrEnumerateSwapchainImages(eye.handle,count,&count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),"swapchain images")) return false;
    }
    bridge.log("XR session created eye_resolution=%ux%u units_per_metre=%.4g gun_pitch_down_degrees=%.4g controls=Touch",bridge.eye_size,bridge.eye_size,bridge.units,bridge.gun_pitch*180/DirectX::XM_PI);
    return true;
}
void poll_events() {
    XrEventDataBuffer event={XR_TYPE_EVENT_DATA_BUFFER};
    while(xrPollEvent(bridge.instance,&event)==XR_SUCCESS) {
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
            bridge.log("XR session state=%d",state);
            bridge.focused=state==XR_SESSION_STATE_FOCUSED;
            if(!bridge.focused) clear_controls();
            if(state==XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin={XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                bridge.running=check(xrBeginSession(bridge.session,&begin),"xrBeginSession");
            } else if(state==XR_SESSION_STATE_STOPPING) {
                if(bridge.frame_open) {
                    XrFrameEndInfo end={XR_TYPE_FRAME_END_INFO};end.displayTime=bridge.frame.predictedDisplayTime;
                    end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                    frame_check(xrEndFrame(bridge.session,&end),"xrEndFrame (session stopping)");bridge.frame_open=false;
                }
                xrEndSession(bridge.session);bridge.running=false;
            } else if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING) {
                bridge.enabled=false;bridge.running=false;bridge.valid_pose=false;
            }
        } else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
            auto change=reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&event);
            if(change->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL) bridge.space_change=change->changeTime;
        }
        else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {bridge.enabled=false;bridge.running=false;}
        event={XR_TYPE_EVENT_DATA_BUFFER};
    }
}
bool acquire_eye_image(XrSwapchain swapchain,uint32_t& index) {
    XrSwapchainImageAcquireInfo acquire={XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if(check(xrAcquireSwapchainImage(swapchain,&acquire,&index),"xrAcquireSwapchainImage")) return true;
    bridge.enabled=false;return false;
}
bool wait_eye_image(XrSwapchain swapchain) {
    XrSwapchainImageWaitInfo wait={XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;
    // XR_TIMEOUT_EXPIRED is positive, but it does not grant image ownership.
    // Retry the same acquired image; never upload/release it while still waiting.
    for(unsigned attempt=0;attempt<3;++attempt) {
        auto result=xrWaitSwapchainImage(swapchain,&wait);
        if(result==XR_TIMEOUT_EXPIRED) continue;
        if(check(result,"xrWaitSwapchainImage")) return true;
        bridge.enabled=false;return false;
    }
    if(bridge.log) bridge.log("XR swapchain wait timed out three times; VR stopped without writing an unready image");
    bridge.enabled=false;return false;
}
bool release_eye_image(XrSwapchain swapchain) {
    XrSwapchainImageReleaseInfo release={XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    if(check(xrReleaseSwapchainImage(swapchain,&release),"xrReleaseSwapchainImage")) return true;
    // Do not acquire again after a failed release: ownership is uncertain.
    bridge.enabled=false;return false;
}
bool upload_eyes(IDirect3DDevice7* device,IDirectDrawSurface7* atlas) {
    LARGE_INTEGER started={},frequency={};QueryPerformanceCounter(&started);QueryPerformanceFrequency(&frequency);
    IDirectDrawSurface7* surface=nullptr;
    if(atlas) {surface=atlas;surface->AddRef();}
    else if(FAILED(device->GetRenderTarget(&surface))||!surface) return false;
    DDSURFACEDESC2 d={};d.dwSize=sizeof(d);
    HRESULT locked=surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT,nullptr);
    if(FAILED(locked)) {
        static unsigned failures=0;if(failures++<3&&bridge.log) bridge.log("XR atlas lock failure hr=%08lx",locked);
        surface->Release();return false;
    }
    DWORD bits=d.ddpfPixelFormat.dwRGBBitCount;
    unsigned size=bridge.eye_size;
    bool valid=atlas&&(bits==16||bits==32)&&d.dwWidth==size*2&&d.dwHeight==size;
    if(valid) valid=bridge.pixel_converter.convert(d.lpSurface,d.lPitch,size,bits,d.ddpfPixelFormat.dwRBitMask,
        d.ddpfPixelFormat.dwGBitMask,d.ddpfPixelFormat.dwBBitMask,bridge.rgba);
    surface->Unlock(nullptr);surface->Release();
    if(!valid) {bridge.log("XR frame format unsupported width=%lu height=%lu bits=%lu",d.dwWidth,d.dwHeight,bits);return false;}
    for(unsigned i=0;i<2;++i) {
        auto& eye=bridge.eyes[i]; uint32_t index=0;
        if(!acquire_eye_image(eye.handle,index)||!wait_eye_image(eye.handle)) return false;
        bridge.context->UpdateSubresource(eye.images[index].texture,0,nullptr,bridge.rgba[i].data(),size*4,0);
        bridge.context->Flush();
        if(!release_eye_image(eye.handle)) return false;
    }
    LARGE_INTEGER finished={};QueryPerformanceCounter(&finished);
    if(frequency.QuadPart>0) {
        double ms=1000.0*static_cast<double>(finished.QuadPart-started.QuadPart)/frequency.QuadPart;
        bridge.transfer_total_ms+=ms;bridge.transfer_max_ms=std::max(ms,bridge.transfer_max_ms);
        if(++bridge.transfer_count==120) {
            if(bridge.log) bridge.log("XR eye_transfer frames=120 mean_ms=%.3f max_ms=%.3f source_bits=%lu",bridge.transfer_total_ms/120,bridge.transfer_max_ms,bits);
            bridge.transfer_count=0;bridge.transfer_total_ms=bridge.transfer_max_ms=0;
        }
    }
    return true;
}
}

void configure_xr(bool enabled,float units_per_metre,unsigned eye_size,ProbeLog log,float gun_pitch_degrees,bool haptics,bool aim_down_reload,float down_reload_degrees,float haptic_scale,bool aiming_cursor,bool dual_wield) {
    bridge.enabled=enabled;bridge.units=units_per_metre;bridge.eye_size=eye_size;bridge.log=log;
    bridge.gun_pitch=std::clamp(gun_pitch_degrees,-45.0f,45.0f)*DirectX::XM_PI/180;
    bridge.cursor_visible=aiming_cursor;bridge.cursor_toggle_held=false;bridge.cursor_toggle_armed=true;
    bridge.dual_wield=dual_wield;bridge.shots=hotd2_input::DualShotRouter{};bridge.left_reload_gesture.reset();
    bridge.independent_magazines=false;bridge.magazine_reload_mask=0;bridge.magazine_button_held=false;
    bridge.magazine_rounds[0]=bridge.magazine_rounds[1]=6;
    bridge.magazine_counts_valid=false;
    bridge.health_valid=false;
    bridge.magazine_reload_pulse[0]=bridge.magazine_reload_pulse[1]=false;
    if(log)log("VR_DUAL_WIELD enabled=%d shared_player=1 shared_native_ammo=1 serialized_trigger_edges=1",dual_wield);
    if(log) log("VR_AIM_CURSOR visible=%d binding=left_Y session_toggle=1",aiming_cursor);
    bridge.haptics=haptics;
    bridge.haptic_scale=std::isfinite(haptic_scale)?std::clamp(haptic_scale,0.0f,2.0f):1.0f;
    if(!enabled||!haptics||bridge.haptic_scale==0) stop_controller_feedback();
    if(log) log("XR haptics configured enabled=%d strength_percent=%.0f fire_amplitude=%.2f fire_ms=55 reload_amplitude=%.2f reload_ms=80 input_acknowledgement_only=1",haptics,bridge.haptic_scale*100,std::min(1.0f,.85f*bridge.haptic_scale),std::min(1.0f,.4f*bridge.haptic_scale));
    bridge.aim_down_reload=aim_down_reload;
    float degrees=std::clamp(down_reload_degrees,35.0f,85.0f);
    bridge.reload_down_y=-std::sin(degrees*DirectX::XM_PI/180);
    bridge.reload_rearm_y=-std::sin((degrees-20)*DirectX::XM_PI/180);
    bridge.reload_gesture.reset();
}
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
static float replay_yaw=0;
static bool replay_passive=false,replay_combat=false;
static bool replay_cursor_toggle=false;
void configure_xr_replay(float yaw_degrees,bool passive,bool combat,bool cursor_toggle) {
    replay_yaw=std::clamp(yaw_degrees,-180.0f,180.0f)*DirectX::XM_PI/180;
    replay_passive=passive;replay_combat=combat;
    replay_cursor_toggle=cursor_toggle;
}
#endif
void begin_xr_frame() {
    if(!bridge.enabled) {clear_controls();return;}
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    // Only compiled into the separate replay-test DLL, never the desktop shortcut build.
    static unsigned frame=0;
    if(bridge.frame_open) return;
    ++frame;bridge.frame_open=true;bridge.valid_pose=true;bridge.recentered=true;
    bridge.frame.predictedDisplayTime=static_cast<XrTime>(frame)*16666667;
    bridge.origin={};bridge.origin.orientation.w=1;
    for(unsigned i=0;i<2;++i) {
        bridge.views[i].pose={};bridge.views[i].pose.orientation={0,std::sin(replay_yaw/2),0,std::cos(replay_yaw/2)};
        float eye_x=i==0?-.032f:.032f;
        bridge.views[i].pose.position={eye_x*std::cos(replay_yaw),0,-eye_x*std::sin(replay_yaw)};
        bridge.views[i].fov={-.75f,.75f,.75f,-.75f};
    }
    bridge.input={};bridge.input.active=true;bridge.aim_valid=true;
    if(replay_cursor_toggle) update_cursor_toggle(true,(frame>=1440&&frame<1450)||(frame>=2400&&frame<2410)||(frame>=2700&&frame<2710));
    bridge.aim_pose={};bridge.aim_pose.orientation.w=1;bridge.aim_pose.position={.1f,-.15f,-.3f};
    bridge.input.start=!replay_passive&&((frame>=900&&frame<905)||(frame>=1020&&frame<1025)||(frame>=1140&&frame<1145));
    bridge.input.fire=!replay_passive&&frame>=720&&frame%30<3;
    bridge.input.reload=!replay_passive&&frame>=720&&frame%240>=120&&frame%240<125;
    bridge.focused=true;
    if(!replay_passive&&frame>=1320&&frame%240>=60&&frame%240<85) {
        float half_angle=70*DirectX::XM_PI/360;
        bridge.aim_pose.orientation={-std::sin(half_angle),0,0,std::cos(half_angle)};
    }
    if(replay_combat&&!replay_passive&&frame>=1320){
        using namespace DirectX;
        // Test-only level/torso-height sweep rather than the ordinary lowered ray.
        auto raw=XMMatrixRotationX(-bridge.gun_pitch)*XMMatrixRotationRollPitchYaw(-.08f+.1f*std::sin(frame*.015f),.23f*std::sin(frame*.009f),0);
        auto q=XMQuaternionRotationMatrix(raw);bridge.aim_pose.orientation={-XMVectorGetX(q),-XMVectorGetY(q),XMVectorGetZ(q),XMVectorGetW(q)};
        bridge.aim_pose.position={0,-.03f,-.3f};bridge.input.fire=frame%12<3;
    }
    if(bridge.dual_wield){
        bridge.left_aim_valid=true;bridge.left_aim_pose=bridge.aim_pose;bridge.left_aim_pose.position.x=-.18f;bridge.left_fire=!replay_passive&&frame>=1320&&frame%24<3;
        if(bridge.independent_magazines&&!replay_passive&&frame>=1320){
            float half_angle=70*DirectX::XM_PI/360;
            if(frame%240>=60&&frame%240<85)bridge.aim_pose.orientation={-std::sin(half_angle),0,0,std::cos(half_angle)};
            if(frame%240>=100&&frame%240<125)bridge.left_aim_pose.orientation={-std::sin(half_angle),0,0,std::cos(half_angle)};
        }
    }
    bridge.trigger_down=bridge.input.fire;bridge.left_trigger_down=bridge.left_fire;
    update_reload_gesture();update_dual_shots();
    if(frame<=3||frame%120==0) bridge.log("REPLAY_TEST frame=%u simulated_controller=1",frame);
    return;
#else
    if(!bridge.attempted) {
        bridge.attempted=true;
        if(!initialize()) {cleanup_failed_init();bridge.enabled=false;clear_controls();return;}
    }
    poll_events();
    if(!bridge.running) {clear_controls();return;}
    if(bridge.frame_open) return;
    XrFrameWaitInfo wait={XR_TYPE_FRAME_WAIT_INFO};bridge.frame={XR_TYPE_FRAME_STATE};
    if(!frame_check(xrWaitFrame(bridge.session,&wait,&bridge.frame),"xrWaitFrame")) return;
    XrFrameBeginInfo begin={XR_TYPE_FRAME_BEGIN_INFO};
    if(!frame_check(xrBeginFrame(bridge.session,&begin),"xrBeginFrame")) return;
    bridge.frame_open=true;bridge.valid_pose=false;
    sync_input();
    if(!bridge.frame.shouldRender) {clear_controls();return;}
    XrViewLocateInfo locate={XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime=bridge.frame.predictedDisplayTime;locate.space=bridge.space;
    XrViewState state={XR_TYPE_VIEW_STATE};uint32_t count=0;
    if(!check(xrLocateViews(bridge.session,&locate,&state,2,&count,bridge.views),"xrLocateViews")||count!=2) {clear_controls();return;}
    auto required=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT|XR_VIEW_STATE_POSITION_TRACKED_BIT;
    bridge.valid_pose=(state.viewStateFlags&required)==required;
    if(bridge.space_change&&bridge.frame.predictedDisplayTime>=bridge.space_change) {bridge.recentered=false;bridge.space_change=0;}
    if(bridge.valid_pose&&(!bridge.recentered||bridge.recenter_request)) {
        XrSpaceLocation head={XR_TYPE_SPACE_LOCATION};
        auto flags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        if(XR_SUCCEEDED(xrLocateSpace(bridge.head_space,bridge.space,bridge.frame.predictedDisplayTime,&head))&&(head.locationFlags&flags)==flags) {
            bridge.origin=hotd2_xr::level_origin(head.pose);
            bridge.recentered=true;bridge.recenter_request=false;
            const auto& l=bridge.views[0].pose.position;const auto& r=bridge.views[1].pose.position;
            float ipd=std::sqrt((l.x-r.x)*(l.x-r.x)+(l.y-r.y)*(l.y-r.y)+(l.z-r.z)*(l.z-r.z));
            bridge.log("XR recentered level_origin=(%.4g,%.4g,%.4g) ipd_metres=%.5g units=%.4g",head.pose.position.x,head.pose.position.y,head.pose.position.z,ipd,bridge.units);
        } else bridge.valid_pose=false;
    }
    if(!bridge.valid_pose) clear_controls();
    update_reload_gesture();update_dual_shots();controller_feedback();
#endif
}
bool xr_eye_matrices(unsigned eye,const D3DMATRIX& game_view,const D3DMATRIX& game_projection,D3DMATRIX& view,D3DMATRIX& projection,bool flat_pass) {
    if(!bridge.frame_open||!bridge.valid_pose||!bridge.recentered||eye>1) return false;
    using namespace DirectX;
    XMFLOAT4X4 gv;memcpy(&gv,&game_view,sizeof(gv));
    auto origin=hotd2_xr::pose_lh(bridge.origin,bridge.units);
    auto eye_pose=hotd2_xr::pose_lh(bridge.views[eye].pose,bridge.units);
    float near_z=game_projection._33!=0?-game_projection._43/game_projection._33:0.8f;
    float far_z=game_projection._33>1?-game_projection._43/(game_projection._33-1):10000.0f;
    if(!(near_z>0&&far_z>near_z)) {near_z=0.8f;far_z=10000.0f;}
    // HOTD2 draws camera-aligned XYZ quads for flat cinematic graphics at Z=1.
    // Give those passes the same physical distance as the HUD instead of 1 game unit.
    auto camera_scale=flat_pass?XMMatrixScaling(2*bridge.units,2*bridge.units,2*bridge.units):XMMatrixIdentity();
    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result,XMLoadFloat4x4(&gv)*camera_scale*origin*XMMatrixInverse(nullptr,eye_pose));
    memcpy(&view,&result,sizeof(view));
    XMStoreFloat4x4(&result,hotd2_xr::projection_lh(bridge.views[eye].fov,near_z,far_z));
    memcpy(&projection,&result,sizeof(projection));return true;
}
void submit_xr_frame(IDirect3DDevice7* device,IDirectDrawSurface7* atlas) {
    if(!bridge.frame_open) return;
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
    (void)device;(void)atlas;bridge.frame_open=false;bridge.valid_pose=false;return;
#else
    XrCompositionLayerProjectionView views[2]={{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    bool render=bridge.frame.shouldRender&&bridge.valid_pose&&device&&upload_eyes(device,atlas);
    for(unsigned i=0;i<2;++i) {
        views[i].pose=bridge.views[i].pose;views[i].fov=bridge.views[i].fov;
        views[i].subImage.swapchain=bridge.eyes[i].handle;views[i].subImage.imageRect.extent={static_cast<int32_t>(bridge.eye_size),static_cast<int32_t>(bridge.eye_size)};
    }
    XrCompositionLayerProjection layer={XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=bridge.space;layer.viewCount=2;layer.views=views;
    const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
    XrFrameEndInfo end={XR_TYPE_FRAME_END_INFO};end.displayTime=bridge.frame.predictedDisplayTime;
    end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=render?1:0;end.layers=render?layers:nullptr;
    bool ok=frame_check(xrEndFrame(bridge.session,&end),"xrEndFrame");bridge.frame_open=false;bridge.valid_pose=false;
    if(!ok||!render) clear_controls();
    if(ok&&render&&(++bridge.submitted<=3||bridge.submitted%120==0)) bridge.log("XR submitted_frame=%u",bridge.submitted);
#endif
}
unsigned xr_eye_size() {return bridge.eye_size;}
bool xr_frame_ready() {return bridge.frame_open&&bridge.valid_pose&&bridge.recentered;}
bool xr_aim_cursor_visible() {return bridge.cursor_visible;}
void xr_set_game_projection(const D3DMATRIX& projection) {bridge.game_projection=projection;}
void xr_set_cinematic_guard(bool cinematic) {
    if(cinematic&&!bridge.cinematic_guard&&bridge.log) bridge.log("VR_RELOAD cinematic_guard=1 gesture_cancelled=1 B_preserved=1");
    bridge.cinematic_guard=cinematic;
    if(cinematic) {bridge.reload_gesture.reset();bridge.left_reload_gesture.reset();}
}
static bool aim_hit(DirectX::XMFLOAT3& hit,unsigned hand=0) {
    if(hand>1||!bridge.input.active||!(hand?bridge.dual_wield&&bridge.left_aim_valid:bridge.aim_valid)||!bridge.recentered) return false;
    using namespace DirectX;
    auto aim=calibrated_aim(bridge.units,hand)*XMMatrixInverse(nullptr,hotd2_xr::pose_lh(bridge.origin,bridge.units));
    return hotd2_xr::aim_plane(aim,4*bridge.units,hit);
}
static XrGameInput hand_game_input(unsigned hand) {
    auto input=bridge.input;DirectX::XMFLOAT3 hit;
    input.aim_valid=aim_hit(hit,hand);
    if(hand)input.fire=bridge.left_fire;
    if(input.aim_valid) {
        float sx=bridge.game_projection._11,sy=bridge.game_projection._22;
        if(sx<=0||sy<=0) {sx=2;sy=2.667f;}
        input.x=(1+hit.x*sx/hit.z)*320;input.y=(1-hit.y*sy/hit.z)*240;
        input.aim_valid=std::isfinite(input.x)&&std::isfinite(input.y);
    }
    if(!input.aim_valid) input.fire=false;
    return input;
}
static void update_dual_shots() {
    if(!bridge.dual_wield)return;
    auto right=hand_game_input(0),left=hand_game_input(1);
    bool valid[]={right.aim_valid,left.aim_valid},fire[]={bridge.trigger_down,bridge.left_trigger_down};float x[]={right.x,left.x},y[]={right.y,left.y};
    if(bridge.input.reload){bridge.shots.reset();return;}
    bridge.shots.observe(bridge.input.active&&bridge.focused&&bridge.valid_pose&&bridge.recentered,valid,fire,x,y,GetTickCount64());
}
XrGameInput xr_game_input() {
    auto input=hand_game_input(0);
    if(bridge.dual_wield) {
        input.fire=false;
        // Empty-gun presses still reach the native engine's dry-fire/reload voice
        // path; the selected native zero ammo count prevents a projectile.
        if(bridge.shots.phase!=hotd2_input::DualShotRouter::Idle&&input.active){input.x=bridge.shots.current.x;input.y=bridge.shots.current.y;input.aim_valid=true;input.fire=bridge.shots.phase==hotd2_input::DualShotRouter::Down&&!input.reload;}
    }
    return input;
}
bool xr_dual_wield_enabled(){return bridge.dual_wield;}
void xr_enable_independent_magazines(bool enabled){
    bool desired=enabled&&bridge.dual_wield;
    if(desired!=bridge.independent_magazines){
        bridge.magazine_counts_valid=false;
        bridge.health_valid=false;
        bridge.shots.reset();bridge.reload_gesture.reset();bridge.left_reload_gesture.reset();
        bridge.magazine_reload_pulse[0]=bridge.magazine_reload_pulse[1]=false;bridge.magazine_button_held=false;
    }
    bridge.independent_magazines=desired;bridge.magazine_reload_mask=0;
}
XrMagazineControl xr_take_magazine_control(){
    XrMagazineControl out;
    if(!bridge.independent_magazines||!bridge.input.active)return out;
    if(bridge.shots.phase!=hotd2_input::DualShotRouter::Idle)out.hand=static_cast<int>(bridge.shots.current.hand);
    out.reload_mask=bridge.magazine_reload_mask;bridge.magazine_reload_mask=0;return out;
}
void xr_publish_magazines(int right,int left){
    bridge.magazine_counts_valid=right>=0&&right<=6&&left>=0&&left<=6;
    bridge.magazine_rounds[0]=right;bridge.magazine_rounds[1]=left;
}
void xr_configure_ammo_gauges(bool enabled){bridge.ammo_gauges=enabled;}
void xr_configure_health_gauge(bool enabled){bridge.health_gauge=enabled;}
void xr_publish_health(int current,int maximum){
    bridge.health_valid=maximum>0&&maximum<=9&&current>=0&&current<=maximum;
    bridge.health=current;bridge.health_maximum=maximum;
}
bool xr_replace_native_status(){
    return bridge.health_gauge&&bridge.health_valid&&bridge.ammo_gauges&&bridge.independent_magazines&&
        bridge.magazine_counts_valid&&bridge.input.active&&xr_frame_ready();
}
bool xr_health_gauge(int& current,int& maximum){
    if(!xr_replace_native_status()||bridge.cinematic_guard)return false;
    current=bridge.health;maximum=bridge.health_maximum;return true;
}
bool xr_magazine_gauges(int& right,int& left){
    if(!bridge.ammo_gauges||!bridge.independent_magazines||!bridge.magazine_counts_valid||
       !bridge.input.active||bridge.cinematic_guard||!xr_frame_ready())return false;
    right=bridge.magazine_rounds[0];left=bridge.magazine_rounds[1];return true;
}
bool xr_ammo_gauge_vertex(unsigned eye,float x,float y,float& out_x,float& out_y){
    if(eye>1||!xr_frame_ready()||!std::isfinite(x)||!std::isfinite(y))return false;
    using namespace DirectX;
    // Follow the current head, rather than the game/recenter origin, with real
    // per-eye disparity. Never use identical screen coordinates in both eyes.
    XrPosef head=bridge.views[0].pose;
    head.position.x=(head.position.x+bridge.views[1].pose.position.x)*.5f;
    head.position.y=(head.position.y+bridge.views[1].pose.position.y)*.5f;
    head.position.z=(head.position.z+bridge.views[1].pose.position.z)*.5f;
    auto transform=hotd2_xr::pose_lh(head,1)*XMMatrixInverse(nullptr,hotd2_xr::pose_lh(bridge.views[eye].pose,1));
    auto point=XMVector3TransformCoord(XMVectorSet(x*2,-y*2,2,1),transform);
    if(XMVectorGetZ(point)<=.01f)return false;
    auto p=XMVector3TransformCoord(point,hotd2_xr::projection_lh(bridge.views[eye].fov,.01f,100));
    out_x=(XMVectorGetX(p)+1)*.5f;out_y=(1-XMVectorGetY(p))*.5f;
    return std::isfinite(out_x)&&std::isfinite(out_y);
}
void xr_native_cursor_poll(){if(bridge.dual_wield)bridge.shots.cursor_poll();}
void xr_native_mouse_poll(){if(bridge.dual_wield)bridge.shots.mouse_poll();}
void xr_native_present(){
    if(!bridge.dual_wield)return;
    auto before=bridge.shots.phase;auto shot=bridge.shots.current;
    bridge.shots.present(GetTickCount64());
    if(before==hotd2_input::DualShotRouter::Down&&bridge.shots.phase==hotd2_input::DualShotRouter::Up&&bridge.log)
        bridge.log("INPUT dual_dispatch hand=%s aim=(%.1f,%.1f) routed_right=%u routed_left=%u discarded=%u native_acceptance_unverified=1",shot.hand?"left":"right",shot.x,shot.y,bridge.shots.delivered[0],bridge.shots.delivered[1],bridge.shots.discarded);
}
static bool project_eye(unsigned eye,const DirectX::XMFLOAT3& point,float& x,float& y) {
    if(!xr_frame_ready()||eye>1) return false;
    using namespace DirectX;
    auto transform=hotd2_xr::pose_lh(bridge.origin,bridge.units)*XMMatrixInverse(nullptr,hotd2_xr::pose_lh(bridge.views[eye].pose,bridge.units));
    auto eye_point=XMVector3TransformCoord(XMLoadFloat3(&point),transform);
    if(XMVectorGetZ(eye_point)<=0.01f) return false;
    auto p=XMVector3TransformCoord(eye_point,hotd2_xr::projection_lh(bridge.views[eye].fov,0.01f,10000));
    x=(XMVectorGetX(p)+1)*0.5f;y=(1-XMVectorGetY(p))*0.5f;
    return std::isfinite(x)&&std::isfinite(y);
}
bool xr_hud_vertex(unsigned eye,float x,float y,const D3DVIEWPORT7& source,const D3DMATRIX& projection,float& out_x,float& out_y) {
    float sx=projection._11,sy=projection._22;
    if(sx<=0||sy<=0) {sx=2;sy=2.667f;}
    float depth=2*bridge.units;
    DirectX::XMFLOAT3 point={(2*(x-source.dwX)/source.dwWidth-1)*depth/sx,(1-2*(y-source.dwY)/source.dwHeight)*depth/sy,depth};
    return project_eye(eye,point,out_x,out_y);
}
bool xr_pointer_vertex(unsigned eye,float& x,float& y) {
    return xr_hand_pointer_vertex(0,eye,x,y);
}
bool xr_hand_pointer_vertex(unsigned hand,unsigned eye,float& x,float& y) {
    if(!bridge.cursor_visible) return false;
    DirectX::XMFLOAT3 hit;
    if(!aim_hit(hit,hand)) return false;
    // Match the native crosshair's two-metre HUD plane. The original game still
    // receives coordinates from the four-metre aim intersection above.
    hit.x*=0.5f;hit.y*=0.5f;hit.z*=0.5f;
    return project_eye(eye,hit,x,y);
}
bool xr_gun_matrices(unsigned eye,D3DMATRIX& view,D3DMATRIX& projection) {
    return xr_hand_gun_matrices(0,eye,view,projection);
}
bool xr_hand_gun_matrices(unsigned hand,unsigned eye,D3DMATRIX& view,D3DMATRIX& projection) {
    if(hand>1||!xr_frame_ready()||!bridge.input.active||!(hand?bridge.dual_wield&&bridge.left_aim_valid:bridge.aim_valid)||eye>1) return false;
    using namespace DirectX;
    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result,calibrated_aim(1,hand)*XMMatrixInverse(nullptr,hotd2_xr::pose_lh(bridge.views[eye].pose,1)));
    memcpy(&view,&result,sizeof(view));
    XMStoreFloat4x4(&result,hotd2_xr::projection_lh(bridge.views[eye].fov,0.01f,100));
    memcpy(&projection,&result,sizeof(projection));return true;
}
