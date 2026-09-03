# DEBUG HISTORY — ASIO WaveRT active runtime success

Updated: 2026-09-03 KST

## Scope

Hardware runtime validation of the Windows ARM64 Sound Blaster X4 `msft_wave` KS/WaveRT path using a read/write user-mode probe. This test does not use Creative ASIO binaries.

Target device:

- Sound Blaster X4 / SB1815
- USB VID/PID `041E:3278`
- MI_03 / Microsoft USB Audio 2.0 path
- KS filter path suffix `\msft_wave`

Test stream:

- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM
- silence only
- WaveRT notification buffer requested with size 4096 bytes
- notification count = 2

## Hardware-confirmed results

`KsCreatePin` succeeded:

`0x00000000`

`KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` succeeded.

Driver returned:

- non-null user-mode buffer address
- `ActualBufferSize = 4096`
- `CallMemoryBarrier = 0`

`KSPROPERTY_RTAUDIO_HWLATENCY` succeeded.

Observed values:

- FIFO size = 0
- chipset delay = 0
- codec delay = 0

`KSPROPERTY_RTAUDIO_POSITIONREGISTER` returned Win32 50 / `ERROR_NOT_SUPPORTED`.

`KSPROPERTY_RTAUDIO_CLOCKREGISTER` returned Win32 50 / `ERROR_NOT_SUPPORTED`.

These two failures did not prevent active streaming.

Notification event registration succeeded:

`REGISTER_NOTIFICATION_EVENT SET -> OK`

KS state transitions succeeded:

- `KSSTATE_ACQUIRE` -> OK
- `KSSTATE_PAUSE` -> OK
- `KSSTATE_RUN` -> OK

The probe then received all 20 requested DMA notification events.

`KSPROPERTY_RTAUDIO_PACKETCOUNT` advanced monotonically from 1 through 20.

`KSPROPERTY_RTAUDIO_PRESENTATION_POSITION` succeeded during RUN.

Observed `PositionInBlocks` sequence began:

- 0
- 464
- 976
- 1488
- 2000
- 2512
- 3024
- 3536
- ...
- 9680

After startup, the position advanced by 512 frames per notification.

At 48 kHz:

`512 / 48000 = 10.666666... ms`

The requested 4096-byte stereo 16-bit buffer contains 1024 frames total. With notification count 2, half-buffer size is exactly 512 frames. The observed presentation-position cadence therefore matches the expected half-buffer notification cadence.

`PresentationQpc` also advanced on each notification.

`KSPROPERTY_AUDIO_POSITION` did not return usable values in this probe (`n/a`), but that was not required because packet count and presentation position were both available and advancing.

Shutdown succeeded:

- `KSSTATE_PAUSE` -> OK
- `KSSTATE_ACQUIRE` -> OK
- `KSSTATE_STOP` -> OK
- `UNREGISTER_NOTIFICATION_EVENT SET` -> OK

## Conclusion

The active WaveRT DMA notification path is hardware-confirmed on the user's Windows ARM64 X4 environment.

Confirmed user-mode primitives now include:

1. X4 KS filter discovery
2. Render streaming pin creation
3. WaveRT cyclic buffer allocation
4. notification event registration
5. successful ACQUIRE / PAUSE / RUN / STOP transitions
6. periodic DMA notification delivery
7. monotonically advancing packet count
8. monotonically advancing presentation position/QPC
9. clean event unregister and stream shutdown

This removes the central technical uncertainty for implementing an independent ARM64 ASIO buffer-switch engine on top of the Microsoft USB Audio 2.0 KS/WaveRT path.

## Important nuance

Do not claim that hardware-mapped position or clock registers are available on this path. They returned `ERROR_NOT_SUPPORTED` in the active probe.

A new implementation should use the working WaveRT/KS position mechanisms demonstrated here, especially packet count and presentation position, unless later analysis of Creative's ASIO driver proves a different fallback path.

## Next step

Move from feasibility probing to an ASIO engine prototype:

- render-only first
- fixed 48 kHz / stereo / 16-bit initially
- two ASIO buffers mapped onto the two halves of the 4096-byte WaveRT cyclic buffer
- one callback per DMA notification
- derive sample position from presentation position / packet count
- no capture, sample-rate switching, or multichannel support in the first prototype

Only after stable render callbacks are proven should capture Pin 4 and full-duplex synchronization be added.
