@echo off
setlocal
cd /d "%~dp0"
echo Sound Blaster X4 ARM64EC ASIO - unregister
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b4d-register.exe' -ArgumentList '--unregister' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  echo.
  echo UNREGISTER FAILED.
  pause
  exit /b 1
)
echo.
echo UNREGISTER PASS.
pause
exit /b 0
