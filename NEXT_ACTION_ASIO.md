# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated baseline

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Known-good B4D runtime:

- REAPER ARM64EC real playback
- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- Render Pin 1
- local + global BUSY gates
- joined worker stop
- ASIO 2.x time-info

Do not alter or re-prove B4D unless B5 exposes a concrete regression.

---

# B5 capability matrix is now complete

Second combined runtime capture result:

`B5 COMBINED CAPABILITY PROBE RESULT: PASS`

See:

`DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`

Measured Creative reference:

- 2 inputs / 10 outputs
- all channels `Int24LSB` type 17
- buffers 96..4800, preferred 240, granularity 48
- rates 48/96/192 kHz only
- Internal Clock
- preferred latency 240 in / 240 out
- lifecycle reopen x3 PASS

Measured X4 KS ranges:

- Render Pin 1: 2/6/8ch, 16/24-bit, 48/96/192 kHz
- Capture Pin 4: stereo, 16/24-bit, 48/96 kHz

Independent B4D lifecycle x3 also re-passed in the same capture.

---

# B5 productization is implemented

Current B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@de87abca5540a077e5a9d9bf9708738ccbefecd5`

The B5 implementation was performed as one coherent batch. Validated B4D core files remain unchanged.

B5 validation identity is side-by-side:

`Sound Blaster X4 ARM64 ASIO B5`

so the existing proven B4D entry remains available.

Implemented B5 contract:

- 2 outputs, `Int24LSB`
- 2 inputs at 48/96 kHz, `Int24LSB`
- output at 48/96/192 kHz
- 192 kHz reports zero inputs
- ASIO buffer contract 96..4800 frames, granularity 48, preferred 240
- 512 frames accepted as a compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT paths
- full-duplex capture-before-render start ordering
- joined worker lifecycle
- separate render/capture BUSY gates immediately before pin creation
- shared functional B5 source for Classic ARM64 and ARM64EC

## Immutable safety

Never bypass BUSY.

Historical failure class remains:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery

B5 retains the proven Render Pin 1 gate at `init()` and re-checks the relevant render/capture pin directly before each `KsCreatePin`.

---

# Immediate action — one build, then one bundled runtime validation

Manual workflow on `main`:

`.github/workflows/build-asio-b5-productization.yml`

Workflow name:

`Build ASIO B5 Productization`

## Step 1

Run this workflow once.

It builds both:

- ARM64EC/ARM64X B5 validation driver and tools
- Classic ARM64 B5 DLL from the same functional source

If Actions fails, fix the compile issue on the same B5 branch. Do not create A/B/C/D-style micro-branches and do not ask for a hardware test until the full package builds.

## Step 2

After Actions PASS, download:

`SoundBlaster-X4-ASIO-B5-Productization.zip`

On the X4 test system:

- close REAPER and other X4 playback;
- move Windows default playback away from X4 if needed;
- run `install_and_validate_b5.cmd` once;
- return `B5_PRODUCT_VALIDATION_REPORT.txt`.

The script performs a single bundled silent matrix covering preferred/min/max/512 buffers, 48/96/192 output, full duplex at 48/96, repeated reopen, registration and BUSY safety.

If the initial gate is BUSY/indeterminate, it stops before lifecycle work. Do not override it.

## Step 3 — only after bundled runtime PASS

Perform one final REAPER B5 real-use validation covering actual audible output plus stereo input. Do not split this into per-feature micro-tests.

Until Actions compile PASS and the bundled runtime report pass, the new B5 transport is implemented but **not yet hardware-proven**.
