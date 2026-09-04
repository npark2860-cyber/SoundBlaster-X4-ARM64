# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch:

`main`

Verified `main` immediately before this handoff update:

`d6d1134de1cd683bee5fbba5392d9f132f5c8453`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 implementation branch:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

At the start of the next chat, verify the actual GitHub heads again. Do not reconstruct state from old conversation context.

## Read order

Read these first, in this order:

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_PROBE_IMPLEMENTED.md`
3. `NEXT_ACTION_ASIO.md`
4. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
5. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`
7. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
8. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
9. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

Only if work returns to device-control/CTCDC, then read its existing CTCDC history and `NEXT_ACTION_CTCDC.md`. CTCDC is currently deferred until the ASIO first-release capability/productization pass is closed.

---

# Current priority — ASIO B5 capability/productization

## Proven baseline

Real REAPER Windows ARM/ARM64EC playback through the independent ASIO driver is hardware/user proven.

Known-good B4D configuration:

- 48 kHz
- stereo output only
- signed 16-bit PCM transport
- 512 ASIO frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- `writePacket = PacketCount + 1`
- slot = `writePacket % 2`
- local + global pin-instance ownership gates
- B4A joined worker stop
- B4C ASIO 2.x time-info callbacks

The validated B4D source branch remains exactly at:

`a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not modify or re-prove this baseline without a concrete regression reason.

## Creative behavioral reference

Creative `SB USB RT ASIO` is also confirmed working in the same REAPER ARM64EC environment, including actual playback and exposed input/output channels.

Use Creative only as a behavioral reference for:

- channel counts/names
- ASIO sample types
- sample-rate support
- buffer-size contract
- latency reporting
- clock sources
- lifecycle behavior

Do not copy proprietary implementation code and do not create a Creative runtime dependency.

---

# B5-0 implementation state

B5-0 measurement tooling is implemented on:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

It is exactly one commit ahead of validated B4D and zero commits behind. Validated B4D is the merge base.

Important: **B5-0 has not yet received build/runtime PASS evidence.** It is implemented and waiting for the single capability build/capture.

Added source/tooling:

- `src/asio-arm64-stage-b0/capability_probe_b5_arm64ec.cpp`
- `src/asio-arm64-stage-b0/ks_capability_probe_b5_arm64ec.cpp`
- `src/asio-arm64-stage-b0/probe_b5.cmd`
- `src/asio-arm64-stage-b0/README_B5_CAPABILITY.md`
- B5 probe targets appended to `src/asio-arm64-stage-b0/CMakeLists.txt`

The validated B4D transport/core files remain unchanged.

## B5-0 capability capture

The combined sequence is designed to collect one report rather than repeat A/B/C/D-style user tests.

It records:

- ASIO registry name / CLSID / registry view
- driver name/version
- input/output channel counts
- every channel name and raw sample type
- buffer min/max/preferred/granularity
- current sample rate and `canSampleRate()` candidate matrix
- clock sources
- latency reporting
- repeated create/start/stop/dispose/reopen behavior
- X4 KS pin count/dataflow/local+global instances
- X4 `KSPROPERTY_PIN_DATARANGES` audio channel/bit/rate ranges

Creative is probed first. After it is released, the X4 idle gate is checked again before the independent driver is opened.

Output file:

`B5_CAPABILITY_REPORT.txt`

## Manual build workflow

Main workflow:

`.github/workflows/build-asio-b5-capability-arm64ec.yml`

Workflow name:

`Build ASIO B5 Capability Probe ARM64EC`

Trigger:

`workflow_dispatch` only

The workflow checks out the B5 branch, verifies ancestry from B4D and frozen-core equality, builds only the two B5 probes, verifies final PE `0x8664` plus `ARM64X`, and packages:

`SoundBlaster-X4-ASIO-B5-Capability-ARM64EC.zip`

---

# Immutable safety rule

Never bypass BUSY.

Never intentionally reproduce the old active-render green-screen collision.

Known failure evidence:

- active concurrent X4 playback was the differentiator;
- `WDF_VIOLATION 0x10D`, Parameter 1 = 5;
- stale/destroyed `WDFUSBPIPE` path was observed in `usbaudio2` recovery;
- the validated driver refuses BUSY before `KsCreatePin` using both local and global instance state.

B5-0 preserves this rule twice:

1. its KS probe is property-only and contains no `KsCreatePin` call;
2. `probe_b5.cmd` requires the known Render Pin 1 local/global gate to be FREE before lifecycle work and re-checks it after Creative release.

If the gate is BUSY or indeterminate, stop. Do not override it.

---

# Immediate next action

1. Run `Build ASIO B5 Capability Probe ARM64EC` once.
2. If build passes, keep the existing validated B4D registration, close REAPER/other X4 playback, and run the packaged `probe_b5.cmd` once.
3. Return `B5_CAPABILITY_REPORT.txt`.

Do not implement 24-bit/rates/buffers/input by assumption before this report.

After the capability matrix is known, continue on the same B5 branch and implement as much as supported in one coherent productization cycle:

- 24-bit output
- confirmed additional sample rates
- measured selectable buffer sizes
- lifecycle/stability hardening
- narrow stereo input
- Classic ARM64 / ARM64EC maintainability

Then create one combined validation package. Do not ask for a manual user test after every internal change.

---

# CTCDC

CTCDC Direct Mode and its recovered session/control path remain preserved in the existing history. Do not rediscover BLE/HID/naked-COM or other previously excluded paths.

Broader CTCDC work remains paused until B5 first-release ASIO capability/productization is complete.
