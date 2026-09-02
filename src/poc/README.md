# Windows ARM64 Direct Mode PoC

Minimal native Windows C++/WinRT proof-of-concept for controlling **Sound Blaster X4 (`SB1815`)** over BLE without Creative App.

## Confirmed protocol used by this PoC

- BLE device name: `Control for SB1815`
- Service UUID: `b7860001-11b8-b681-6343-5a6c2286633f`
- Write characteristic UUID: `b7860002-11b8-b681-6343-5a6c2286633f`
- Direct Mode OFF: `5A3903000500`
- Direct Mode ON: `5A3903000501`

Both Direct Mode commands were reproduced on physical X4 hardware before this PoC was written.

## Build

Requirements:

- Windows 11
- Visual Studio 2022 with Desktop development with C++
- Windows 10/11 SDK containing C++/WinRT headers
- ARM64 C++ build tools

```powershell
cmake -S src/poc -B build/poc -G "Visual Studio 17 2022" -A ARM64
cmake --build build/poc --config Release
```

Output:

`build/poc/Release/x4-poc.exe`

The repository also contains a manual-only GitHub Actions workflow that cross-builds ARM64 and verifies the PE machine field is `0xAA64`.

## Run

Make sure Windows Bluetooth is enabled and the BLE control endpoint `Control for SB1815` is visible to Windows. If Windows asks to pair/allow the device, complete that once before running the PoC.

```powershell
.\x4-poc.exe off
.\x4-poc.exe on
```

Successful execution prints the discovered device, the exact six-byte command, and the GATT write result.

## Scope

This PoC intentionally does only one thing: independently reproduce Direct Mode OFF/ON from native Windows ARM64 code. It does not implement the full Creative App UI, DSP controls, mixer, EQ, notifications, or persistent device management yet.
