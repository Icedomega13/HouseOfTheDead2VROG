#include "xr_math.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks=0;
static void close(float a,float b,const char* description) {
    ++checks;
    if(std::fabs(a-b)>0.0001f) {fprintf(stderr,"FAIL %s: %.8g != %.8g\n",description,a,b);exit(1);}
}
int main() {
    using namespace DirectX;
    XrPosef origin={};origin.orientation.w=1;
    XrPosef left=origin,right=origin;
    left.position.x=-0.032f;right.position.x=0.032f;
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m,XMMatrixInverse(nullptr,hotd2_xr::pose_lh(left,100)));
    close(m._41,3.2f,"left-eye view translation");
    XMStoreFloat4x4(&m,XMMatrixInverse(nullptr,hotd2_xr::pose_lh(right,100)));
    close(m._41,-3.2f,"right-eye view translation");
    XrPosef moved=origin;moved.position={1,2,-3};
    XMStoreFloat4x4(&m,hotd2_xr::pose_lh(moved,100));
    close(m._41,100,"metres to game units X");close(m._42,200,"metres to game units Y");close(m._43,300,"RH to LH Z");
    moved.orientation={0,0.382683432f,0,0.923879532f};
    auto pose=hotd2_xr::pose_lh(moved,100);
    XMStoreFloat4x4(&m,pose*XMMatrixInverse(nullptr,pose));
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)close(m.m[r][c],r==c?1.0f:0.0f,"recenter neutral identity");
    XrPosef yaw=origin;yaw.orientation={0,0.707106781f,0,0.707106781f};
    XMFLOAT3 direction;
    XMStoreFloat3(&direction,XMVector3TransformNormal(XMVectorSet(0,0,1,0),hotd2_xr::pose_lh(yaw,1)));
    close(direction.x,-1,"OpenXR left yaw maps to game left");close(direction.z,0,"yaw rotates forward");
    XrFovf fov={-0.7f,0.8f,0.75f,-0.65f};float near_z=0.8f,far_z=8000;
    auto p=hotd2_xr::projection_lh(fov,near_z,far_z);
    auto project=[&](float x,float y,float z) {XMFLOAT3 out;XMStoreFloat3(&out,XMVector3TransformCoord(XMVectorSet(x,y,z,1),p));return out;};
    close(project(std::tan(fov.angleLeft)*near_z,0,near_z).x,-1,"left FOV boundary");
    close(project(std::tan(fov.angleRight)*near_z,0,near_z).x,1,"right FOV boundary");
    close(project(0,std::tan(fov.angleUp)*near_z,near_z).y,1,"upper FOV boundary");
    close(project(0,std::tan(fov.angleDown)*near_z,near_z).y,-1,"lower FOV boundary");
    close(project(0,0,near_z).z,0,"D3D near depth");close(project(0,0,far_z).z,1,"D3D far depth");
    // Recenter retains yaw and position while removing initial pitch/roll.
    XrPosef tilted=origin;
    XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionRotationRollPitchYaw(0.3f,0.6f,0.2f));
    tilted.orientation={q.x,q.y,q.z,q.w};tilted.position={1,2,3};
    auto level=hotd2_xr::level_origin(tilted);
    close(level.orientation.x,0,"recenter removes pitch");close(level.orientation.z,0,"recenter removes roll");
    close(level.position.y,2,"recenter preserves current height");
    XMFLOAT3 a,b;
    XMStoreFloat3(&a,XMVector3Rotate(XMVectorSet(0,0,-1,0),XMLoadFloat4(&q)));
    XMFLOAT4 lq={level.orientation.x,level.orientation.y,level.orientation.z,level.orientation.w};
    XMStoreFloat3(&b,XMVector3Rotate(XMVectorSet(0,0,-1,0),XMLoadFloat4(&lq)));
    close(std::atan2(a.x,a.z),std::atan2(b.x,b.z),"recenter preserves forward heading");
    XMFLOAT3 hit;
    if(!hotd2_xr::aim_plane(XMMatrixTranslation(0.2f,-0.1f,0.3f),4,hit)) return 1;
    close(hit.x,0.2f,"aim plane controller X");close(hit.y,-0.1f,"aim plane controller Y");close(hit.z,4,"aim plane depth");
    if(hotd2_xr::aim_plane(XMMatrixRotationY(XM_PI),4,hit)) {fprintf(stderr,"FAIL backward ray accepted\n");return 1;}++checks;
    // Same physical point remains compatible with asymmetric eye FOVs: unproject both eyes back to the center.
    XMFLOAT3 point={10,5,200};
    for(const auto& eye:{left,right}) {
        auto eye_pose=hotd2_xr::pose_lh(eye,100);
        auto vp=XMMatrixInverse(nullptr,eye_pose)*p;
        auto clip=XMVector3TransformCoord(XMLoadFloat3(&point),vp);
        auto recovered=XMVector3TransformCoord(clip,XMMatrixInverse(nullptr,vp));
        XMStoreFloat3(&a,recovered);close(a.x,point.x,"stereo shared point X");close(a.y,point.y,"stereo shared point Y");
    }
    struct Vertex {float x,y,z,normal[3],uv[2];};
    Vertex plane[]={{-.5f,-.4f,-1,{0,0,1},{0,0}},{.5f,.4f,-1,{0,0,1},{1,1}}};
    if(!hotd2_xr::unit_depth_plane(XMMatrixScaling(1,1,-1),plane,2)) {fprintf(stderr,"FAIL screen-plane classification\n");return 1;}++checks;
    plane[1].z=-2;
    if(hotd2_xr::unit_depth_plane(XMMatrixScaling(1,1,-1),plane,2)) {fprintf(stderr,"FAIL ordinary geometry classified as screen-plane\n");return 1;}++checks;
    plane[1].z=-1;plane[1].x=2;
    if(hotd2_xr::unit_depth_plane(XMMatrixScaling(1,1,-1),plane,2)) {fprintf(stderr,"FAIL large world plane classified as screen-plane\n");return 1;}++checks;
    if(hotd2_xr::trigger_pressed(.5f,false)||!hotd2_xr::trigger_pressed(.6f,false)||
        !hotd2_xr::trigger_pressed(.5f,true)||hotd2_xr::trigger_pressed(.4f,true)) {fprintf(stderr,"FAIL trigger hysteresis\n");return 1;}++checks;
    if(hotd2_xr::trigger_pressed(NAN,true)) {fprintf(stderr,"FAIL invalid trigger value\n");return 1;}++checks;
    auto require=[&](bool ok,const char* label) {++checks;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}};
    Vertex bars[]={{-.515f,.30f,-1,{0,0,1},{0,0}},{.515f,.30f,-1,{0,0,1},{1,0}},
        {-.515f,.40f,-1,{0,0,1},{0,1}},{.515f,.40f,-1,{0,0,1},{1,1}}};
    auto camera=XMMatrixScaling(1,1,-1);
    require(hotd2_xr::cinematic_bar(camera,bars,4),"observed upper NPC letterbox recognized");
    for(auto& vertex:bars) vertex.y=-vertex.y;
    require(hotd2_xr::cinematic_bar(camera,bars,4),"observed lower NPC letterbox recognized");
    require(!hotd2_xr::cinematic_bar(camera,bars,3),"ordinary triangle preserved");
    bars[0].z=-2;require(!hotd2_xr::cinematic_bar(camera,bars,4),"world depth geometry preserved");
    bars[0].z=-1;bars[0].x=0;require(!hotd2_xr::cinematic_bar(camera,bars,4),"partial screen quad preserved");
    bars[2].x=0;require(!hotd2_xr::cinematic_bar(camera,bars,4),"narrow dialogue/text quad preserved");
    bars[0].x=bars[2].x=-.515f;for(auto& vertex:bars) vertex.y+=.2f;
    require(!hotd2_xr::cinematic_bar(camera,bars,4),"center screen quad preserved");
    bars[0].x=NAN;require(!hotd2_xr::cinematic_bar(camera,bars,4),"invalid quad rejected");
    for(unsigned step=0;step<=40;++step) {
        float inner=.30f+step*.0025f;
        for(unsigned i=0;i<4;++i){bars[i].x=i%2?.515f:-.515f;bars[i].y=inner+(i>=2?.10f:0);bars[i].z=-1;}
        require(hotd2_xr::cinematic_bar(camera,bars,4),"animated upper bar suppressed through full slide");
        for(auto& vertex:bars) vertex.y=-vertex.y;
        require(hotd2_xr::cinematic_bar(camera,bars,4),"animated lower bar suppressed through full slide");
    }
    for(unsigned i=0;i<4;++i){bars[i].y=i>=2?.50f:.20f;}
    require(!hotd2_xr::cinematic_bar(camera,bars,4),"large fade/dialogue panel preserved");
    for(unsigned i=0;i<4;++i){bars[i].y=i>=2?.44f:.36f;}
    require(!hotd2_xr::cinematic_bar(camera,bars,4),"wrong-height edge panel preserved");
    auto clip=XMMatrixPerspectiveFovLH(XM_PIDIV2,1,1,100);
    unsigned visibility=99;
    auto sphere=[&](XMMATRIX matrix,XMFLOAT3 center,float radius,unsigned expected,const char* label) {
        require(hotd2_xr::sphere_frustum(matrix,center,radius,visibility)&&visibility==expected,label);
    };
    sphere(clip,{0,0,10},1,0,"forward sphere inside");
    sphere(clip,{30,0,10},1,2,"right sphere outside");
    sphere(clip,{-30,0,10},1,2,"left sphere outside");
    sphere(clip,{0,30,10},1,2,"upper sphere outside");
    sphere(clip,{0,-30,10},1,2,"lower sphere outside");
    sphere(clip,{0,0,-10},1,2,"rear sphere outside");
    sphere(clip,{0,0,200},1,2,"far plane retained");
    sphere(clip,{0,0,1},.1f,1,"near intersection retained");
    sphere(clip,{10,0,10},1,1,"side intersection retained");
    sphere(XMMatrixRotationY(XM_PI)*clip,{0,0,-10},1,0,"rear sphere visible after half turn");
    sphere(XMMatrixScaling(2,3,4)*clip,{0,0,3},.1f,0,"nonuniform world scale supported");
    sphere(XMMatrixTranslation(0,0,20)*clip,{0,0,-10},1,0,"translated object supported");
    require(!hotd2_xr::sphere_frustum(clip,{NAN,0,10},1,visibility),"invalid center retains native result");
    require(!hotd2_xr::sphere_frustum(clip,{0,0,10},-1,visibility),"negative radius retains native result");
    require(!hotd2_xr::sphere_frustum(XMMatrixScaling(0,0,0),{0,0,10},1,visibility),"degenerate matrix retains native result");
    require(hotd2_xr::headset_visible(2,2,0,2),"left eye adds visibility");
    require(hotd2_xr::headset_visible(2,2,2,1),"right eye intersection adds visibility");
    require(!hotd2_xr::headset_visible(2,2,2,2),"outside all views remains culled");
    require(!hotd2_xr::headset_visible(0,2,0,0),"native visible object unchanged");
    require(!hotd2_xr::headset_visible(2,0,0,0),"native calculation disagreement unchanged");
    require(hotd2_xr::native_sphere_status(0)==0,"D3D7 inside encoding");
    require(hotd2_xr::native_sphere_status(2)==1,"D3D7 right intersection is not D3D3 outside enum");
    require(hotd2_xr::native_sphere_status(0x1001)==2,"D3D7 left outside encoding");
    require(hotd2_xr::native_sphere_status(0x20020)==2,"D3D7 far outside encoding");
    require(hotd2_xr::native_sphere_status(0x3f)==1,"conservative union clips without rejecting object");
    printf("PASS %u OpenXR coordinate/projection checks\n",checks);return 0;
}
