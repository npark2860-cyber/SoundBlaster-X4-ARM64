@echo off
setlocal
cd /d "%~dp0"
chcp 65001 >nul
echo Sound Blaster X4 ARM64 MMDevice endpoint association probe
echo.
x4-mmdevice-endpoint-probe.exe > X4_MMDEVICE_ENDPOINT_REPORT.txt 2>&1
type X4_MMDEVICE_ENDPOINT_REPORT.txt
echo.
echo Report: %CD%\X4_MMDEVICE_ENDPOINT_REPORT.txt
pause
