@echo off
setlocal
cd /d "%~dp0"

echo Installing Sound Blaster X4 ARM64 ASIO B5 ARM64X bridge...
echo This only updates the B5 COM/ASIO registration. It does not open WaveRT pins.
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b5-arm64x-register.exe' -ArgumentList '--register' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  echo.
  echo B5 ARM64X REGISTER FAILED.
  pause
  exit /b 1
)

echo.
echo B5 ARM64X REGISTER PASS.
echo InprocServer32 now points to x4-asio-arm64x-b5.dll.
pause
exit /b 0
