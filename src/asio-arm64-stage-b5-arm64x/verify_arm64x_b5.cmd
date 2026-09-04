@echo off
setlocal
cd /d "%~dp0"

echo Verifying B5 ARM64X registration and both loader routes...
echo.

x4-asio-stage-b5-arm64x-register.exe --verify || goto :fail
x4-asio-stage-b5-arm64x-probe-arm64ec.exe || goto :fail
x4-asio-stage-b5-arm64x-probe-arm64.exe || goto :fail

echo.
echo B5 ARM64X DUAL-ARCH VERIFY: PASS
pause
exit /b 0

:fail
echo.
echo B5 ARM64X DUAL-ARCH VERIFY: FAIL
pause
exit /b 1
