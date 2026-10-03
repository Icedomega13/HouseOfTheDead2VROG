@echo off
setlocal
set "PROJECT_ROOT=%~dp0.."
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
if /I not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo An x86 Visual Studio developer environment is required.
  exit /b 1
)
if not defined HOTD2_BUILD_DIR set "HOTD2_BUILD_DIR=%PROJECT_ROOT%\build\pcvr"
if not exist "%HOTD2_BUILD_DIR%" mkdir "%HOTD2_BUILD_DIR%"
pushd "%HOTD2_BUILD_DIR%"
set "XR_SDK=%PROJECT_ROOT%\working\pcvr\deps\openxr-1.1.63"
if not exist "%XR_SDK%\include\openxr\openxr.h" (
  echo OpenXR 1.1.63 dependency is required. See pcvr README.
  goto fail
)
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT /I"%XR_SDK%\include" /LD "%PROJECT_ROOT%\pcvr\native\ddraw_probe.cpp" "%PROJECT_ROOT%\pcvr\native\xr_bridge.cpp" /link /DEF:"%PROJECT_ROOT%\pcvr\native\ddraw_probe.def" /OUT:ddraw.dll dxguid.lib d3d11.lib dxgi.lib user32.lib ole32.lib windowscodecs.lib bcrypt.lib delayimp.lib /LIBPATH:"%XR_SDK%\Win32\lib" openxr_loader.lib /DELAYLOAD:openxr_loader.dll
if errorlevel 1 goto fail
cl /nologo /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\probe_smoke.cpp" /Fe:probe_smoke.exe /link dxguid.lib
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\render_state_tests.cpp" /Fe:render_state_tests.exe
if errorlevel 1 goto fail
cl /nologo /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\startup_trace.cpp" /Fe:startup_trace.exe /link dbghelp.lib
if errorlevel 1 goto fail
set "XR_SDK=%PROJECT_ROOT%\working\pcvr\deps\openxr-1.1.63"
if exist "%XR_SDK%\include\openxr\openxr.h" (
  cl /nologo /W4 /WX /EHsc /std:c++17 /MT /I"%XR_SDK%\include" "%PROJECT_ROOT%\pcvr\native\xr_probe.cpp" /Fe:xr_probe.exe /link /LIBPATH:"%XR_SDK%\Win32\lib" openxr_loader.lib
  if errorlevel 1 goto fail
  cl /nologo /W4 /WX /EHsc /std:c++17 /MT /I"%XR_SDK%\include" "%PROJECT_ROOT%\pcvr\native\xr_math_tests.cpp" /Fe:xr_math_tests.exe
  if errorlevel 1 goto fail
  cl /nologo /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\input_bridge_tests.cpp" /Fe:input_bridge_tests.exe /link user32.lib dinput8.lib dxguid.lib
  if errorlevel 1 goto fail
  cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT /I"%XR_SDK%\include" "%PROJECT_ROOT%\pcvr\native\xr_bridge_tests.cpp" /Fe:xr_bridge_tests.exe /link d3d11.lib dxgi.lib /LIBPATH:"%XR_SDK%\Win32\lib" openxr_loader.lib
  if errorlevel 1 goto fail
  cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\xr_pixels_tests.cpp" /Fe:xr_pixels_tests.exe
  if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\reload_gesture_tests.cpp" /Fe:reload_gesture_tests.exe
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\native_ammo_tests.cpp" /Fe:native_ammo_tests.exe /link bcrypt.lib
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\texture_image_tests.cpp" /Fe:texture_image_tests.exe /link ole32.lib windowscodecs.lib bcrypt.lib
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\pistol_mesh_tests.cpp" /Fe:pistol_mesh_tests.exe
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\hud_prompt_tests.cpp" /Fe:hud_prompt_tests.exe
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\hud_cache_tests.cpp" /Fe:hud_cache_tests.exe /link ole32.lib windowscodecs.lib bcrypt.lib
if errorlevel 1 goto fail
cl /nologo /O2 /W4 /WX /EHsc /std:c++17 /MT "%PROJECT_ROOT%\pcvr\native\gun_render_tests.cpp" /Fe:gun_render_tests.exe /link dxguid.lib user32.lib
if errorlevel 1 goto fail
  copy /y "%XR_SDK%\Win32\bin\openxr_loader.dll" . >nul
)
popd
exit /b 0
:fail
popd
exit /b 1
