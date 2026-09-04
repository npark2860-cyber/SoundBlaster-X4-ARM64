@echo off
setlocal
cd /d "%~dp0"

set "REPORT=B5_CAPABILITY_REPORT.txt"
> "%REPORT%" echo Sound Blaster X4 B5 capability/reference report
>>"%REPORT%" echo Generated: %DATE% %TIME%
>>"%REPORT%" echo.

echo [1/5] Checking X4 idle gate and dumping KS/WaveRT ranges...
x4-asio-stage-b5-ks-probe.exe --require-idle >>"%REPORT%" 2>&1
if errorlevel 1 goto :blocked

echo [2/5] Enumerating registered ASIO drivers...
x4-asio-stage-b5-capability-probe.exe --list >>"%REPORT%" 2>&1
if errorlevel 1 goto :failed

echo [3/5] Probing Creative SB USB RT ASIO first...
x4-asio-stage-b5-capability-probe.exe --match "SB USB RT ASIO" --lifecycle 3 >>"%REPORT%" 2>&1
if errorlevel 5 if not errorlevel 6 (
  >>"%REPORT%" echo Creative RT name did not resolve uniquely; trying SB USB ASIO fallback.
  x4-asio-stage-b5-capability-probe.exe --match "SB USB ASIO" --lifecycle 3 >>"%REPORT%" 2>&1
)
if errorlevel 1 goto :failed

timeout /t 2 /nobreak >nul

echo [4/5] Re-checking idle gate after Creative release...
x4-asio-stage-b5-ks-probe.exe --require-idle >>"%REPORT%" 2>&1
if errorlevel 1 goto :blocked

echo [5/5] Probing the validated independent driver...
x4-asio-stage-b5-capability-probe.exe --match "Sound Blaster X4 ARM64" --lifecycle 3 >>"%REPORT%" 2>&1
if errorlevel 1 goto :failed

>>"%REPORT%" echo.
>>"%REPORT%" echo B5 COMBINED CAPABILITY PROBE RESULT: PASS
echo.
echo PASS - %REPORT%
exit /b 0

:blocked
>>"%REPORT%" echo.
>>"%REPORT%" echo B5 COMBINED CAPABILITY PROBE RESULT: BLOCKED BY IDLE/BUSY GATE
echo.
echo BLOCKED - close active X4 playback and keep BUSY protection enabled.
echo Report: %REPORT%
exit /b 10

:failed
>>"%REPORT%" echo.
>>"%REPORT%" echo B5 COMBINED CAPABILITY PROBE RESULT: FAIL
echo.
echo FAIL - see %REPORT%
exit /b 1
