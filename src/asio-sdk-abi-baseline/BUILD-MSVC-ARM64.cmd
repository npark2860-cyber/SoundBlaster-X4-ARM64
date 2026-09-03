@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: cmake.exe was not found.
  echo Run this from a Visual Studio 2022 Developer Command Prompt with Desktop development with C++ and the Windows SDK installed.
  exit /b 1
)

if exist build rmdir /s /q build
cmake -S src -B build -G "Visual Studio 17 2022" -A ARM64
if errorlevel 1 exit /b %errorlevel%

cmake --build build --config Release --parallel
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built:
echo   %CD%\build\Release\x4-asio-sdk-abi-baseline.exe
endlocal
