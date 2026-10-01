@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
if /I not "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 1
if not defined HOTD2_REPLAY_DIR set "HOTD2_REPLAY_DIR=%PROJECT_ROOT%\build\pcvr\replay"
if not exist "%HOTD2_REPLAY_DIR%" mkdir "%HOTD2_REPLAY_DIR%"
pushd "%HOTD2_REPLAY_DIR%"
set "XR_SDK=%PROJECT_ROOT%\working\pcvr\deps\openxr-1.1.63"
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT /DHOTD2_CONTROLLER_REPLAY_TEST /I"%XR_SDK%\include" /LD "%PROJECT_ROOT%\pcvr\native\ddraw_probe.cpp" "%PROJECT_ROOT%\pcvr\native\xr_bridge.cpp" /link /DEF:"%PROJECT_ROOT%\pcvr\native\ddraw_probe.def" /OUT:ddraw.dll dxguid.lib d3d11.lib dxgi.lib user32.lib ole32.lib windowscodecs.lib bcrypt.lib /LIBPATH:"%XR_SDK%\Win32\lib" openxr_loader.lib
set "BUILD_RESULT=%ERRORLEVEL%"
popd
exit /b %BUILD_RESULT%
