@echo off
setlocal
cd /d "%~dp0"

set "REPORT=B5_192K_CADENCE_REPORT.txt"

echo Sound Blaster X4 ASIO B5 192 kHz cadence probe
echo Close REAPER and all other X4 playback/capture before continuing.
echo This probe keeps the existing strict packet-discontinuity checks enabled.
echo.

if not exist "x4-asio-stage-b5-product-validation.exe" (
  echo ERROR: x4-asio-stage-b5-product-validation.exe not found.
  exit /b 2
)

if exist "%REPORT%" del /q "%REPORT%" >nul 2>nul

"x4-asio-stage-b5-product-validation.exe" --cadence-192 > "%REPORT%" 2>&1
set "CODE=%ERRORLEVEL%"

type "%REPORT%"
echo.
echo Report: %CD%\%REPORT%
echo Exit code: %CODE%

exit /b %CODE%
