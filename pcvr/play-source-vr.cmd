@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-probe.ps1" -VR -TexturePack
if errorlevel 1 pause
