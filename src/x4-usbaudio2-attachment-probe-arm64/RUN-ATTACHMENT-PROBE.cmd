@echo off
setlocal
cd /d "%~dp0"
echo Sound Blaster X4 ARM64 usbaudio2 attachment probe
echo READ-ONLY - no registry writes, no CTCDC writes, no driver install.
echo.
x4-usbaudio2-attachment-probe.exe > X4_USBAUDIO2_ATTACHMENT_REPORT.txt 2>&1
type X4_USBAUDIO2_ATTACHMENT_REPORT.txt
echo.
echo Report: %CD%\X4_USBAUDIO2_ATTACHMENT_REPORT.txt
pause
