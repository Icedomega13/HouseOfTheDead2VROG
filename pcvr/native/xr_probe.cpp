#include <openxr/openxr.h>
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    uint32_t count=0;
    XrResult result=xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr);
    printf("Enumerate extensions result=%d count=%u\n",result,count);
    if(XR_FAILED(result)) return 2;
    std::vector<XrExtensionProperties> extensions(count);
    for(auto& e:extensions) e.type=XR_TYPE_EXTENSION_PROPERTIES;
    result=xrEnumerateInstanceExtensionProperties(nullptr,count,&count,extensions.data());
    if(XR_FAILED(result)) return 3;
    for(auto& e:extensions) if(strstr(e.extensionName,"D3D11")||strstr(e.extensionName,"headless"))
        printf("Extension %s\n",e.extensionName);
    XrInstanceCreateInfo info={}; info.type=XR_TYPE_INSTANCE_CREATE_INFO;
    strcpy_s(info.applicationInfo.applicationName,"HOTD2 PCVR capability probe");
    info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    XrInstance instance=XR_NULL_HANDLE;
    result=xrCreateInstance(&info,&instance);
    printf("Create instance result=%d\n",result);
    if(XR_FAILED(result)) return 4;
    XrInstanceProperties properties={}; properties.type=XR_TYPE_INSTANCE_PROPERTIES;
    xrGetInstanceProperties(instance,&properties);
    printf("Runtime=%s version=%llu\n",properties.runtimeName,properties.runtimeVersion);
    XrSystemGetInfo system_info={}; system_info.type=XR_TYPE_SYSTEM_GET_INFO;
    system_info.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    result=xrGetSystem(instance,&system_info,&system);
    printf("Get HMD system result=%d system=%llu\n",result,system);
    if(XR_SUCCEEDED(result)) {
        XrSystemProperties p={}; p.type=XR_TYPE_SYSTEM_PROPERTIES;
        xrGetSystemProperties(instance,system,&p);
        printf("HMD=%s orientation=%u position=%u\n",p.systemName,
            p.trackingProperties.orientationTracking,p.trackingProperties.positionTracking);
    }
    xrDestroyInstance(instance);
    return XR_SUCCEEDED(result)?0:5;
}
