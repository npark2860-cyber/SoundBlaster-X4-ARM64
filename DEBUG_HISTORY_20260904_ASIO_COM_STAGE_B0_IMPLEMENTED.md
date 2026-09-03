# DEBUG HISTORY — ASIO COM Stage B0 implemented

Date: 2026-09-04 KST

## Purpose

Resume ASIO productization only after the X4 render coexistence gate was hardware-confirmed safe.

Stage B0 intentionally validates only the Windows ARM64 ASIO COM ABI shell. It does not connect the WaveRT streaming engine yet.

## Preconditions now satisfied

Hardware-confirmed coexistence result:

- idle X4 Render Pin 1 `GLOBALCINSTANCES`: `0 / 1`
- active Windows X4 playback: `1 / 1`
- native gate detects active playback as BUSY
- `KsCreatePin` is skipped while BUSY
- process exits normally
- no green screen

See:

- `DEBUG_HISTORY_20260904_ASIO_CREATIVE_EXCLUSIVE_PREFLIGHT_STATIC.md`
- `DEBUG_HISTORY_20260904_ASIO_GLOBAL_INSTANCE_GATE_ACTIVE_SUCCESS.md`

Therefore the previous coexistence blocker no longer prevents starting ASIO COM Stage B.

## Stage B0 branch

Branch:

`exp/windows-arm64-asio-com-stage-b0`

Verified implementation HEAD:

`d766bc70cbb3d9abfc9ac265ee577b5185354103`

All Stage B0 implementation files are isolated under:

`src/asio-arm64-stage-b0/`

## Independent COM identity

Stage B0 uses a new project-owned CLSID:

`{0AA6D99C-4AF6-45EF-9CCA-10AC9239B7D4}`

Creative's ASIO CLSID is not reused.

Reference-only Creative x64 static evidence had shown a normal in-process COM server with:

- `DllCanUnloadNow`
- `DllGetClassObject`
- `DllRegisterServer`
- `DllUnregisterServer`
- `InprocServer32`
- `ThreadingModel = Apartment`
- standard `Software\\ASIO` registration

The final ARM64 code remains independent and does not load Creative binaries.

## Stage B0 implementation

The native ARM64 DLL provides:

- `IClassFactory`
- ASIO-compatible interface vtable derived from `IUnknown`
- `DllCanUnloadNow`
- `DllGetClassObject`
- `DllRegisterServer`
- `DllUnregisterServer`

The driver object accepts both:

- `IID_IUnknown`
- its own ASIO CLSID as the requested interface GUID

This matches the Windows ASIO host loading model in which the registry CLSID is also used as the requested ASIO interface ID during `CoCreateInstance`.

## Deliberately narrow capabilities

Stage B0 reports only the already-proven fixed render geometry:

- inputs: 0
- outputs: 2
- sample rate: 48,000 Hz only
- buffer size: fixed 512 frames

Streaming is deliberately disabled:

- `start()` -> `ASE_InvalidMode`
- `createBuffers()` -> `ASE_InvalidMode`

No KS or WaveRT operation occurs in Stage B0.

## Registry-free smoke harness

`x4-asio-stage-b0-smoke.exe` validates the COM ABI without registration or hardware access:

1. `LoadLibraryW(x4-asio-arm64.dll)`
2. resolve `DllGetClassObject`
3. obtain `IClassFactory`
4. `CreateInstance` using the project's ASIO CLSID as requested interface ID
5. call `init`
6. call metadata/capability methods
7. confirm `start()` remains disabled
8. release objects
9. verify `DllCanUnloadNow == S_OK`

The smoke executable does not:

- write registry keys
- open X4 `msft_wave`
- call `KsCreatePin`
- request a WaveRT buffer
- perform hardware I/O

## Build workflow

Main now contains manual-only workflow:

`Build ASIO COM Stage B0 ARM64`

It checks out the Stage B0 branch, cross-builds DLL + smoke EXE for native ARM64, verifies PE machine `0xAA64`, packages both binaries with the README, and uploads the ZIP artifact.

No automatic push/pull-request trigger was added.

## Immediate hardware-machine validation

First validation is not a hardware streaming test.

Run only:

`x4-asio-stage-b0-smoke.exe`

with `x4-asio-arm64.dll` in the same folder.

Expected final line:

`STAGE B0 COM SMOKE RESULT: PASS`

Do not register the DLL and do not connect the WaveRT engine until this registry-free COM smoke passes on the Windows ARM64 machine.
