# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated baseline

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety remains:

- never bypass local/global BUSY gates
- never intentionally reproduce the historical active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@869307d44750af3e23c2de68dc84cc32d9b5e05f`

B5 first-release contract remains:

- 2 output channels, Int24LSB
- 2 input channels at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz reports zero input channels
- buffer 96..4800 / granularity 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

---

# Latest runtime evidence

Latest returned runtime report still proves:

- 48k/240 output x3 PASS
- 48k/240 full duplex x2 PASS
- 96k/240 full duplex failure caused by serial Render->Capture notification waits and auto-reset event coalescing

The dual-event mux was introduced to fix that structural problem.

---

# Latest build failure

The first mux build failed before DLL creation:

```text
cguid.h(33,18): error C2059: syntax error: '__uuidof'
```

while compiling `driver_b5_arm64ec.cpp` with Windows SDK 10.0.26100.0.

Cause: `#define private public` was active while project headers pulled Windows/COM SDK declarations.

This is a compile-time adapter error only. Do not derive any new hardware conclusion from this run.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`

---

# Fix now implemented — dual-event-mux-v2

The SDK-facing keyword macro contamination is removed.

ARM64EC and Classic adapters now include all SDK/project headers normally first.

WaveRT engine access from the mux is through a narrow public API:

`process_signaled_notification(...)`

Implementation:

`wavert_engine_b5_signaled.inl`

Behavior retained:

- simultaneous stop/capture/render wait in full duplex
- capture lower wait index
- exact render N / capture N-1 pairing
- render write-ahead N+1
- capture `ERROR_NOT_READY` => transient `NoData`
- `MoreData=TRUE` drain
- strict packet discontinuity detection
- strict callback-index/copy/sync failure handling
- MMCSS `Pro Audio` + `AVRT_PRIORITY_CRITICAL`

Runtime/build marker is now:

`dual-event-mux-v2`

The main workflow scans both built DLLs for this exact marker and fails before packaging if either lacks it.

---

# Immediate action

Run manual workflow:

`Build ASIO B5 Productization`

Do **not** run hardware validation until the workflow passes all of:

1. ARM64EC DLL compile/link;
2. B5 helper compile/link;
3. Classic ARM64 DLL compile/link;
4. PE/ARM64X architecture checks;
5. `dual-event-mux-v2` present in both DLLs;
6. ZIP package produced.

If build fails, return the exact Actions log. Continue fixing all related build/link/workflow problems on this same B5 branch; do not create a microbranch and do not request a hardware micro-test.

After build PASS only:

1. download the new `SoundBlaster-X4-ASIO-B5-Productization.zip`;
2. close other X4 playback/default endpoint ownership as before;
3. run `install_and_validate_b5.cmd` once;
4. return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.

A runtime report counts as mux-v2 evidence only if it contains:

`adapter=dual-event-mux-v2`

The strict matrix must still reach:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

Only after full matrix PASS should final REAPER validation cover audible 24-bit output plus real stereo input together.
