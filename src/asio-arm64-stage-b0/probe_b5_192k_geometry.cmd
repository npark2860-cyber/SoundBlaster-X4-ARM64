@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set REPORT=%~dp0B5_192K_GEOMETRY_REPORT.txt

> "%REPORT%" echo Sound Blaster X4 ARM64 ASIO B5 192 kHz WaveRT geometry probe
>>"%REPORT%" echo Generated: %date% %time%
>>"%REPORT%" echo.

echo Running safe 192 kHz WaveRT geometry scan...
echo Close REAPER, media players, Creative App playback, and other X4 users first.
echo The probe never enters KSSTATE_RUN and checks Render Pin 1 FREE before every KsCreatePin.
echo.

x4-asio-stage-b5-192k-geometry-probe.exe >>"%REPORT%" 2>&1
set PROBEERR=%ERRORLEVEL%

>>"%REPORT%" echo.
>>"%REPORT%" echo B5 192K GEOMETRY PROBE EXIT=%PROBEERR%

if "%PROBEERR%"=="0" (
  echo PASS. Geometry report:
  echo   %REPORT%
) else if "%PROBEERR%"=="10" (
  echo BUSY/INDETERMINATE. No BUSY bypass was attempted.
  echo Report:
  echo   %REPORT%
) else (
  echo Probe stopped with code %PROBEERR%.
  echo Report:
  echo   %REPORT%
)

pause
exit /b %PROBEERR%
