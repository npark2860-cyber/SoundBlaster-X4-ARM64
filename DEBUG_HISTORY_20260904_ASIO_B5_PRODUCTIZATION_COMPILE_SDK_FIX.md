# DEBUG HISTORY — ASIO B5 productization compile SDK fix

Date: 2026-09-04 KST

## First failing build

Workflow: `Build ASIO B5 Productization`

ARM64EC compilation reached the new B5 sources and failed in `wavert_engine_b5.cpp` with Windows SDK 10.0.26100.0:

- `_countof` not found at the X4 path buffer call.
- `KSRTAUDIO_GETREADPACKET_INFO::PerformanceCount` does not exist.
- informational C4324 padding warning on the deliberately 64-byte aligned B5 host buffers.

This was a B5 source/SDK compatibility failure, not a validated B4D transport regression.

### First fix

B5 was advanced to `60de28df150776eb8ff60ebb74d0c84483903f79`:

- ARM64EC WaveRT adapter supplies local compatibility definitions.
- Current SDK capture packet timestamp member `PerformanceCounterValue` is used.
- Classic ARM64 WaveRT adapter uses the same compatibility layer.
- Classic CMake compiles through that adapter.
- C4324 is suppressed only for B5 driver targets because 64-byte host-buffer alignment is deliberate.

## Second rebuild

The ARM64EC B5 DLL then compiled and linked successfully:

`x4-asio-arm64ec-b5.dll`

The build progressed to the B5 registration helper and stopped only because `register_b5_arm64ec.cpp` still contained four independent `_countof` uses.

The successful DLL link also exposed non-fatal warnings inherited from the shared B4D `driver.def`:

- LNK4104 for the four COM exports not marked PRIVATE.
- LNK4070 because the B4D `.def` declares `LIBRARY "x4-asio-arm64"` while the B5 DLL output is `x4-asio-arm64ec-b5.dll`.

### Second fix

B5 was advanced to:

`exp/windows-arm64-asio-b5-capability-productization@1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

Changes:

- removed all four `_countof` dependencies from `register_b5_arm64ec.cpp` using direct compile-time array-size expressions;
- added B5-only `driver_b5.def` with the four COM exports marked `PRIVATE` and no conflicting `LIBRARY` directive;
- ARM64EC B5 target now uses `driver_b5.def`;
- Classic ARM64 B5 target uses the same B5-only export file;
- validated B4D continues to use its original `driver.def` unchanged.

Compare against validated B4D `a95a95d014bcc1c3a521be41325841ae96dc8a61` after the second fix:

- ahead: 22
- behind: 0
- merge base: validated B4D

Validated B4D core files remain untouched.

## Status

The B5 ARM64EC DLL itself has compile/link PASS evidence from the second rebuild.

The complete workflow still needs one more run to prove:

- B5 register helper;
- product validation host;
- capability/KS helper targets in the combined build;
- Classic ARM64 B5 build;
- PE architecture checks;
- final ZIP packaging.

Do not run hardware validation until the workflow completes successfully.
