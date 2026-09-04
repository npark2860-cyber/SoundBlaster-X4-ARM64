# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch:

`main`

Verified `main` immediately before this handoff update:

`a6ee02a96f50b371b7c58def094d5f03827b4171`

Validated Stage B4D source branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration`

Verified B4D HEAD:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

At the start of the next chat, verify the actual GitHub heads again. Do not reconstruct state from old conversation context.

## Read order for the next chat

Read these first, in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
3. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
4. `NEXT_ACTION_ASIO.md`
5. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
7. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
8. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

Only if work returns to device-control/CTCDC, then read:

- `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
- `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
- `DEBUG_HISTORY_20260903_CTCDC_MAX_PAYLOAD_RUNTIME.md`
- `DEBUG_HISTORY_20260903_CTCDC_OPEN_SESSION_RUNTIME.md`
- `DEBUG_HISTORY_20260903_DIRECT_MODE_RUNTIME_SUCCESS.md`
- `NEXT_ACTION_CTCDC.md`

---

# Project status

The project has two independent Windows-on-Arm tracks:

1. **ASIO** — independent user-mode ASIO implementation over Microsoft KS/WaveRT / `usbaudio2.sys`.
2. **CTCDC control** — independent X4 control path over the USB CDC/COM interface.

Current priority is **finish ASIO to a practical first-release level before returning to broader CTCDC work**.

Creative binaries are reference-only. The final independent implementation must not load or redistribute Creative binaries.

Device:

- Sound Blaster X4
- codename `Accent2`
- model/package `SB1815`
- USB VID/PID `041E:3278`

---

# ASIO — CURRENT PRIORITY

## Major milestone is complete

The independent driver is no longer only a smoke-test implementation.

Real REAPER playback on Windows ARM is hardware/user proven:

```text
REAPER Windows ARM build (ARM64EC)
-> registered independent ARM64EC ASIO COM DLL
-> ASIO 2.x host contract / time-info callbacks
-> driver-owned host buffers
-> mapped WaveRT DMA
-> Microsoft usbaudio2.sys
-> Sound Blaster X4
-> audible project playback
```

The user confirmed that actual REAPER project playback through the X4 works correctly.

## Validated B4D implementation

Branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration`

HEAD:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated parent B4C:

`e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

B4D deliberately keeps the hardware-proven B4C transport implementation intact and adds thin ARM64EC adapter translation units plus registration/test tooling.

Hardware-proven inherited files were not modified by B4D:

- `driver_b4c.cpp`
- `preflight.cpp`
- `wavert_engine_b4a.cpp`
- `smoke_b4c_monotonic.cpp`

## B4D Actions proof

Workflow:

`Build ASIO COM Stage B4D REAPER ARM64EC`

Run/job:

- run `33822642892`
- job `100868446837`
- checkout `a95a95d014bcc1c3a521be41325841ae96dc8a61`
- result `success`

Final ARM64EC/ARM64X validation passed for:

- `x4-asio-arm64ec.dll`
- `x4-asio-stage-b4d-smoke.exe`
- `x4-asio-stage-b4d-register.exe`
- `x4-asio-stage-b4d-host-probe.exe`

Important hashes from the validated run:

- DLL: `E6E5905F0F616BE0E96FEC3F5D576C1295EFEA4BFBFF58DC5AF3F2C4AD152060`
- smoke: `59525037ED156A1F8B9B43693FA76A7A8AD053D9CEDCCF47824C7DB9674B75E9`
- register helper: `ECAA42A79F5A5DB9E35CA692F74A617121BEB6AA0BCA5A1E91C0786BF4D77045`
- host probe: `A7AAF2BBA732970DADE64EC660DDF7C9D74F2D55CC2DEE89A137BB9753406AAD`
- inner distribution ZIP: `C739A18BCEFF7F91610C9E09803BACCB26D8060016686AEA7A1CCB954F9E9FC5`

The packaged `x4-asio-arm64.dll` is a byte-identical alias of `x4-asio-arm64ec.dll` only for the inherited smoke loader filename.

## Registration / host proof

Observed:

```text
Registration verified. Probing normal COM registry load from ARM64EC host...
CoInitializeEx hr=0x00000000
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

This proves normal registry COM discovery and in-process IASIO vtable use from an ARM64EC host.

## REAPER proof

REAPER successfully listed and selected the independent driver and exposed:

- `1: X4 Output L`
- `2: X4 Output R`

The first validated REAPER engine state was:

```text
48kHz
512spls
~10/10ms ASIO
```

The `24bit WAV` text visible in REAPER referred to the source/project side; the current driver transport is still fixed 16-bit stereo PCM.

Actual playback through X4 was confirmed working.

## Proven baseline to preserve

Use this as the known-good first-release baseline while expanding features:

- Windows ARM host: REAPER ARM64EC
- 48 kHz
- stereo output only
- signed 16-bit PCM transport
- ASIO buffer 512 frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- `writePacket = PacketCount + 1`
- slot = `writePacket % 2`
- both C-instance/global-instance coexistence gates
- B4A worker lifetime / joined stop
- B4C ASIO 2.x time-info path

Do not reopen or rewrite this core path without a concrete reason.

---

# Creative SB USB RT ASIO — NEW REFERENCE FACT

The existing Creative `SB USB RT ASIO` driver also works correctly in the same Windows-on-Arm REAPER environment.

User-observed facts:

- it is listed by REAPER;
- it can be selected;
- Creative input/output channels are exposed;
- actual playback works.

This changes the practical framing:

- the independent driver is **not** needed merely to make ASIO possible in today's ARM64EC REAPER;
- its value is independence from Creative runtime binaries, control over implementation, and preservation of a Classic ARM64 path for a possible future pure-ARM64 host;
- the working Creative driver is now an excellent behavioral reference for completing our implementation quickly.

Do not copy proprietary implementation code. Use Creative only as a black-box/static behavioral reference.

High-value reference comparisons:

- `getChannels()`
- `getBufferSize()`
- `getLatencies()`
- `getSampleRate()` / `canSampleRate()`
- clock-source reporting
- `getChannelInfo()` names/types
- supported sample-rate set
- selectable buffer sizes
- input/output channel exposure
- start/stop/reopen/close behavior

Do not infer that Creative's driver would work in a future pure Classic ARM64 REAPER. That case has not been tested.

---

# ASIO safety — immutable rule

The old ungated path can collide with an existing X4 render stream and trigger a kernel failure.

Hardware/crash evidence established:

- active concurrent X4 playback was the runtime differentiator;
- crash class `WDF_VIOLATION 0x10D`, Parameter 1 = 5;
- prior dump showed `usbaudio2!UAWdfUsbDataPipe::SendBufferToTarget` using a stale/destroyed `WDFUSBPIPE` during recovery;
- Creative static analysis revealed a pin-instance gate using `KSPROPERTY_PIN_CINSTANCES` and `KSPROPERTY_PIN_GLOBALCINSTANCES`;
- the independent driver now refuses BUSY before `KsCreatePin`.

Rules:

- **never bypass BUSY**;
- **never intentionally reproduce the old green-screen collision**;
- do not remove the coexistence gate while adding capability.

---

# NEXT ASIO WORK — SPEED-OPTIMIZED PRODUCTIZATION

The next chat should not restart A/B/C/D-style micro-staging.

The next goal is a **B5 capability/productization batch** built from the validated B4D source, with one coherent implementation branch and a combined test package.

## Start point

Create the next implementation branch from:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Suggested branch name:

`exp/windows-arm64-asio-b5-capability-productization`

Do not base implementation work on `main`; `main` is documentation/orchestration state.

## First B5 action: reference/capability probe

Before changing transport format logic, quickly establish the official and hardware capability matrix.

Probe the working Creative reference driver and X4 KS/WaveRT data ranges for:

1. output/input channel counts and names;
2. ASIO sample types;
3. supported sample rates;
4. buffer-size min/max/preferred/granularity;
5. reported latencies;
6. clock sources;
7. reopen/start-stop behavior.

Run Creative and independent-driver probes **sequentially**, not concurrently.

Use the results as a specification for independent implementation, not as code to copy.

## B5 implementation targets

Prioritize these in one development cycle, but preserve measurable sub-results in logs:

1. **stability** — longer playback plus repeated start/stop and REAPER reopen/close;
2. **24-bit output**;
3. **additional X4-supported sample rates** discovered by the reference/KS probe;
4. **selectable ASIO buffer sizes** based on the measured/reference contract;
5. **capture/input**, initially the narrow stereo input path;
6. **dual build hygiene** so Classic ARM64 and ARM64EC remain maintainable from the same functional source where practical.

Do not ask the user to test after every tiny internal change. Build a combined smoke/registration/REAPER validation package once a coherent B5 slice is ready.

## Defer until core B5 is stable

- multichannel output beyond the first practical stereo release
- direct monitoring
- ASIO time code
- MMCSS/AVRT tuning unless measurements show a need
- custom kernel driver
- Creative runtime dependencies
- broader CTCDC feature work

---

# Classic ARM64 value

Classic ARM64 B4C is already hardware-proven independently of REAPER ARM64EC.

If a future REAPER build becomes pure Classic ARM64, an x64/ARM64EC in-process ASIO DLL would not be the right ABI. Preserve the Classic ARM64 implementation path rather than replacing it with ARM64EC-only code.

Current architecture goal should therefore remain:

```text
Classic ARM64 host -> Classic ARM64 build
ARM64EC host       -> ARM64EC/ARM64X build
                     \
                      same independent ASIO/WaveRT behavior
```

---

# CTCDC — PAUSED, NOT LOST

CTCDC Direct Mode remains hardware-proven and should not be rediscovered.

Confirmed Windows fast path includes:

- CDC/COM serial initialization
- `5A 03 00` maximum-payload query
- maximum payload 59
- firmware query
- buttons query
- Direct Mode OFF `5A 39 03 00 05 00`
- Direct Mode ON `5A 39 03 00 05 01`

The native AES-GCM unlock/challenge fallback has also already been recovered.

Do not revisit excluded BLE/HID/naked-COM/UAC-extension/vendor-interface paths.

Resume broader CTCDC productization only after the ASIO first-release capability pass is closed.
