# DEBUG HISTORY — ASIO COM Stage B2 FREE WaveRT lifecycle runtime success

Date: 2026-09-04 KST

## Source under test

Branch:

`exp/windows-arm64-asio-com-stage-b2-wavert`

Validated source HEAD:

`a6d3201260056a46ae8bce57271132871904d6ee`

## Test state

Sound Blaster X4 had no active competing Windows playback stream.

The test remained registry-free and used the Stage B2 smoke executable.

## Runtime result

Key output:

```text
init=1
initMessage=Stage B2 init preflight FREE: C 0/1 G 0/1; engine not prepared
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B2 WaveRT BufferAddress=... ActualBufferSize=4096 CallMemoryBarrier=0
createBuffers=0
createMessage=Stage B2 WaveRT prepared: pin/buffer/event ready; not RUN yet
B2 KSSTATE 1 -> OK
B2 KSSTATE 2 -> OK
B2 KSSTATE 3 -> OK
...
B2 notification=20 packet=20 samplePosition=9680 ...
start=0
startMessage=Stage B2 RUN observed 20/20 notifications; packetDiscontinuities=0 positionRegressions=0
B2 KSSTATE 2 -> OK
B2 KSSTATE 1 -> OK
B2 KSSTATE 0 -> OK
stop=0
stopMessage=Stage B2 stop OK: RUN->PAUSE->ACQUIRE->STOP
B2 unregister notification -> OK
disposeBuffers=0
disposeMessage=Stage B2 disposeBuffers OK: event/pin/filter closed
DllCanUnloadNow hr=0x00000000
STAGE B2 COM/WAVERT RESULT: PASS (FIXED WAVERT LIFECYCLE)
```

## Hardware/runtime conclusions

The COM-integrated fixed WaveRT engine is now hardware-confirmed in the FREE state.

Confirmed path:

1. COM `init()` preflight reports local/global instance counts `0/1` and FREE.
2. A second instance-capacity gate immediately before the real pin creation again reports `0/1` and FREE.
3. Render Pin 1 creation succeeds.
4. `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION` returns the fixed 4096-byte WaveRT buffer.
5. Notification event registration succeeds.
6. `ACQUIRE -> PAUSE -> RUN` succeeds.
7. Exactly 20 notifications are observed.
8. Packet sequence remains continuous: 1 through 20.
9. Presentation position remains monotonic.
10. `RUN -> PAUSE -> ACQUIRE -> STOP` succeeds.
11. Notification unregister succeeds.
12. Event, pin and filter are closed cleanly.
13. COM unload returns `S_OK`.

This also confirms that moving the already-proven A0 WaveRT lifecycle behind the independent ARM64 ASIO COM object did not break the fixed baseline.

## Coexistence status

Stage B2 has now passed both required sides:

- BUSY: Windows X4 playback active -> COM `init()` returns false and skips pin creation.
- FREE: idle -> fixed COM-to-WaveRT lifecycle completes successfully.

The known green-screen condition must not be intentionally reproduced or bypassed.

## Next scope

Do not add audio sample transfer yet.

The next single-variable stage should validate actual ASIO host buffer/callback ABI independently of hardware-buffer writes:

- concrete `ASIOBufferInfo`
- concrete `ASIOCallbacks`
- two output channels
- fixed 512-frame double buffers
- `bufferSwitch` notification delivery during the already-proven 20-notification WaveRT run
- no writes from host buffers into the WaveRT DMA buffer yet
- keep registry-free smoke testing

This separates callback ABI risk from planar-to-interleaved audio transport risk.
