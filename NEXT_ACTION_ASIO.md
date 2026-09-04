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

`exp/windows-arm64-asio-b5-capability-productization@60de28df150776eb8ff60ebb74d0c84483903f79`

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

## Latest compile failure and fix

First `Build ASIO B5 Productization` run reached the new B5 ARM64EC sources and failed only on SDK compatibility:

- `_countof` unavailable in the ARM64EC adapted shared source;
- Windows SDK 10.0.26100.0 uses `KSRTAUDIO_GETREADPACKET_INFO::PerformanceCounterValue`, not `PerformanceCount`;
- C4324 was informational padding from intentional 64-byte host-buffer alignment.

Fixed on the same B5 branch without touching validated B4D core:

- ARM64EC WaveRT SDK adapter added compatibility definitions;
- Classic ARM64 WaveRT adapter added with the same definitions;
- Classic CMake switched to that adapter;
- C4324 suppressed only on B5 driver targets.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`

After the fix B5 is ahead 18 / behind 0 from validated B4D, with validated B4D still the merge base.

---

# Immediate action

Re-run `Build ASIO B5 Productization`.

The workflow explicitly checks out the current B5 branch, so the rebuild must use:

`60de28df150776eb8ff60ebb74d0c84483903f79`

Do not perform hardware validation yet.

If compile fails again, fix the remaining compiler/linker issues on the same B5 branch and keep bundling changes; do not return to A/B/C/D micro-tests.

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
