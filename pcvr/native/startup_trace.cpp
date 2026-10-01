// Bounded x86 debugger for an owned child process. No executable files are patched.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

template<class T> static T read_value(HANDLE p, DWORD address) {
    T value={}; SIZE_T got=0;
    ReadProcessMemory(p,reinterpret_cast<void*>(address),&value,sizeof(value),&got);
    return value;
}
static std::string read_string(HANDLE p, DWORD address) {
    std::string value;
    if(!address) return "<null>";
    for(DWORD i=0;i<2048;++i) {
        char c=read_value<char>(p,address+i); if(!c) break;
        value.push_back(c);
    }
    return value;
}
static DWORD imported_function(HANDLE p, DWORD base, const char* wanted) {
    auto dos=read_value<IMAGE_DOS_HEADER>(p,base);
    auto nt=read_value<IMAGE_NT_HEADERS32>(p,base+dos.e_lfanew);
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE || nt.Signature!=IMAGE_NT_SIGNATURE) return 0;
    DWORD table=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    for(DWORD i=0;i<256;++i) {
        auto d=read_value<IMAGE_IMPORT_DESCRIPTOR>(p,base+table+i*sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if(!d.Name) break;
        if(!d.OriginalFirstThunk) continue;
        for(DWORD j=0;j<4096;++j) {
            DWORD n=read_value<DWORD>(p,base+d.OriginalFirstThunk+j*4);
            if(!n) break;
            if(!(n&IMAGE_ORDINAL_FLAG32) && read_string(p,base+n+2)==wanted)
                return read_value<DWORD>(p,base+d.FirstThunk+j*4);
        }
    }
    return 0;
}
static bool write_byte(HANDLE p,DWORD address,BYTE value) {
    void* target=reinterpret_cast<void*>(address); DWORD old=0,ignored=0; SIZE_T wrote=0;
    if(!VirtualProtectEx(p,target,1,PAGE_EXECUTE_READWRITE,&old)) return false;
    bool ok=WriteProcessMemory(p,target,&value,1,&wrote)!=FALSE && wrote==1;
    VirtualProtectEx(p,target,1,old,&ignored); FlushInstructionCache(p,target,1); return ok;
}

static void snapshot(HANDLE process, const std::map<DWORD,HANDLE>& threads) {
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    bool symbols = SymInitialize(process, "", TRUE) != FALSE;
    for (const auto& t : threads) {
        CONTEXT c = {}; c.ContextFlags = CONTEXT_FULL;
        if (!GetThreadContext(t.second, &c)) continue;
        printf("THREAD %lu EIP=%08lx ESP=%08lx EBP=%08lx\n", t.first, c.Eip, c.Esp, c.Ebp);
        DWORD original_sp=c.Esp;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset=c.Eip; frame.AddrPC.Mode=AddrModeFlat;
        frame.AddrStack.Offset=c.Esp; frame.AddrStack.Mode=AddrModeFlat;
        frame.AddrFrame.Offset=c.Ebp; frame.AddrFrame.Mode=AddrModeFlat;
        for (int depth=0; depth<24; ++depth) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_I386, process, t.second, &frame, &c,
                nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME] = {};
            auto info = reinterpret_cast<SYMBOL_INFO*>(storage);
            info->SizeOfStruct = sizeof(SYMBOL_INFO); info->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            bool named = symbols && SymFromAddr(process, frame.AddrPC.Offset, &displacement, info);
            printf("  %08llx %s+%llx\n", frame.AddrPC.Offset, named ? info->Name : "?", displacement);
        }
        // Raw words aid diagnosis when this old binary has no unwind information.
        DWORD words[64] = {}; SIZE_T got = 0;
        if (ReadProcessMemory(process, reinterpret_cast<void*>(original_sp), words, sizeof(words), &got)) {
            printf("  STACK");
            for (size_t i=0; i<got/sizeof(DWORD); ++i) printf(" %08lx", words[i]);
            puts("");
        }
    }
    if (symbols) SymCleanup(process);
}

int wmain(int argc, wchar_t** argv) {
    if (argc<2 || argc>4) { fputs("Usage: startup_trace.exe <absolute-game-exe> [seconds:1-90] [--visible-game]\n",stderr); return 2; }
    DWORD duration=argc>=3?wcstoul(argv[2],nullptr,10):5;
    if(duration<1 || duration>90) return 2;
    bool visible=argc==4 && wcscmp(argv[3],L"--visible-game")==0;
    if(argc==4&&!visible) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::wstring path=argv[1], dir=path.substr(0,path.find_last_of(L"\\/"));
    std::wstring cmd=L"\""+path+L"\"";
    STARTUPINFOW si={}; si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=visible?SW_SHOWNORMAL:SW_HIDE;
    PROCESS_INFORMATION pi={};
    if (!CreateProcessW(path.c_str(), cmd.data(), nullptr,nullptr,FALSE,
        DEBUG_ONLY_THIS_PROCESS,nullptr,dir.c_str(),&si,&pi)) {
        printf("CreateProcess failed %lu\n",GetLastError()); return 3;
    }
    CloseHandle(pi.hThread);
    std::map<DWORD,HANDLE> threads;
    ULONGLONG start=GetTickCount64(); bool breaking=false, terminating=false, done=false;
    DWORD base=0, message_address=0; BYTE original=0; bool initial=true, armed=false;
    std::map<DWORD,unsigned> exception_counts;
    while (!done && GetTickCount64()-start<duration*1000+10000) {
        if (!breaking && GetTickCount64()-start>duration*1000) {
            breaking=true;
            printf("TIMED_SNAPSHOT DebugBreakProcess=%d\n",DebugBreakProcess(pi.hProcess));
        }
        DEBUG_EVENT e={};
        if (!WaitForDebugEvent(&e,100)) {
            continue;
        }
        DWORD status=DBG_CONTINUE;
        switch(e.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            base=reinterpret_cast<DWORD>(e.u.CreateProcessInfo.lpBaseOfImage);
            threads[e.dwThreadId]=e.u.CreateProcessInfo.hThread;
            printf("CREATE base=%p entry=%p\n",e.u.CreateProcessInfo.lpBaseOfImage,e.u.CreateProcessInfo.lpStartAddress);
            if(e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
            CloseHandle(e.u.CreateProcessInfo.hProcess);
            break;
        case CREATE_THREAD_DEBUG_EVENT:
            threads[e.dwThreadId]=e.u.CreateThread.hThread; break;
        case EXIT_THREAD_DEBUG_EVENT:
            if(threads.count(e.dwThreadId)) {CloseHandle(threads[e.dwThreadId]); threads.erase(e.dwThreadId);} break;
        case LOAD_DLL_DEBUG_EVENT: {
            wchar_t file[32768]={};
            if(e.u.LoadDll.hFile) {
                GetFinalPathNameByHandleW(e.u.LoadDll.hFile,file,32768,0);
                CloseHandle(e.u.LoadDll.hFile);
            }
            printf("DLL base=%p path=%ls\n",e.u.LoadDll.lpBaseOfDll,file); break;
        }
        case OUTPUT_DEBUG_STRING_EVENT: {
            auto& s=e.u.DebugString;
            std::vector<char> buffer(static_cast<size_t>(s.nDebugStringLength)*2+2,0); SIZE_T got=0;
            ReadProcessMemory(pi.hProcess,s.lpDebugStringData,buffer.data(),buffer.size()-2,&got);
            if(s.fUnicode) printf("DEBUG %ls\n",reinterpret_cast<wchar_t*>(buffer.data()));
            else printf("DEBUG %s\n",buffer.data());
            break;
        }
        case EXCEPTION_DEBUG_EVENT: {
            DWORD code=e.u.Exception.ExceptionRecord.ExceptionCode;
            unsigned count=++exception_counts[code];
            if(count<=3) printf("EXCEPTION code=%08lx at=%p first=%lu thread=%lu\n",code,
                e.u.Exception.ExceptionRecord.ExceptionAddress,e.u.Exception.dwFirstChance,e.dwThreadId);
            if(code==0xe06d7363 && count==1) {
                auto& record=e.u.Exception.ExceptionRecord;
                printf("CPP_EXCEPTION parameters:");
                for(DWORD j=0;j<record.NumberParameters;++j) printf(" %08lx",record.ExceptionInformation[j]);
                puts("");
                if(record.NumberParameters>=2) {
                    printf("CPP_EXCEPTION object:");
                    for(DWORD j=0;j<8;++j) printf(" %08lx",read_value<DWORD>(pi.hProcess,record.ExceptionInformation[1]+4*j));
                    puts("");
                }
                snapshot(pi.hProcess,{{e.dwThreadId,threads[e.dwThreadId]}});
            }
            if(code==EXCEPTION_BREAKPOINT) {
                DWORD address=reinterpret_cast<DWORD>(e.u.Exception.ExceptionRecord.ExceptionAddress);
                if(armed && address==message_address) {
                    CONTEXT c={}; c.ContextFlags=CONTEXT_FULL;
                    HANDLE thread=threads[e.dwThreadId];
                    if(!GetThreadContext(thread,&c)) { TerminateProcess(pi.hProcess,6); break; }
                    printf("MESSAGEBOX caption=%s text=%s flags=%08lx caller=%08lx\n",
                        read_string(pi.hProcess,read_value<DWORD>(pi.hProcess,c.Esp+12)).c_str(),
                        read_string(pi.hProcess,read_value<DWORD>(pi.hProcess,c.Esp+8)).c_str(),
                        read_value<DWORD>(pi.hProcess,c.Esp+16),read_value<DWORD>(pi.hProcess,c.Esp));
                    if(!write_byte(pi.hProcess,address,original)) { TerminateProcess(pi.hProcess,7); break; }
                    c.Eip=address; SetThreadContext(thread,&c); armed=false;
                } else if(initial) {
                    initial=false;
                    message_address=imported_function(pi.hProcess,base,"MessageBoxA");
                    if(message_address) {
                        original=read_value<BYTE>(pi.hProcess,message_address);
                        armed=write_byte(pi.hProcess,message_address,0xcc);
                        printf("MESSAGEBOX_BREAKPOINT address=%08lx armed=%d\n",message_address,armed);
                    }
                } else if(breaking && !terminating) {
                    snapshot(pi.hProcess,threads); terminating=true;
                    TerminateProcess(pi.hProcess,0); // Only the process created by this tool.
                }
            } else status=DBG_EXCEPTION_NOT_HANDLED;
            break;
        }
        case EXIT_PROCESS_DEBUG_EVENT:
            printf("EXIT %lu\n",e.u.ExitProcess.dwExitCode); done=true; break;
        }
        ContinueDebugEvent(e.dwProcessId,e.dwThreadId,status);
    }
    if(!done) TerminateProcess(pi.hProcess,1);
    for(const auto& count:exception_counts) printf("EXCEPTION_COUNT %08lx %u\n",count.first,count.second);
    for(auto& t:threads) CloseHandle(t.second);
    CloseHandle(pi.hProcess);
    return done ? 0 : 4;
}
