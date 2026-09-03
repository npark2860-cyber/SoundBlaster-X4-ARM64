# DEBUG HISTORY — ASIO Engine Stage A0 runtime success

Updated: 2026-09-03 KST

## Purpose

After the native Stage A prototype triggered `WDF_VIOLATION (0x10D)`, Stage A0 reduced the native ARM64 WaveRT path back to the exact hardware-proven shape:

- one filter/pin open only
- Render Pin 1
- 48 kHz / stereo / 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- notification count = 2
- zero the full DMA buffer once before RUN
- no writes to the WaveRT buffer during RUN
- 20 notifications only
- observe `PACKETCOUNT` and `PRESENTATION_POSITION`
- one clean `RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister event and close all handles once

## Hardware runtime result

The user ran the native ARM64 Stage A0 executable successfully.

Observed:

- `KsCreatePin` succeeded
- `BUFFER_WITH_NOTIFICATION` succeeded
- buffer zeroing before RUN completed
- notification event registration succeeded
- `KSSTATE_ACQUIRE` succeeded
- `KSSTATE_PAUSE` succeeded
- `KSSTATE_RUN` succeeded
- 20/20 notifications were received
- packet count advanced exactly `1..20`
- presentation position advanced monotonically
- cleanup state transitions all succeeded
- notification unregister succeeded
- event/pin/filter handles closed cleanly

Final invariants:

- `notifications=20`
- `packet_discontinuities=0`
- `position_regressions=0`
- `STAGE A0 RESULT: PASS`

## What this rules out

The previous `0x10D` crash was not caused by the basic native ARM64 ABI or by the single-lifecycle KS/WaveRT path itself.

Hardware-confirmed safe in native ARM64 code:

- SetupAPI X4 discovery
- opening `msft_wave`
- `KsCreatePin`
- the ARM64 structure layouts used by the Stage A0 requests
- WaveRT notification-buffer allocation
- notification event registration
- KS state transitions
- `PACKETCOUNT`
- `PRESENTATION_POSITION`
- one full unregister/close lifecycle

## Remaining crash differential

The crashed Stage A added two meaningful behaviors beyond Stage A0:

1. repeated open/run/stop/close lifecycles (three complete cycles), and
2. writing the logical half-buffer during RUN on every notification.

Do not combine these variables again.

Because the crash dump reported `WDF_VIOLATION 0x10D` with parameter 1 = `0x5` (wrong WDF object handle type), repeated teardown/reopen is the first variable to isolate. Runtime DMA writes are tested only after repeated lifecycle behavior is cleared.

## Next experiment

Stage A0-R2:

- exactly two complete Stage A0 lifecycles
- 20 notifications per lifecycle
- no writes during RUN
- same format and WaveRT geometry

If A0-R2 passes, repeated lifecycle is safe at two cycles and the next step is a controlled third cycle or runtime-buffer-write isolation.

If A0-R2 reproduces the WDF violation, the failure is in teardown/reopen behavior rather than host-buffer writes.
