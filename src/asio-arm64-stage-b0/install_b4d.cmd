@echo off
setlocal
cd /d "%~dp0"
echo Sound Blaster X4 ARM64EC ASIO - register for REAPER
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b4d-register.exe' -ArgumentList '--register' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  echo.
  echo REGISTER FAILED. No REAPER test should be attempted yet.
  pause
  exit /b 1
)
echo.
echo Registration verified. Probing normal COM registry load from ARM64EC host...
x4-asio-stage-b4d-host-probe.exe
if errorlevel 1 (
  echo.
  echo HOST PROBE FAILED. Run uninstall_b4d.cmd and do not open REAPER yet.
  pause
  exit /b 2
)
echo.
echo REGISTER + HOST PROBE PASS. Open REAPER ARM64EC and select:
echo   Audio system: ASIO
echo   ASIO Driver: Sound Blaster X4 ARM64 ASIO
echo.
echo Frozen first test: 48000 Hz, stereo output, 512 frames.
pause
exit /b 0
