// ABI-neutral x86 tail forwarding for system-private exports used by d3dim700.dll.
// Preserve registers, flags, caller stack and the original calling convention.
#define FORWARD_EXPORT(name) \
    static FARPROC target_##name; \
    static void initialize_##name() { \
        target_##name=resolve(#name); \
        if(!target_##name) RaiseFailFastException(nullptr,nullptr,0); \
    } \
    extern "C" __declspec(naked) void Forward_##name() { \
        __asm { pushad } \
        __asm { pushfd } \
        __asm { call initialize_##name } \
        __asm { popfd } \
        __asm { popad } \
        __asm { jmp dword ptr [target_##name] } \
    }
FORWARD_EXPORT(AcquireDDThreadLock)
FORWARD_EXPORT(CompleteCreateSysmemSurface)
FORWARD_EXPORT(D3DParseUnknownCommand)
FORWARD_EXPORT(DDGetAttachedSurfaceLcl)
FORWARD_EXPORT(DDInternalLock)
FORWARD_EXPORT(DDInternalUnlock)
FORWARD_EXPORT(DSoundHelp)
FORWARD_EXPORT(DirectDrawCreate)
FORWARD_EXPORT(DirectDrawCreateClipper)
FORWARD_EXPORT(DirectDrawEnumerateA)
FORWARD_EXPORT(DirectDrawEnumerateExW)
FORWARD_EXPORT(DirectDrawEnumerateW)
FORWARD_EXPORT(DllCanUnloadNow)
FORWARD_EXPORT(DllGetClassObject)
FORWARD_EXPORT(GetDDSurfaceLocal)
FORWARD_EXPORT(GetOLEThunkData)
FORWARD_EXPORT(GetSurfaceFromDC)
FORWARD_EXPORT(RegisterSpecialCase)
FORWARD_EXPORT(ReleaseDDThreadLock)
FORWARD_EXPORT(SetAppCompatData)

