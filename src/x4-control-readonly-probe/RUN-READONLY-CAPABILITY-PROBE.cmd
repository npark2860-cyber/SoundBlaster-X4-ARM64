@echo off
cd /d "%~dp0"
x4-control-readonly-probe.exe
echo.
echo Report: X4_READONLY_CAPABILITY_REPORT.txt
pause
