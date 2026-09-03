# Sound Blaster X4 — Windows SDK ARM64 ABI Baseline A0

This is the ABI reset point for the native ARM64 ASIO work.

## Why this exists

Earlier A0/A1 prototypes manually reconstructed Win32/KS/WaveRT ABI types and function declarations. A0 completed one hardware lifecycle successfully, but repeated lifecycles later exposed a repeatable `WDF_VIOLATION 0x10D / subtype 0x5`.

This baseline removes that uncertainty before any more lifecycle testing.

## Hard rules in this baseline

- Native **ARM64**, not ARM64EC.
- Build only with **MSVC + Microsoft Windows SDK**.
- No hand-written replacements for `HANDLE`, `GUID`, `SP_DEVICE_INTERFACE_DATA`, `KSPROPERTY`, `KSPIN_CONNECT`, `KSDATAFORMAT`, `WAVEFORMATEXTENSIBLE`, `KSRTAUDIO_*`, or `KSAUDIO_PRESENTATION_POSITION`.
- SetupAPI/KS function prototypes come from Microsoft headers.
- `SP_DEVICE_INTERFACE_DETAIL_DATA_W::cbSize` is `sizeof(...)`, never a hard-coded 8.
- `KSPIN_CONNECT` is followed by the SDK `KSDATAFORMAT_WAVEFORMATEXTENSIBLE`; a compile-time assertion verifies there is no aggregate padding between them.
- Runtime logs `sizeof`, `alignof`, and critical `offsetof` values from the actual ARM64 SDK compilation.
- Exactly **one** A0 lifecycle. No repeated reopen test in this build.
- Render Pin 1 / 48 kHz / stereo / 16-bit / 4096 bytes / NotificationCount=2.
- 20 notifications, observe-only while RUN is active.
- If `KSRTAUDIO_BUFFER::CallMemoryBarrier` is true, `MemoryBarrier()` is issued after the pre-RUN buffer initialization.
- Creative DLLs are not loaded or redistributed.

## Build

From a Visual Studio 2022 Developer Command Prompt with Desktop development with C++ and the Windows SDK installed:

```bat
BUILD-MSVC-ARM64.cmd
```

Equivalent commands:

```bat
cmake -S src -B build -G "Visual Studio 17 2022" -A ARM64
cmake --build build --config Release --parallel
```

## Run

Close Creative App, DAWs, browsers playing audio, and other audio clients, then run:

```bat
RUN-ON-X4.cmd
```

Expected log:

`x4-asio-sdk-abi-baseline.txt`

The first section is the SDK ABI layout. The second section performs the single A0 hardware lifecycle.

## PASS condition

- process/native architecture reports ARM64
- X4 `msft_wave` opens
- Render Pin 1 creates
- 4096-byte WaveRT notification buffer allocates
- notification event registers
- ACQUIRE -> PAUSE -> RUN succeeds
- 20 notifications arrive
- packet discontinuities = 0
- position regressions = 0
- RUN -> PAUSE -> ACQUIRE -> STOP succeeds
- event unregister and handles close
- final line: `SDK ABI BASELINE RESULT: PASS`

Do **not** run a repeated-lifecycle experiment until this SDK-native baseline passes on hardware.
