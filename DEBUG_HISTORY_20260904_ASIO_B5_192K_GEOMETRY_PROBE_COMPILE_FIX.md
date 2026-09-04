# DEBUG HISTORY — ASIO B5 192 kHz geometry probe compile fix

Date: 2026-09-04 KST

## Context

After mux-v3 runtime proved 96 kHz / 240 full duplex PASS twice, the next blocker moved to 192 kHz / 240 output:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

A separate ARM64EC geometry probe was added to scan 192 kHz Render Pin 1 notification sizes without entering KSSTATE_RUN.

## Build failure

The first workflow build of the new probe failed only in:

`geometry_probe_b5_arm64ec.cpp`

with:

`error C3861: '_countof': identifier not found`

at the `find_x4_wave_path(path, _countof(path))` call.

All existing B5 ARM64EC outputs before that target compiled and linked successfully, including:

- `x4-asio-arm64ec-b5.dll`
- register helper
- product-validation helper
- capability probe
- KS probe

Therefore this was isolated to the new measurement helper and had no runtime meaning.

## Fix

On the existing B5 branch, replace the `_countof` dependency with a standard compile-time array count:

`constexpr size_t path_chars = sizeof(path) / sizeof(path[0]);`

and pass `path_chars` to `find_x4_wave_path`.

No WaveRT runtime, mux, BUSY gate, pin ownership, product contract, or B4D source changed.

## Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@127ca482ef18575ce4dc69d03d41a5a01287e992`

## Next action

Re-run `Build ASIO B5 Productization`.

If it passes, run only `probe_b5_192k_geometry.cmd` from the new ZIP and return `B5_192K_GEOMETRY_REPORT.txt`.
