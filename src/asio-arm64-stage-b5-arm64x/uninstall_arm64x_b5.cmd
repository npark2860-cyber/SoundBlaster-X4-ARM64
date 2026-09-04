@echo off
setlocal
cd /d "%~dp0"

echo Unregistering Sound Blaster X4 ARM64 ASIO B5 ARM64X bridge...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b5-arm64x-register.exe' -ArgumentList '--unregister' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  echo.
  echo B5 ARM64X UNREGISTER FAILED.
  pause
  exit /b 1
)

echo.
echo B5 ARM64X UNREGISTER PASS.
pause
exit /b 0
