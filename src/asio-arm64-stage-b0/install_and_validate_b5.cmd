@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set REPORT=%~dp0B5_PRODUCT_VALIDATION_REPORT.txt

> "%REPORT%" echo Sound Blaster X4 ARM64 ASIO B5 product validation
>>"%REPORT%" echo Generated: %date% %time%
>>"%REPORT%" echo.

echo Registering B5 side-by-side ASIO driver...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = Start-Process -FilePath '%~dp0x4-asio-stage-b5-register.exe' -ArgumentList '--register' -Verb RunAs -PassThru -Wait; exit $p.ExitCode"
if errorlevel 1 (
  >>"%REPORT%" echo B5 REGISTER: FAIL
  echo REGISTER FAILED. Existing B4D registration was not touched.
  goto :fail
)
>>"%REPORT%" echo B5 REGISTER: PASS
x4-asio-stage-b5-register.exe --verify >>"%REPORT%" 2>&1
if errorlevel 1 goto :fail

echo Checking immutable Render Pin 1 idle gate...
x4-asio-stage-b5-ks-probe.exe --require-idle >>"%REPORT%" 2>&1
set KSERR=%ERRORLEVEL%
if not "%KSERR%"=="0" (
  >>"%REPORT%" echo.
  >>"%REPORT%" echo B5 PRODUCT VALIDATION RESULT: BUSY_OR_INDETERMINATE_AT_INITIAL_GATE code=%KSERR%
  echo X4 is BUSY or indeterminate. No lifecycle test was started.
  goto :busy_initial
)

rem Important: consume the proven FREE window immediately. The public capability
rem probe does not create pins, but an external Windows/audio client can acquire
rem Render Pin 1 between separate processes. Run the actual lifecycle matrix first.
echo Running bundled 24-bit/rate/buffer/full-duplex lifecycle matrix...
x4-asio-stage-b5-product-validation.exe >>"%REPORT%" 2>&1
set VALERR=%ERRORLEVEL%
if "%VALERR%"=="10" (
  >>"%REPORT%" echo.
  >>"%REPORT%" echo B5 PRODUCT VALIDATION RESULT: BUSY_BLOCKED_DURING_MATRIX
  >>"%REPORT%" echo Post-block property-only KS snapshot follows; no pin is created by this probe.
  x4-asio-stage-b5-ks-probe.exe >>"%REPORT%" 2>&1
  goto :busy_runtime
)
if not "%VALERR%"=="0" goto :fail

echo Capturing B5 public ASIO contract after lifecycle PASS...
x4-asio-stage-b5-capability-probe.exe --match "Sound Blaster X4 ARM64 ASIO B5" >>"%REPORT%" 2>&1
set CAPERR=%ERRORLEVEL%
if not "%CAPERR%"=="0" (
  >>"%REPORT%" echo.
  >>"%REPORT%" echo B5 POST-MATRIX CAPABILITY REPORT: UNAVAILABLE code=%CAPERR%
  >>"%REPORT%" echo Lifecycle matrix already passed; this post-report failure does not reopen or bypass BUSY.
)

>>"%REPORT%" echo.
>>"%REPORT%" echo B5 INSTALL + PRODUCT VALIDATION: PASS
echo.
echo PASS. Report:
echo   %REPORT%
echo.
echo B4D remains installed side-by-side. In REAPER select:
echo   Sound Blaster X4 ARM64 ASIO B5
pause
exit /b 0

:busy_initial
echo.
echo No unsafe pin open was attempted. Close X4 playback/change default output before one later validation attempt.
echo Report: %REPORT%
pause
exit /b %KSERR%

:busy_runtime
echo.
echo Render Pin 1 became busy after the initial FREE gate. No BUSY bypass was attempted.
echo If this repeats after X4 is not the Windows default output and playback apps are closed, use ownership diagnostics instead of repeated retries.
echo Report: %REPORT%
pause
exit /b 10

:fail
>>"%REPORT%" echo.
>>"%REPORT%" echo B5 INSTALL + PRODUCT VALIDATION: FAIL
echo.
echo B5 validation failed. Do not remove B4D; it remains the proven fallback.
echo Report: %REPORT%
pause
exit /b 1
