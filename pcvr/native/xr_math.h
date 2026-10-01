#pragma once
#include <DirectXMath.h>
#include <openxr/openxr.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace hotd2_xr {
// D3D's homogeneous clip volume is -W<=X,Y<=W and 0<=Z<=W.
// Normalize the six planes in model space so scaled WORLD matrices are handled.
inline bool sphere_frustum(DirectX::FXMMATRIX model_to_clip,const DirectX::XMFLOAT3& center,float radius,unsigned& status) {
    using namespace DirectX;
    if(!std::isfinite(radius)||radius<0||!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)) return false;
    auto columns=XMMatrixTranspose(model_to_clip);
    XMVECTOR planes[]={columns.r[3]+columns.r[0],columns.r[3]-columns.r[0],
        columns.r[3]+columns.r[1],columns.r[3]-columns.r[1],columns.r[2],columns.r[3]-columns.r[2]};
    status=0;
    for(auto plane:planes) {
        XMFLOAT4 p;XMStoreFloat4(&p,plane);
        float length=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
        if(!std::isfinite(length)||length<=0||!std::isfinite(p.w)) return false;
        float distance=(p.x*center.x+p.y*center.y+p.z*center.z+p.w)/length;
        if(!std::isfinite(distance)) return false;
        if(distance < -radius) status=2;
        else if(distance<=radius&&status<2) status=1;
    }
    return true;
}
inline bool headset_visible(unsigned native,unsigned computed_native,unsigned left,unsigned right) {
    return native==2&&computed_native==2&&(left<2||right<2);
}
// Device7 uses D3DSTATUS_CLIP union/intersection bits; Device3's D3DVIS
// enum is a different encoding. Limit this to the six built-in clip planes.
inline unsigned native_sphere_status(unsigned flags) {
    return flags&0x3f000u?2u:flags&0x3fu?1u:0u;
}
inline bool trigger_pressed(float value,bool previously_down) {
    return std::isfinite(value)&&value>(previously_down?0.4f:0.55f);
}
inline DirectX::XMMATRIX pose_lh(const XrPosef& pose,float units) {
    using namespace DirectX;
    // Reflect OpenXR's right-handed coordinates into the game's +Z-forward view space.
    auto q=XMVectorSet(-pose.orientation.x,-pose.orientation.y,pose.orientation.z,pose.orientation.w);
    auto r=XMMatrixRotationQuaternion(XMQuaternionNormalize(q));
    r.r[3]=XMVectorSet(pose.position.x*units,pose.position.y*units,-pose.position.z*units,1);
    return r;
}
inline DirectX::XMMATRIX projection_lh(const XrFovf& fov,float near_z,float far_z) {
    return DirectX::XMMatrixPerspectiveOffCenterLH(std::tan(fov.angleLeft)*near_z,
        std::tan(fov.angleRight)*near_z,std::tan(fov.angleDown)*near_z,
        std::tan(fov.angleUp)*near_z,near_z,far_z);
}
inline XrPosef level_origin(const XrPosef& head) {
    using namespace DirectX;
    auto q=XMVectorSet(head.orientation.x,head.orientation.y,head.orientation.z,head.orientation.w);
    auto forward=XMVector3Rotate(XMVectorSet(0,0,-1,0),XMQuaternionNormalize(q));
    float yaw=std::atan2(-XMVectorGetX(forward),-XMVectorGetZ(forward));
    XrPosef out=head;out.orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};return out;
}
// Intersect an aim ray with a plane in the original camera's coordinates.
inline bool aim_plane(DirectX::FXMMATRIX aim,float depth,DirectX::XMFLOAT3& hit) {
    using namespace DirectX;
    auto position=aim.r[3],direction=aim.r[2];
    float dz=XMVectorGetZ(direction);
    if(dz<=0.0001f) return false;
    float t=(depth-XMVectorGetZ(position))/dz;
    if(t<=0) return false;
    XMStoreFloat3(&hit,XMVectorAdd(position,XMVectorScale(direction,t)));return true;
}
inline bool unit_depth_plane(DirectX::FXMMATRIX camera,const void* vertices,size_t count) {
    if(!vertices||!count||count>4096) return false;
    for(size_t i=0;i<count;++i) {
        DirectX::XMFLOAT3 point;
        std::memcpy(&point,static_cast<const char*>(vertices)+i*32,sizeof(point));
        auto transformed=DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point),camera);
        float x=DirectX::XMVectorGetX(transformed),y=DirectX::XMVectorGetY(transformed),z=DirectX::XMVectorGetZ(transformed);
        // Observed native bars fit within +/-0.6 on the unit-depth screen.
        // Reject larger geometry even if it happens to lie at camera Z=1.
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
            std::fabs(x)>0.6f||std::fabs(y)>0.6f||std::fabs(z-1)>0.02f) return false;
    }
    return true;
}
inline bool cinematic_bar(DirectX::FXMMATRIX camera,const void* vertices,size_t count) {
    // Observed native letterbox: X +/-0.515, Z=1, height .10. It slides
    // from Y .30.. .40 to .40.. .50 around dialogue/area transitions.
    if(count!=4||!unit_depth_plane(camera,vertices,count)) return false;
    float low_x=1,high_x=-1,low_y=1,high_y=-1;
    for(size_t i=0;i<count;++i) {
        DirectX::XMFLOAT3 p;std::memcpy(&p,static_cast<const char*>(vertices)+i*32,sizeof(p));
        auto v=DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&p),camera);
        float x=DirectX::XMVectorGetX(v),y=DirectX::XMVectorGetY(v);
        low_x=std::min(low_x,x);high_x=std::max(high_x,x);low_y=std::min(low_y,y);high_y=std::max(high_y,y);
    }
    bool height=std::fabs(high_y-low_y-.10f)<.005f;
    bool top=height&&low_y>=.295f&&high_y<=.505f;
    bool bottom=height&&low_y>=-.505f&&high_y<=-.295f;
    if(std::fabs(low_x+.515f)>=.015f||std::fabs(high_x-.515f)>=.015f||(!top&&!bottom)) return false;
    unsigned corners=0;
    for(size_t i=0;i<count;++i) {
        DirectX::XMFLOAT3 p;std::memcpy(&p,static_cast<const char*>(vertices)+i*32,sizeof(p));
        auto v=DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&p),camera);
        float x=DirectX::XMVectorGetX(v),y=DirectX::XMVectorGetY(v);
        bool right=std::fabs(x-high_x)<.015f,upper=std::fabs(y-high_y)<.015f;
        if((!right&&std::fabs(x-low_x)>=.015f)||(!upper&&std::fabs(y-low_y)>=.015f)) return false;
        corners|=1u<<((right?1u:0u)+(upper?2u:0u));
    }
    return corners==15;
}
}
