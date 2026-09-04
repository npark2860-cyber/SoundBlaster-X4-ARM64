# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Current validated baseline

Real REAPER ARM64EC playback through the independent driver is hardware/user proven.

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Known-good B4D transport:

- REAPER ARM64EC
- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- `writePacket = PacketCount + 1`
- slot = `writePacket % 2`
- local + global pin-instance BUSY gates
- joined worker stop
- ASIO 2.x time-info callbacks

Do not re-prove or rewrite B4D unless B5 introduces a regression.

## Creative behavioral reference

Creative `SB USB RT ASIO` is confirmed working in the same REAPER ARM64EC environment. Use it only as a black-box behavioral reference. Do not copy proprietary code or add Creative runtime dependencies.

Reference dimensions:

- input/output channel counts and names
- ASIO sample types
- sample-rate support
- buffer min/max/preferred/granularity
- latency reporting
- clock sources
- lifecycle behavior

See:

`DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`

---

# B5-0 is implemented — next action is one capability capture

B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

Parent:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Compare state:

- ahead of validated B4D by exactly 1 commit
- behind by 0
- validated B4D is the merge base

Implementation record:

`DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_PROBE_IMPLEMENTED.md`

## What B5-0 contains

The validated B4D transport/core is unchanged. B5-0 adds measurement-only tools:

- `x4-asio-stage-b5-capability-probe.exe`
- `x4-asio-stage-b5-ks-probe.exe`
- `probe_b5.cmd`
- `README_B5_CAPABILITY.md`

The ASIO probe records the Creative and independent contracts in the same format. The KS probe is property-only and queries pin counts/dataflow, local/global instances, and `KSPROPERTY_PIN_DATARANGES` without creating a pin.

The combined script performs, sequentially:

1. X4 idle/BUSY gate + KS/WaveRT data ranges
2. ASIO registry list
3. Creative report + three silent start/stop/reopen cycles
4. post-Creative idle/BUSY re-check
5. independent driver report + three silent start/stop/reopen cycles

Output:

`B5_CAPABILITY_REPORT.txt`

Do not run Creative and the independent driver concurrently.

## Manual build workflow

Workflow:

`.github/workflows/build-asio-b5-capability-arm64ec.yml`

Name:

`Build ASIO B5 Capability Probe ARM64EC`

Trigger:

`workflow_dispatch` only

The workflow checks out the B5 branch, verifies validated-B4D ancestry and frozen-core equality, builds only the two ARM64EC probes, verifies final `0x8664` + `ARM64X`, and uploads:

`SoundBlaster-X4-ASIO-B5-Capability-ARM64EC.zip`

## Immediate action

Run the manual B5 capability workflow once.

If the build passes, on the X4 test system:

- keep the existing validated B4D registration;
- close REAPER and other active X4 playback first;
- run `probe_b5.cmd` once;
- return `B5_CAPABILITY_REPORT.txt`.

If the gate reports BUSY or INDETERMINATE, do not bypass it. The probe must stop.

---

# After the capability report — one B5 productization batch

Do not return to A/B/C/D micro-stages and do not request user testing after every internal change.

Use the measured Creative + KS matrix as the specification and implement as much of the following as the matrix supports in one coherent branch cycle:

1. **24-bit output** using the exact reported X4/Creative format contract.
2. **Additional sample rates** only when confirmed by `canSampleRate()` and/or X4 KS data ranges.
3. **Selectable ASIO buffer sizes** using the measured min/max/preferred/granularity contract while keeping 512 frames as a regression option.
4. **Stability/lifecycle hardening** for repeated create/start/stop/dispose/reopen plus longer playback.
5. **Narrow stereo input** first; do not mix this with broad multichannel expansion.
6. **Dual-target maintainability** so Classic ARM64 and ARM64EC/ARM64X share functional source where practical.

Then create one combined validation package covering architecture, registration/COM, capability, playback formats/buffers, lifecycle stress, and input.

## Immutable safety rule

Never bypass BUSY.

Never intentionally reproduce the historical active-render green-screen collision.

Known failure class:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path observed in `usbaudio2` recovery

Every new output format/rate/buffer path must retain the local/global ownership gate before pin creation. Input work must establish and respect its own relevant pin ownership state rather than weakening the render gate.

## Defer until core B5 is stable

- broad multichannel output
- direct monitoring
- ASIO time code
- MMCSS/AVRT tuning without measurement evidence
- custom kernel driver
- Creative runtime dependencies
- broader CTCDC work

## Read before continuing B5

- `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_PROBE_IMPLEMENTED.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
- `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
- `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`
