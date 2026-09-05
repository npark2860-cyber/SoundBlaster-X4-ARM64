@echo off
setlocal
cd /d "%~dp0"
echo X4 APO ARM64 Stage A0 offline COM probe
echo.
X4ApoComProbeArm64.exe X4ApoArm64.dll
echo.
pause
