@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo Verifying B5 ARM64X registration and both loader routes...
echo The route probes are copied away from the driver package so this also
 echo verifies that a real host does not depend on its EXE directory to find the backend DLL.
echo.

x4-asio-stage-b5-arm64x-register.exe --verify || goto :fail

set "PROBE_DIR=%TEMP%\X4_B5_ARM64X_PROBE"
if exist "%PROBE_DIR%" rmdir /s /q "%PROBE_DIR%"
mkdir "%PROBE_DIR%" || goto :fail
copy /y "x4-asio-stage-b5-arm64x-probe-arm64ec.exe" "%PROBE_DIR%\" >nul || goto :fail
copy /y "x4-asio-stage-b5-arm64x-probe-arm64.exe" "%PROBE_DIR%\" >nul || goto :fail

pushd "%PROBE_DIR%" || goto :fail
"%PROBE_DIR%\x4-asio-stage-b5-arm64x-probe-arm64ec.exe" "%~dp0x4-asio-arm64x-b5.dll"
if errorlevel 1 (
  popd
  goto :fail
)
"%PROBE_DIR%\x4-asio-stage-b5-arm64x-probe-arm64.exe" "%~dp0x4-asio-arm64x-b5.dll"
if errorlevel 1 (
  popd
  goto :fail
)
popd

rmdir /s /q "%PROBE_DIR%" >nul 2>nul

echo.
echo B5 ARM64X DUAL-ARCH VERIFY: PASS
pause
exit /b 0

:fail
if defined PROBE_DIR rmdir /s /q "%PROBE_DIR%" >nul 2>nul
echo.
echo B5 ARM64X DUAL-ARCH VERIFY: FAIL
pause
exit /b 1
