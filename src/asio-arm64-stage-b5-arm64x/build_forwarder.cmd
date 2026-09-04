@echo off
setlocal EnableExtensions

if "%~1"=="" (
  echo Usage: build_forwarder.cmd ^<output-dir^>
  exit /b 2
)

where cl.exe >nul 2>nul || (
  echo cl.exe not found. Run this from an ARM64 Visual Studio Developer Command Prompt.
  exit /b 3
)
where link.exe >nul 2>nul || (
  echo link.exe not found. Run this from an ARM64 Visual Studio Developer Command Prompt.
  exit /b 4
)
where lib.exe >nul 2>nul || (
  echo lib.exe not found. Run this from an ARM64 Visual Studio Developer Command Prompt.
  exit /b 5
)

set "SRC=%~dp0"
set "OUT=%~f1"
if not exist "%OUT%" mkdir "%OUT%" || exit /b 6

set "EMPTY_ARM64=%OUT%\empty_arm64.obj"
set "EMPTY_X64=%OUT%\empty_x64.obj"
set "LIB_ARM64=%OUT%\x4_arm64_forward.lib"
set "LIB_X64=%OUT%\x4_x64_forward.lib"
set "DLL_OUT=%OUT%\x4-asio-arm64x-b5.dll"

cl /nologo /c /Fo"%EMPTY_ARM64%" "%SRC%empty.cpp" || exit /b 10
cl /nologo /c /arm64EC /Fo"%EMPTY_X64%" "%SRC%empty.cpp" || exit /b 11

rem Modern VS toolsets expose import-library creation through LIB.EXE.
rem This is equivalent to the ARM64X pure-forwarder documentation's
rem historical "link /lib" form, but avoids LINK treating /lib as unknown.
lib /nologo /machine:arm64 /def:"%SRC%forward_arm64.def" /out:"%LIB_ARM64%" || exit /b 12
lib /nologo /machine:x64 /def:"%SRC%forward_arm64ec.def" /out:"%LIB_X64%" || exit /b 13

if not exist "%LIB_ARM64%" (
  echo ARM64 forward import library was not produced: %LIB_ARM64%
  exit /b 12
)
if not exist "%LIB_X64%" (
  echo x64/ARM64EC forward import library was not produced: %LIB_X64%
  exit /b 13
)

link /nologo /dll /noentry /machine:arm64x ^
  /defArm64Native:"%SRC%forward_arm64.def" ^
  /def:"%SRC%forward_arm64ec.def" ^
  "%EMPTY_ARM64%" "%EMPTY_X64%" ^
  "%LIB_ARM64%" "%LIB_X64%" ^
  /out:"%DLL_OUT%" || exit /b 14

link /dump /headers "%DLL_OUT%" > "%OUT%\ARM64X_HEADERS.txt" || exit /b 15
link /dump /exports "%DLL_OUT%" > "%OUT%\ARM64X_EXPORTS.txt" || exit /b 16

findstr /I /C:"ARM64X" "%OUT%\ARM64X_HEADERS.txt" >nul || (
  echo ARM64X marker not found in linker header dump.
  type "%OUT%\ARM64X_HEADERS.txt"
  exit /b 17
)

findstr /I /C:"DllGetClassObject" "%OUT%\ARM64X_EXPORTS.txt" >nul || exit /b 18
findstr /I /C:"DllCanUnloadNow" "%OUT%\ARM64X_EXPORTS.txt" >nul || exit /b 19

echo ARM64X pure forwarder built: %DLL_OUT%
exit /b 0
