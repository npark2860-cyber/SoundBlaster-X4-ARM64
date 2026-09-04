# Stage B4D ARM64EC Registration + Host Probe Runtime Success

Date: 2026-09-04 KST

## Context

Stage B4D combines the validated B4C ASIO2 transport with ARM64EC host compatibility, registration, and the first normal COM registry-load probe intended to mirror the loading model used by REAPER ARM64EC.

Validated B4D branch/source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

## Runtime result

The user ran the packaged registration flow and obtained:

```text
Sound Blaster X4 ARM64EC ASIO - register for REAPER

Registration verified. Probing normal COM registry load from ARM64EC host...
Sound Blaster X4 ASIO Stage B4D ARM64EC registered-host probe
CoInitializeEx hr=0x00000000
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)

REGISTER + HOST PROBE PASS. Open REAPER ARM64EC and select:
  Audio system: ASIO
  ASIO Driver: Sound Blaster X4 ARM64 ASIO

Frozen first test: 48000 Hz, stereo output, 512 frames.
```

## Hardware/runtime-proven facts

- B4D registration helper completed and verified the expected ASIO/COM registration.
- An ARM64EC host process successfully initialized COM.
- `CoCreateInstance()` resolved the registered CLSID and loaded the ASIO DLL in-process with `S_OK`.
- The returned object exposed the expected IASIO vtable.
- `getDriverName()` returned `Sound Blaster X4 ARM64`.
- `getDriverVersion()` returned `107`.
- Therefore the ARM64EC packaging/ABI barrier that blocked direct use of the Classic ARM64 DLL has been crossed successfully.

## What this does not yet prove

This probe does not itself prove that REAPER:

- enumerates the driver in its ASIO device list,
- calls the full initialization/query/buffer lifecycle successfully,
- starts transport,
- delivers host PCM through the ASIO callbacks,
- produces audible playback through the X4,
- or closes/reopens the driver cleanly in a real DAW session.

Those are the immediate next proof points.

## Immediate next action

Open REAPER ARM64EC and keep the first test frozen at:

- Audio system: ASIO
- ASIO Driver: `Sound Blaster X4 ARM64 ASIO`
- 48 kHz
- stereo output
- 512 frames

With normal Windows playback through X4 idle, verify in order:

1. driver appears in REAPER,
2. selecting it succeeds,
3. output channels 1/2 appear,
4. REAPER transport starts,
5. project/test audio is audible through X4,
6. stop/start works,
7. closing REAPER releases the X4 cleanly without a stuck stream or crash.

Do not bypass the existing BUSY coexistence gate.
