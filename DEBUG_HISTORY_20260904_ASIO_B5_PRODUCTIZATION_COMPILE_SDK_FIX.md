# DEBUG HISTORY — ASIO B5 productization compile SDK fix

Date: 2026-09-04 KST

## Failing build

Workflow: `Build ASIO B5 Productization`

ARM64EC compilation reached the new B5 sources and failed in `wavert_engine_b5.cpp` with Windows SDK 10.0.26100.0:

- `_countof` not found at the X4 path buffer call.
- `KSRTAUDIO_GETREADPACKET_INFO::PerformanceCount` does not exist.
- informational C4324 padding warning on the deliberately 64-byte aligned B5 host buffers.

This was a B5 source/SDK compatibility failure, not a validated B4D transport regression.

## Root cause

Current `ksmedia.h` names the capture packet timestamp member `PerformanceCounterValue`.

The ARM64EC source adapter also cannot rely on `_countof` surviving the architecture-macro adaptation used to include the shared Classic-ARM64 implementation.

C4324 is expected padding required to honor explicit cache-line alignment; it is not a runtime defect.

## Fix

B5 branch was advanced to:

`exp/windows-arm64-asio-b5-capability-productization@60de28df150776eb8ff60ebb74d0c84483903f79`

Changes:

- ARM64EC WaveRT adapter supplies a local `_countof` equivalent and aliases the old source token to SDK field `PerformanceCounterValue`.
- Added a Classic ARM64 WaveRT adapter with the same SDK compatibility definitions so both targets remain on one shared functional implementation.
- Classic CMake now compiles through that adapter.
- C4324 is suppressed only for the B5 driver targets because the 64-byte audio-buffer alignment is deliberate.

Validated B4D core files remain untouched.

Compare against validated B4D `a95a95d014bcc1c3a521be41325841ae96dc8a61` after the fix:

- ahead: 18
- behind: 0
- merge base: validated B4D

## Status

Fix implemented. Rebuild pending.

Do not run hardware validation until `Build ASIO B5 Productization` compiles and packages successfully.
