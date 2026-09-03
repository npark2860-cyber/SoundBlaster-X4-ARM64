# DEBUG HISTORY — ASIO WaveRT Active Stage 1

Date: 2026-09-03 KST

Branch: `diag/windows-arm64-asio-wavert-active`

## Scope

Hardware runtime validation of the X4 `msft_wave` render path using standard KS/WaveRT only.

No Creative DLLs were loaded.
No driver or registry changes were made.
No CTCDC or Direct Mode command was sent.
The render buffer was filled with silence.

## Hardware-confirmed results

Device/filter:

- Sound Blaster X4
- HWID `USB\\VID_041E&PID_3278&MI_03`
- KS filter suffix `msft_wave`
- Render Pin 1
- 48 kHz / 2 channel / 16-bit PCM

Observed:

- `KsCreatePin` succeeded: `0x00000000`
- `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` GET succeeded
- driver returned a valid user-mode cyclic buffer address
- actual buffer size = `4096` bytes
- `CallMemoryBarrier = 0`
- `KSPROPERTY_RTAUDIO_HWLATENCY` GET succeeded
- returned latency fields were all zero in this runtime state
- `KSPROPERTY_RTAUDIO_POSITIONREGISTER` GET failed with Win32 `50` (`ERROR_NOT_SUPPORTED`)

## Interpretation

This is a partial success, not an ASIO/WaveRT failure.

The critical DMA-capable WaveRT cyclic buffer was actually allocated in user mode. That is stronger evidence than the earlier BASICSUPPORT-only probe.

The position-register failure is compatible with a device/driver stack that does not expose a directly memory-mappable hardware position register. Microsoft documents that `KSPROPERTY_RTAUDIO_POSITIONREGISTER` can fail when such a register is not available, and clients can use other position mechanisms instead.

Do not infer from BASICSUPPORT alone that the hardware register mapping itself must succeed.

## Next action

Run Active Stage 2 v2 with the same render pin and buffer setup, but:

1. treat `POSITIONREGISTER` and `CLOCKREGISTER` as optional;
2. continue to register the WaveRT DMA notification event;
3. enter `ACQUIRE -> PAUSE -> RUN` with silence;
4. verify repeated notification events;
5. log position through available fallbacks:
   - `KSPROPERTY_AUDIO_POSITION`
   - `KSPROPERTY_RTAUDIO_PACKETCOUNT`
   - `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION`
6. stop cleanly and unregister the event.

The primary success criterion for Stage 2 is repeated DMA notification delivery while the WaveRT pin is in RUN state.
