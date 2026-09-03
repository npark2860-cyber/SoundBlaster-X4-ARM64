@echo off
setlocal
cd /d "%~dp0"

set EXE=x4-asio-sdk-abi-baseline.exe
if not exist "%EXE%" (
  if exist "build\Release\%EXE%" set EXE=build\Release\%EXE%
)

if not exist "%EXE%" (
  echo ERROR: x4-asio-sdk-abi-baseline.exe was not found.
  echo Build it first with BUILD-MSVC-ARM64.cmd or use the GitHub Actions artifact.
  pause
  exit /b 1
)

echo Close Creative App, DAWs, browsers playing audio, and other audio clients before running.
echo This performs ONE hardware-confirmed A0 lifecycle only. It does NOT repeat/reopen the WaveRT pin.
echo.
"%EXE%"
echo.
echo Log: x4-asio-sdk-abi-baseline.txt
pause
endlocal
