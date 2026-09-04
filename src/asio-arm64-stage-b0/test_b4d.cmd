@echo off
setlocal
cd /d "%~dp0"
echo Sound Blaster X4 ARM64EC ASIO - pre-REAPER smoke
echo Close all apps using X4 Windows playback first.
echo A BUSY result is a safe refusal, not a failure of the driver.
echo.
x4-asio-stage-b4d-smoke.exe
set rc=%errorlevel%
echo.
if "%rc%"=="0" (
  echo B4D smoke returned success.
) else (
  echo B4D smoke returned exit code %rc%.
)
pause
exit /b %rc%
