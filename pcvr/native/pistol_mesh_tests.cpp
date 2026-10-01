#include "pistol_mesh.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

int wmain(int argc,wchar_t** argv) {
    unsigned checks=0;auto require=[&](bool ok,const char* label){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}};
    const auto& mesh=hotd2_pistol::mesh();
    require(sizeof(hotd2_pistol::Vertex)==16,"D3D7 XYZ/diffuse vertex layout");
    require(!mesh.empty()&&mesh.size()%3==0&&mesh.size()/3<5000,"bounded complete triangles");
    bool finite=true,opaque=true,area=true;float minz=1,maxz=-1,miny=1,maxy=-1,maxx=0;
    for(auto v:mesh){finite=finite&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);opaque=opaque&&(v.color>>24)==255;
        minz=std::min(minz,v.z);maxz=std::max(maxz,v.z);miny=std::min(miny,v.y);maxy=std::max(maxy,v.y);maxx=std::max(maxx,std::fabs(v.x));}
    for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];auto normal=hotd2_pistol::cross({b.x-a.x,b.y-a.y,b.z-a.z},{c.x-a.x,c.y-a.y,c.z-a.z});area=area&&hotd2_pistol::dot(normal,normal)>1e-20f;}
    require(finite,"finite GPU geometry");require(opaque,"opaque fixed vertex colours");require(area,"no collapsed triangles");
    require(minz>-.08f&&std::fabs(maxz-.1802f)<.00001f,"accepted muzzle reach preserved");
    require(miny>-.12f&&maxy<.04f&&maxx<.025f,"hand-sized pistol envelope");
    // The empty trigger-guard opening must not be filled by a side cap.
    bool opening=true;
    for(size_t i=0;i<mesh.size();i+=3){auto a=mesh[i],b=mesh[i+1],c=mesh[i+2];float y=(a.y+b.y+c.y)/3,z=(a.z+b.z+c.z)/3;
        if(std::fabs(a.x-.009f)<1e-6f&&std::fabs(b.x-.009f)<1e-6f&&std::fabs(c.x-.009f)<1e-6f&&y>-.045f&&y<-.033f&&z>.045f&&z<.061f) opening=false;}
    require(opening,"trigger-guard opening remains open");
    if(argc==2){FILE* file=nullptr;if(_wfopen_s(&file,argv[1],L"wb")||!file)return 2;
        fputs("# HotD2VR authored AMS-inspired pistol; metres, +Z bore; vertex RGB\n",file);
        for(auto v:mesh)fprintf(file,"v %.8g %.8g %.8g %.5g %.5g %.5g\n",v.x,v.y,v.z,((v.color>>16)&255)/255.0,((v.color>>8)&255)/255.0,(v.color&255)/255.0);
        for(size_t i=0;i<mesh.size();i+=3)fprintf(file,"f %zu %zu %zu\n",i+1,i+2,i+3);fclose(file);}
    printf("PASS %u pistol geometry checks; %zu triangles\n",checks,mesh.size()/3);return 0;
}
