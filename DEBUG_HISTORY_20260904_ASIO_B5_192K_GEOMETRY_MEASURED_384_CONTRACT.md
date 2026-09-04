# DEBUG HISTORY — ASIO B5 192 kHz WaveRT geometry measured; 384-frame contract

Date: 2026-09-04 KST

## Context

B5 mux-v3 had already passed:

- 48 kHz / 240 output x3
- 48 kHz / 240 full duplex x2
- 96 kHz / 240 full duplex x2

The remaining full-matrix blocker was 192 kHz / 240 output failing inside `createBuffers()` before worker creation and before KSSTATE_RUN:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

A measurement-only ARM64EC probe was added to test the actual WaveRT allocation boundary without entering RUN.

## Probe safety

The geometry probe:

- targeted X4 `msft_wave`, Render Pin 1
- used 192 kHz / stereo / 24-bit PCM
- used `NotificationCount=2`
- checked local/global FREE before every `KsCreatePin`
- never entered KSSTATE_RUN
- requested only `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- closed each pin after each candidate
- required the pin gate to return FREE before proceeding

## Returned runtime evidence

Report generated `2026-09-04 13:05:25.43`.

Initial gate:

`C 0/1 G 0/1 free=YES`

Results by notification size:

- 48 frames / 0.25 ms / requested 576 bytes -> Win32 87
- 96 frames / 0.50 ms / requested 1152 bytes -> Win32 87
- 144 frames / 0.75 ms / requested 1728 bytes -> Win32 87
- 192 frames / 1.00 ms / requested 2304 bytes -> Win32 87
- 240 frames / 1.25 ms / requested 2880 bytes -> Win32 87
- 288 frames / 1.50 ms / requested 3456 bytes -> Win32 87
- 336 frames / 1.75 ms / requested 4032 bytes -> Win32 87
- **384 frames / 2.00 ms / requested 4608 bytes -> PASS, ActualBufferSize=4608**
- every tested candidate from 432 through 960 frames -> PASS

Summary:

`accepted=13 firstAcceptedFramesPerNotif=384 firstAcceptedRequestedBytes=4608`

Every accepted candidate returned `ActualBufferSize == RequestedBufferSize` and `CallMemoryBarrier=0`.

## Conclusion

The 192 kHz failure is not a generic byte-alignment failure. The measured boundary is a sample-rate-dependent minimum WaveRT notification period on the X4 Windows `msft_wave` path:

- 1.75 ms and below rejected
- 2.00 ms and above accepted in the tested range

The previous B5 assumption that a 240-frame ASIO host buffer can always map 1:1 to a WaveRT packet is therefore invalid specifically at 192 kHz.

## First-release decision

Do **not** add a host-buffer/hardware-packet ring-buffer scheduler for the first release.

Preserve the already-validated 1:1 ASIO-buffer-to-WaveRT-packet model and make the public B5 buffer contract sample-rate dependent:

48/96 kHz:

- min 96
- max 4800
- preferred 240
- granularity 48

192 kHz:

- min 384
- max 4800
- preferred 384
- granularity 48

The 512-frame compatibility exception remains accepted.

192 kHz still exposes zero capture inputs.

## Implementation

B5 branch changes:

- `driver_b5.cpp`
  - added 192 kHz min/preferred constants = 384
  - `getBufferSize()` now returns the selected-rate contract
  - `getLatencies()` uses the selected-rate preferred value before buffers exist
  - `createBuffers()` rejects values below 384 at 192 kHz before any WaveRT pin preparation
  - 48/96 kHz behavior remains unchanged
- `product_validation_b5_arm64ec.cpp`
  - validates `getBufferSize()` after `setSampleRate()`
  - 192 kHz output case changed from 240 to 384 frames
- `README_B5_PRODUCTIZATION.md`
  - documents the measured 2.0 ms boundary and sample-rate-dependent contract

No WaveRT engine, mux-v3, BUSY gate, joined-worker rule, or validated B4D source was changed.

## Next runtime gate

Rebuild `Build ASIO B5 Productization`, install the new ZIP, and run the full `install_and_validate_b5.cmd` matrix once.

Expected matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/384 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

A full PASS closes the 192 kHz allocation blocker. 96 kHz capture phase-miss quality remains a separate follow-up observation even if the matrix passes.
