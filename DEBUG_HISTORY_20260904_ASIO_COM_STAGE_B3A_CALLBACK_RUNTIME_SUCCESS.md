# ASIO COM Stage B3A callback ABI — runtime success

Date: 2026-09-04 KST

Validated source branch:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi`

Validated source HEAD:

`46c22ef00f85f3668d6851844fa1558d250cedb8`

## Purpose

Stage B3A adds only the ASIO host-facing double-buffer callback ABI on top of the already hardware-proven Stage B2 WaveRT lifecycle.

No ASIO host samples are copied into the WaveRT DMA buffer in this stage.

## Runtime result

Hardware/runtime output:

```text
Sound Blaster X4 ARM64 ASIO Stage B3A callback ABI smoke
SAFETY: registry-free; B2 WaveRT lifecycle unchanged; ASIO callbacks added; host samples are NOT copied to WaveRT DMA.
ABI sizeof(ASIOBufferInfo)=24 align=4 sizeof(ASIOCallbacks)=32 align=4
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
init=1
initMessage=Stage B3A init preflight FREE: C 0/1 G 0/1; callback buffers not created
driverName=Sound Blaster X4 ARM64
driverVersion=103
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B2 WaveRT BufferAddress=... ActualBufferSize=4096 CallMemoryBarrier=0
createBuffers=0
createMessage=Stage B3A buffers ready: 2ch x 2 x 512 planar int16; WaveRT prepared; DMA copy disabled
ASIO buffers ... distinctNonNull=YES
B2 KSSTATE 1 -> OK
B2 KSSTATE 2 -> OK
B2 KSSTATE 3 -> OK
...
B3A bufferSwitch callback=1 index=0 directProcess=0
B3A bufferSwitch callback=2 index=1 directProcess=0
...
B3A bufferSwitch callback=20 index=1 directProcess=0
start=0
startMessage=Stage B3A RUN notifications=20 callbacks=20 callbackIndexErrors=0 hardwareBufferWrites=0
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=40 hardwareBufferWrites=0
B2 KSSTATE 2 -> OK
B2 KSSTATE 1 -> OK
B2 KSSTATE 0 -> OK
stop=0
stopMessage=Stage B3A stop OK: RUN->PAUSE->ACQUIRE->STOP
B2 unregister notification -> OK
disposeBuffers=0
disposeMessage=Stage B3A disposeBuffers OK: host buffers detached; event/pin/filter closed
dispose cleared ASIO buffer pointers=YES
DllCanUnloadNow hr=0x00000000
STAGE B3A CALLBACK RESULT: PASS (ASIO CALLBACK ABI, NO DMA COPY)
```

## Hardware/runtime-established facts

1. The ASIO public ABI pack-4 layout is valid on native Windows ARM64 for the structures used here:
   - `sizeof(ASIOBufferInfo)=24`, `alignof=4`
   - `sizeof(ASIOCallbacks)=32`, `alignof=4`
2. COM initialization still reports FREE only at C `0/1`, G `0/1`.
3. The second pre-pin gate remains FREE immediately before real `KsCreatePin`.
4. Four host-side ASIO double-buffer pointers are non-null and distinct.
5. The host callback is invoked exactly 20 times for the 20 WaveRT notifications.
6. Callback indices alternate 0/1 without error.
7. `directProcess` remains false for all 20 diagnostic callbacks.
8. The smoke host successfully performs 40 writes to host-side ASIO buffers.
9. Stage B3A performs zero writes to the WaveRT DMA buffer during RUN.
10. The proven B2 packet sequence, monotonic presentation-position behavior, KS state ordering, notification unregister, handle close, and COM unload remain clean.

## Consequence

The host-facing ASIO double-buffer and `bufferSwitch` ABI layer is now independently validated without mixing in the next risk: host-sample transfer into the cyclic WaveRT hardware buffer.

## Next variable

Stage B3B may add only planar 16-bit stereo host-buffer to interleaved WaveRT packet copy.

For NotificationCount=2 and a 4096-byte WaveRT buffer at stereo 16-bit, each WaveRT packet is exactly 2048 bytes = 512 frames, which matches the fixed ASIO buffer size.

The packet target must be derived from `KSPROPERTY_RTAUDIO_PACKETCOUNT`, not merely from callback parity. Microsoft documents PacketCount as the count of packets completely transferred; when packet N is currently in progress, the client writes ahead to packet N+1. With NotificationCount=2, target slot is `(PacketCount + 1) % 2`.

Do not add production callback threading, sample-rate expansion, 24-bit, multichannel, capture, or DAW registration in the same experiment.
