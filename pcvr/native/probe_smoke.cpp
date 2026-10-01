#define WIN32_LEAN_AND_MEAN
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <cstdio>

static BOOL WINAPI adapter(GUID*, LPSTR, LPSTR, LPVOID count, HMONITOR) {
    ++*static_cast<unsigned*>(count);
    return DDENUMRET_OK;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) { fputs("Usage: probe_smoke.exe <absolute-path-to-probe-ddraw.dll>\n", stderr); return 2; }
    HMODULE mod = LoadLibraryW(argv[1]);
    if (!mod) { printf("Load failed: %lu\n", GetLastError()); return 3; }
    using Create = HRESULT (WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    using Enum = HRESULT (WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    auto create = reinterpret_cast<Create>(GetProcAddress(mod, "DirectDrawCreateEx"));
    auto enumerate = reinterpret_cast<Enum>(GetProcAddress(mod, "DirectDrawEnumerateExA"));
    if (!create || !enumerate) return 4;
    unsigned count = 0;
    HRESULT eh = enumerate(adapter, &count, 0);
    IDirectDraw7* draw = nullptr;
    HRESULT ch = create(nullptr, reinterpret_cast<void**>(&draw), IID_IDirectDraw7, nullptr);
    printf("Enumerate hr=0x%08lx adapters=%u; Create IDirectDraw7 hr=0x%08lx\n", eh, count, ch);
    HRESULT qh=E_FAIL;
    if (draw) {
        IDirect3D7* d3d=nullptr;
        qh=draw->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d));
        printf("QueryInterface IDirect3D7 hr=0x%08lx\n",qh);
        if(d3d) d3d->Release();
        draw->Release();
    }
    FreeLibrary(mod);
    return SUCCEEDED(eh) && SUCCEEDED(ch) && SUCCEEDED(qh) ? 0 : 5;
}
