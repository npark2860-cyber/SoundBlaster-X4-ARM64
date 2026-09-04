# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety:

- never bypass local/global BUSY gates
- never intentionally reproduce the active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@127ca482ef18575ce4dc69d03d41a5a01287e992`

Runtime/build marker:

`dual-event-mux-v3`

---

# Latest returned runtime

Report generated `2026-09-04 12:37:34.26`.

PASS:

- registration
- property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2

96k duplex now stops cleanly with no strict packet/index/copy errors. Capture still trails render (`capturePhaseMisses=27/23`), so exact duplex cadence is not yet closed, but mux-v2's false synchronization failure is fixed.

New runtime failure:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

at 192k/240 output during `createBuffers()`, before worker creation or KSSTATE_RUN.

This moves the immediate runtime blocker from scheduling to 192 kHz WaveRT buffer geometry.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`

---

# Measurement tool

ARM64EC target:

`x4-asio-stage-b5-192k-geometry-probe`

Packaged runner:

`probe_b5_192k_geometry.cmd`

Behavior:

- 192 kHz / stereo / 24-bit / Render Pin 1
- never enters KSSTATE_RUN
- checks local/global FREE before every `KsCreatePin`
- scans 48..960 frames per notification in 48-frame steps
- equivalent to 0.25..5.0 ms per notification at 192 kHz
- NotificationCount=2
- records requested bytes, PASS/FAIL, Win32 error and `ActualBufferSize`
- closes each pin and requires the gate to return FREE before the next candidate

The main productization workflow builds and packages this probe but does not execute it automatically.

---

# Latest build status — geometry probe compile fix

The first workflow build containing the new geometry probe compiled and linked all existing ARM64EC B5 targets, then failed only in `geometry_probe_b5_arm64ec.cpp` with:

`error C3861: '_countof': identifier not found`

The failing call was:

`find_x4_wave_path(path, _countof(path))`

This was isolated to the new helper and has no runtime meaning.

Fixed on the same B5 branch by replacing the macro dependency with:

`constexpr size_t path_chars = sizeof(path) / sizeof(path[0]);`

and passing `path_chars`.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_PROBE_COMPILE_FIX.md`

No WaveRT runtime logic, mux-v3 behavior, BUSY gate, ownership rule, product contract, or validated B4D source changed.

---

# Immediate action

Re-run manual workflow:

`Build ASIO B5 Productization`

Required build outcome:

1. ARM64EC B5 DLL + normal helpers compile/link PASS;
2. `x4-asio-stage-b5-192k-geometry-probe.exe` compile/link PASS;
3. Classic ARM64 DLL compile/link PASS;
4. PE/ARM64X checks PASS;
5. both B5 DLLs contain `dual-event-mux-v3`;
6. ZIP is produced with `probe_b5_192k_geometry.cmd`.

After build PASS:

1. download the new ZIP;
2. close REAPER/media players/Creative App playback and any other X4 user;
3. run `probe_b5_192k_geometry.cmd` once;
4. return `B5_192K_GEOMETRY_REPORT.txt`.

Do not rerun the full product matrix first; the known blocker is already the 192 kHz geometry allocation.

Do not patch product buffer geometry until the probe identifies the first accepted 192 kHz notification size.

If the geometry probe reports BUSY/INDETERMINATE or the pin does not return FREE after closing a candidate, stop instead of retrying repeatedly.
