@echo off
rem Reuse an x86 Developer Prompt, otherwise discover installed C++ build tools.
rem No setlocal: vcvars32 must configure the calling build script's environment.
if defined VSCMD_VER goto check_arch
set "HOTD2_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%HOTD2_VSWHERE%" (
  echo Install Visual Studio C++ Build Tools with the Windows SDK, or use an x86 Developer Prompt.
  exit /b 1
)
set "HOTD2_VS_INSTALL="
for /f "usebackq delims=" %%I in (`"%HOTD2_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "HOTD2_VS_INSTALL=%%I"
if not defined HOTD2_VS_INSTALL (
  echo No installation with MSVC x86 C++ tools was found.
  exit /b 1
)
call "%HOTD2_VS_INSTALL%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b 1
:check_arch
if /I not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo Use an x86 Visual Studio Developer Prompt; the original game is 32-bit.
  exit /b 1
)
exit /b 0
