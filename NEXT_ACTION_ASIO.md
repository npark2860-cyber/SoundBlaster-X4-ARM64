# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Current status

The first real DAW milestone is complete.

Hardware/user-proven chain:

```text
REAPER Windows ARM build (ARM64EC)
-> registered independent ARM64EC ASIO COM DLL
-> ASIO 2.x host contract / time-info callbacks
-> mapped WaveRT DMA
-> Microsoft usbaudio2.sys
-> Sound Blaster X4
-> audible project playback
```

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

B4D build proof:

- workflow `Build ASIO COM Stage B4D REAPER ARM64EC`
- run `33822642892`
- job `100868446837`
- result `success`
- ARM64X verification PASS

Registered-host proof:

```text
CoCreateInstance hr=0x00000000
driverName=Sound Blaster X4 ARM64
driverVersion=107
B4D HOST PROBE RESULT: PASS (REGISTRY COM LOAD + IASIO VTABLE)
```

REAPER proof:

- independent driver listed and selected
- output channels `X4 Output L/R` visible
- 48 kHz / 512 frames engine opens
- actual X4 playback works

Do not re-prove B4D unless a later change regresses it.

## New reference fact

The existing Creative `SB USB RT ASIO` driver also works correctly in the same REAPER ARM64EC environment.

Use it as a **behavioral reference** for capability discovery:

- channel counts/names
- sample types
- sample-rate support
- buffer-size contract
- latency reporting
- clock sources
- input/output exposure
- lifecycle behavior

Do not copy proprietary code or make Creative binaries a runtime dependency.

See `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`.

---

# Immediate next milestone — B5 capability/productization batch

Do not return to A/B/C/D micro-stages.

The next development cycle should be one coherent branch with measurable internal checkpoints and one combined user test package.

## Branch start

Before writing code, re-check the actual B4D branch HEAD.

If it still matches the validated source, create:

`exp/windows-arm64-asio-b5-capability-productization`

from:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do **not** branch implementation work from `main`; `main` carries documentation/orchestration state.

## B5-0 — reference/capability probe first

Before modifying the proven transport, establish a compact capability matrix from the working Creative reference driver and the X4 KS/WaveRT interface.

Probe Creative and our driver **sequentially**, never concurrently.

Collect at minimum:

1. ASIO registry name / CLSID discovery
2. `getDriverName()` / `getDriverVersion()`
3. `getChannels()`
4. `getBufferSize()` min/max/preferred/granularity
5. `getSampleRate()`
6. `canSampleRate()` over a candidate rate list
7. `getClockSources()`
8. `getChannelInfo()` for all exposed channels
9. sample types reported by each channel
10. `getLatencies()` after a valid buffer setup where safe
11. start/stop/reopen/close behavior

Also query the X4 `msft_wave` pin data ranges without creating an unsafe second render stream where possible.

The output of this probe becomes the independent implementation specification.

## B5 implementation targets

Work through these in the same development cycle, but preserve each result in debug history so regressions stay attributable.

### 1. Stability / lifecycle

Automate enough of the current known-good path to cover:

- repeated start/stop
- repeated buffer create/dispose
- repeated driver reopen/close
- longer continuous playback
- clean unload

Do not intentionally run another X4 stream concurrently. BUSY must remain a safe refusal.

### 2. 24-bit output

Add native 24-bit output only after the reference/KS matrix confirms the exact X4 format contract.

Preserve the 16-bit/48 kHz/512 path as a regression baseline.

### 3. Additional sample rates

Implement only rates confirmed by the reference driver and/or X4 KS data ranges.

Do not assume the full common set merely because the hardware is a modern USB audio device.

### 4. Selectable ASIO buffer sizes

Implement the measured Creative/ASIO contract rather than hardcoding an arbitrary list.

The existing 512-frame path must remain one validated option.

### 5. Capture/input

Add the narrowest practical stereo input path first.

Keep capture work separate from multichannel output expansion so failures remain diagnosable.

### 6. Dual-target maintainability

Preserve both build paths:

- Classic ARM64 for a pure ARM64 host
- ARM64EC/ARM64X for current REAPER ARM64EC

Where practical, share the functional ASIO/WaveRT source and keep architecture-specific adapter code thin.

---

# User-test strategy

Protect user time.

Do not ask for a manual test after every tiny code change.

Once a coherent B5 slice is buildable, package one combined sequence that checks:

1. architecture/PE validation
2. registry-free smoke
3. registration + COM host probe
4. capability report
5. REAPER load
6. playback at the newly supported formats/buffer sizes
7. start/stop/reopen stress
8. input test when capture is included

If one subtest fails, fix that subtest on the same B5 branch and rerun the workflow rather than creating unnecessary A/B/C/D branches.

---

# Known-good baseline — do not disturb casually

The proven B4D first-use configuration is:

- REAPER ARM64EC
- 48 kHz
- stereo output
- signed 16-bit PCM transport
- 512 ASIO frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- `writePacket = PacketCount + 1`
- slot = `writePacket % 2`
- local + global pin-instance coexistence gates
- B4A worker/joined-stop lifetime
- B4C ASIO 2.x time-info behavior

The core path is already proven. Extend it; do not rewrite it without evidence.

# Immutable safety rule

Never bypass BUSY.

Never intentionally reproduce the old ungated green-screen collision.

The previous failure class was:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` reused in `usbaudio2` recovery

The current coexistence gate is mandatory for every new format/rate/input path unless new evidence proves a different safe ownership model.

# Defer until B5 core is complete

- broad multichannel output
- direct monitoring
- ASIO time code
- MMCSS/AVRT tuning without measurement evidence
- custom kernel driver
- Creative runtime dependencies
- broader CTCDC feature work

# Relevant runtime records

Read before B5 work:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
- `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`
