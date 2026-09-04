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

# B5 capability matrix is complete

Combined capability/runtime capture is fully proven:

`B5 COMBINED CAPABILITY PROBE RESULT: PASS`

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

# B5 productization implementation

Current B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

Implemented B5 contract remains:

- 2 outputs, `Int24LSB`
- 2 inputs at 48/96 kHz, `Int24LSB`
- output at 48/96/192 kHz
- 192 kHz reports zero inputs
- buffers 96..4800, granularity 48, preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- full-duplex capture-before-render start ordering
- joined worker lifecycle
- render/capture local+global BUSY gates before `KsCreatePin`
- shared functional B5 source for Classic ARM64 and ARM64EC

Validation identity remains side-by-side:

`Sound Blaster X4 ARM64 ASIO B5`

so proven B4D registration remains available.

## Compile progression

First build exposed SDK compatibility in the new WaveRT source and was fixed.

Second build successfully produced:

`x4-asio-arm64ec-b5.dll`

so the ARM64EC B5 transport DLL itself now has compile/link PASS evidence.

The same run then failed only in `register_b5_arm64ec.cpp` because four `_countof` uses remained. Non-fatal B5 link warnings also came from reusing the B4D export file.

Latest fixes on the same B5 branch:

- removed the four register-helper `_countof` uses;
- added `driver_b5.def` with B5-only PRIVATE COM exports and no conflicting LIBRARY name;
- ARM64EC and Classic ARM64 B5 targets both use `driver_b5.def`;
- original B4D `driver.def` is unchanged.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`

B5 now compares against validated B4D as:

- ahead 22
- behind 0
- merge base = validated B4D

---

# Immediate action

Re-run `Build ASIO B5 Productization`.

The workflow explicitly checks out the current B5 branch, so this run must use:

`1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

Do not perform hardware validation yet.

This build should now continue past the already-proven ARM64EC B5 DLL into:

1. B5 register helper;
2. B5 product validation host;
3. capability/KS helper targets in the combined build;
4. Classic ARM64 B5 DLL;
5. PE/ARM64X architecture checks;
6. final productization ZIP.

If it fails again, fix all remaining compile/link issues on this same B5 branch and keep changes bundled; do not return to A/B/C/D micro-tests.

After Actions PASS:

1. download `SoundBlaster-X4-ASIO-B5-Productization.zip`;
2. close other X4 playback / move default output away if needed;
3. run `install_and_validate_b5.cmd` once;
4. return `B5_PRODUCT_VALIDATION_REPORT.txt`.

Only after the bundled silent validation passes should one final REAPER test cover audible output + stereo input together.

## Immutable safety

Never bypass BUSY.

Historical collision class remains:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery
