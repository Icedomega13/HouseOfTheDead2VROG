// Investigation only: never compiled into the desktop-shortcut production path.
#ifdef HOTD2_CONTROLLER_REPLAY_TEST
static unsigned research_hz=0;
static bool research_timing=false,research_verified=false;
static bool research_no_vsync=false;
static LARGE_INTEGER research_frequency={},research_first={},research_previous={};
static unsigned research_frames=0,research_capture=0;
static std::vector<double> research_intervals;
static void configure_timing_research(const wchar_t* path) {
    research_timing=GetPrivateProfileIntW(L"OpenXR",L"ReplayTimingAudit",0,path)!=0;
    research_no_vsync=GetPrivateProfileIntW(L"OpenXR",L"ReplayNoVSync",0,path)!=0;
    unsigned value=GetPrivateProfileIntW(L"OpenXR",L"ReplayNativeHz",0,path);
    research_hz=(value==60||value==90||value==120)?value:0;
    auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const BYTE setter[]={0x8b,0x4c,0x24,0x04,0x83,0xec,0x08};
    const BYTE startup[]={0x6a,0x3c,0xc7,0x05,0x9c,0xda,0x7d,0x00,0x01,0,0,0};
    research_verified=memcmp(base+0xA5650,setter,sizeof(setter))==0&&memcmp(base+0x9E550,startup,sizeof(startup))==0;
    QueryPerformanceFrequency(&research_frequency);research_intervals.reserve(120);
    if(research_timing) log_line("TIMING_RESEARCH enabled=1 requested_native_hz=%u verified_executable=%d production_limiter_unchanged=%d",research_hz,research_verified,research_hz==0);
}
static void timing_research_begin() {
    if(!research_timing||!research_verified||!research_hz) return;
    auto base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    auto period=reinterpret_cast<float*>(base+0x1987B4);
    float target=1000.0f/research_hz;
    if(*period>0&&std::isfinite(*period)&&std::fabs(*period-target)>.001f) {
        log_line("TIMING_RESEARCH isolated_period_override previous_ms=%.6g target_ms=%.6g other_native_timing_unchanged=1",*period,target);
        *period=target; // Runtime data in the owned child process; no EXE bytes patched.
    }
}
static DWORD timing_research_flip_flags(DWORD flags) {return research_timing&&research_no_vsync?flags|DDFLIP_NOVSYNC:flags;}
static void timing_research_present(IDirect3DDevice7* device) {
    if(!research_timing||research_frequency.QuadPart<=0) return;
    LARGE_INTEGER now={};QueryPerformanceCounter(&now);
    if(!research_first.QuadPart) research_first=now;
    if(research_previous.QuadPart) {
        research_intervals.push_back(1000.0*(now.QuadPart-research_previous.QuadPart)/research_frequency.QuadPart);
        if(research_intervals.size()==120) {
            auto sorted=research_intervals;std::sort(sorted.begin(),sorted.end());
            double total=0;for(double interval:sorted) total+=interval;
            auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
            float period=research_verified?*reinterpret_cast<const float*>(base+0x1987B4):0;
            DWORD mode=research_verified?*reinterpret_cast<const DWORD*>(base+0x5C8E98):0;
            log_line("TIMING_RESEARCH frames=120 mean_ms=%.4f fps=%.4f min_ms=%.4f p95_ms=%.4f max_ms=%.4f native_period_ms=%.6g native_mode=%lu",total/120,120000/total,sorted.front(),sorted[113],sorted.back(),period,mode);
            research_intervals.clear();
        }
    }
    research_previous=now;++research_frames;
    double seconds=static_cast<double>(now.QuadPart-research_first.QuadPart)/research_frequency.QuadPart;
    const unsigned targets[]={10,20,30};
    if(device&&research_capture<3&&seconds>=targets[research_capture]) {
        unsigned target=targets[research_capture++];
        log_line("TIMING_RESEARCH wall_capture_seconds=%u actual_seconds=%.4f presentations=%u",target,seconds,research_frames);
        capture_frame(device,200000+target*1000+1);
        if(eye_atlas) {SavedGameState saved(device,false,log_line,"timing_capture");if(saved.bind(eye_atlas)) capture_frame(device,200000+target*1000+2);}
    }
}
#else
static void configure_timing_research(const wchar_t*) {}
static void timing_research_begin() {}
static DWORD timing_research_flip_flags(DWORD flags) {return flags;}
static void timing_research_present(IDirect3DDevice7*) {}
#endif
