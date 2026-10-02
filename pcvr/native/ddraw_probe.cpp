// Original HOTD2 DirectDraw proxy with rendering diagnostics, stereo preview,
// and an experimental OpenXR bridge. COM object identity is retained.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <dinput.h>
#include <cstdio>
#include <cstdarg>
#include <intrin.h>
#include <vector>
#include <array>
#include <cstddef>
#include "xr_bridge.h"
#include "xr_math.h"

static HMODULE self;
static HMODULE real;
static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static SRWLOCK log_lock = SRWLOCK_INIT;

static void log_line(const char* format, ...) {
    wchar_t path[32768] = {};
    DWORD n = GetModuleFileNameW(self, path, 32768);
    if (!n || n >= 32768) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || slash-path+24 >= 32768) return;
    wcscpy_s(slash+1, 32768-(slash+1-path), L"hotd2-ddraw-probe.log");
    AcquireSRWLockExclusive(&log_lock);
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"a") == 0 && file) {
        fprintf(file, "[%lu] ", GetTickCount());
        va_list args; va_start(args, format); vfprintf(file, format, args); va_end(args);
        fputc('\n', file); fclose(file);
    }
    ReleaseSRWLockExclusive(&log_lock);
}

static BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[32768] = {};
    DWORD n=GetModuleFileNameW(self,path,32768);
    if(!n || n>=32768) return FALSE;
    wchar_t* slash=wcsrchr(path,L'\\');
    if(!slash || slash-path+24>=32768) return FALSE;
    wcscpy_s(slash+1,32768-(slash+1-path),L"ddraw_backend.dll");
    bool local=GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;
    if(!local) {
        UINT size=GetSystemDirectoryW(path,32768);
        if(!size || size>=32768-11) return FALSE;
        wcscat_s(path,L"\\ddraw.dll");
    }
    real = LoadLibraryW(path); // Explicit backend path avoids loading this proxy again.
    log_line("probe_version=22 arch=x86 backend=%ls loaded=%d error=%lu", path,
        real != nullptr, real ? 0 : GetLastError());
    return real != nullptr;
}

static FARPROC resolve(const char* name) {
    if (!InitOnceExecuteOnce(&once, initialize, nullptr, nullptr)) return nullptr;
    auto fn = GetProcAddress(real, name);
    if (!fn) log_line("missing_export=%s error=%lu", name, GetLastError());
    return fn;
}

#include "frame_capture.inl"
#include "com_trace.inl"

extern "C" HRESULT WINAPI ProbeCreateEx(GUID* adapter, LPVOID* object, REFIID iid, IUnknown* outer) {
    using Fn = HRESULT (WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    auto fn = reinterpret_cast<Fn>(resolve("DirectDrawCreateEx"));
    if (!fn) return E_FAIL;
    static INIT_ONCE config_once=INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&config_once,[](PINIT_ONCE,PVOID,PVOID*)->BOOL {configure_stereo();return TRUE;},nullptr,nullptr);
    HRESULT hr = fn(adapter, object, iid, outer);
    log_line("DirectDrawCreateEx iid=%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x hr=0x%08lx object=%p",
        iid.Data1, iid.Data2, iid.Data3, iid.Data4[0], iid.Data4[1], iid.Data4[2],
        iid.Data4[3], iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7], hr,
        SUCCEEDED(hr) && object ? *object : nullptr);
    if(SUCCEEDED(hr)&&object&&*object&&iid==IID_IDirectDraw7) trace_draw7(*object);
    return hr;
}

extern "C" HRESULT WINAPI ProbeEnumerateExA(LPDDENUMCALLBACKEXA callback, LPVOID context, DWORD flags) {
    using Fn = HRESULT (WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    auto fn = reinterpret_cast<Fn>(resolve("DirectDrawEnumerateExA"));
    if (!fn) return E_FAIL;
    HRESULT hr = fn(callback, context, flags);
    log_line("DirectDrawEnumerateExA flags=0x%08lx hr=0x%08lx", flags, hr);
    return hr;
}

#include "ddraw_forwarders.inl"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) self = instance;
    return TRUE; // No file IO, loading, COM calls, or locking under loader lock.
}
