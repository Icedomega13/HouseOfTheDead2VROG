// In-process diagnostic vtable hooks. Original identity/refcounts are preserved.
// Shared tables are changed only inside this process; the probe is pinned once used.
static SRWLOCK hook_lock=SRWLOCK_INIT;
struct SlotHook { void** table; unsigned slot; void* original; };
static SlotHook slot_hooks[128]={};
static unsigned slot_count=0;
static void install_hook(void* object,unsigned slot,void* replacement) {
    auto table=*reinterpret_cast<void***>(object);
    AcquireSRWLockExclusive(&hook_lock);
    for(unsigned i=0;i<slot_count;++i) if(slot_hooks[i].table==table && slot_hooks[i].slot==slot) {
        ReleaseSRWLockExclusive(&hook_lock); return;
    }
    DWORD old=0,ignored=0;
    if(slot_count<128 && VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old)) {
        HMODULE pinned=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&install_hook),&pinned);
        slot_hooks[slot_count++]={table,slot,table[slot]};
        InterlockedExchangePointer(table+slot,replacement);
        VirtualProtect(table+slot,sizeof(void*),old,&ignored);
    }
    ReleaseSRWLockExclusive(&hook_lock);
}
template<class Fn> static Fn original_slot(void* object,unsigned slot) {
    auto table=*reinterpret_cast<void***>(object); void* result=nullptr;
    AcquireSRWLockShared(&hook_lock);
    for(unsigned i=0;i<slot_count;++i) if(slot_hooks[i].table==table && slot_hooks[i].slot==slot) {
        result=slot_hooks[i].original; break;
    }
    ReleaseSRWLockShared(&hook_lock);
    if(!result) RaiseFailFastException(nullptr,nullptr,0);
    return reinterpret_cast<Fn>(result);
}
