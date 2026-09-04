@echo off
setlocal
cd /d "%~dp0"
echo Unregistering Sound Blaster X4 ARM64 ASIO B5 only...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b5-register.exe' -ArgumentList '--unregister' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  echo B5 UNREGISTER FAILED.
  pause
  exit /b 1
)
echo B5 removed. Existing B4D registration was not touched.
pause
exit /b 0
