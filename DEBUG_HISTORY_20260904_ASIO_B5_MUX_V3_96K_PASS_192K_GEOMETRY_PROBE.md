# ASIO B5 mux-v3 runtime: 96 kHz duplex PASS, 192 kHz WaveRT geometry probe

Date: 2026-09-04 KST

## Returned runtime report

User returned `B5_PRODUCT_VALIDATION_REPORT(4).txt`, generated `2026-09-04 12:37:34.26`.

The loaded driver clearly identifies:

`adapter=dual-event-mux-v3`

MMCSS `Pro Audio` and critical priority were active.

## Proven PASS

- registration PASS
- property-only Render Pin 1 idle gate FREE
- KS capability probe PASS
- 48 kHz / 240 output-only x3 PASS
- 48 kHz / 240 full duplex x2 PASS
- 96 kHz / 240 full duplex x2 PASS

96 kHz results:

- cycle 1: callbacks=278, renderNotif=278, captureNotif=252, stop=ASE_OK
- cycle 2: callbacks=278, renderNotif=278, captureNotif=255, stop=ASE_OK
- no render/capture packet discontinuity was reported by the strict stop checks
- no callback-index or render/capture copy failure was reported

This proves mux-v3 removed the mux-v2 false-positive exact-phase failure.

Capture is still not 1:1 with render at 96 kHz: the worker reported 27 and 23 capture phase misses. This remains a quality/latency issue to revisit after first-release geometry is understood; it is no longer a fatal lifecycle failure.

## New failure: 192 kHz / 240 output

The first 192 kHz output case failed during `createBuffers()`, before worker creation or KSSTATE_RUN:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

Therefore this is not the mux-v3 notification scheduler.

Current host/hardware geometry assumption is:

- ASIO frames = 240
- stereo Int24 = 6 bytes/frame
- notification count = 2
- per-notification hardware packet = 1440 bytes
- requested cyclic buffer = 2880 bytes
- at 192 kHz, one 240-frame notification period = 1.25 ms

The exact same 2880-byte cyclic request succeeds at 48 and 96 kHz, so byte divisibility alone does not explain the 192 kHz rejection. The working hypothesis is a sample-rate-dependent minimum/service-period constraint in the WaveRT/usbaudio2 path.

## Measurement-only geometry probe implemented

B5 branch now includes:

- `src/asio-arm64-stage-b0/geometry_probe_b5_arm64ec.cpp`
- `src/asio-arm64-stage-b0/probe_b5_192k_geometry.cmd`
- target `x4-asio-stage-b5-192k-geometry-probe`

The probe:

- uses X4 `msft_wave`, Render Pin 1
- uses 192 kHz / stereo / 24-bit PCM
- never enters KSSTATE_RUN
- checks local + global Render Pin 1 FREE gate before every `KsCreatePin`
- creates a fresh pin for one candidate geometry
- asks only for `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- records PASS/FAIL, Win32 error and `ActualBufferSize`
- closes the pin and refuses the next candidate unless C/G returns FREE

Candidate scan:

- 48..960 frames per notification
- step 48 frames
- at 192 kHz this is 0.25 ms increments from 0.25 to 5.0 ms
- NotificationCount=2

This covers the failing 240-frame / 1.25 ms geometry and common candidate boundaries including 192, 384, 480 and 960 frames per notification.

## Microsoft reference

The SysVAD `AllocateBufferWithNotification` sample requires a nonzero notification count and a requested size divisible by notification count, then aligns the size to `nBlockAlign`. The public API also reports a separate `ActualSize` output. This supports measuring accepted geometry rather than assuming the ASIO host buffer must equal the WaveRT hardware packet.

## Workflow

Main `Build ASIO B5 Productization` now builds and packages:

- `x4-asio-stage-b5-192k-geometry-probe.exe`
- `probe_b5_192k_geometry.cmd`

The geometry probe is not automatically executed by GitHub Actions because it requires the physical X4.

## Next action

1. Run manual `Build ASIO B5 Productization`.
2. Use the new ZIP.
3. Close all other X4 users.
4. Run `probe_b5_192k_geometry.cmd` once.
5. Return `B5_192K_GEOMETRY_REPORT.txt`.

Do not change the product driver geometry until the accepted 192 kHz sizes are measured.
