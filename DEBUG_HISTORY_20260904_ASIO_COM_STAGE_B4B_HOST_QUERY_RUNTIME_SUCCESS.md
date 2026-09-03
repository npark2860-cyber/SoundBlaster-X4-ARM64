# DEBUG HISTORY — ASIO COM Stage B4B host query runtime success

Date: 2026-09-04 KST

## Validated source

`exp/windows-arm64-asio-com-stage-b4b-host-query@84646e7a5b8d7808e72bcf5fde545b78d34ced3c`

Stage B4B starts from the validated B4A asynchronous transport and adds only the minimum ASIO host inquiry contract. The WaveRT/worker transport remains the B4A path.

## Hardware runtime result

Registry-free native ARM64 smoke result:

```text
ABI ASIOSamples=8 ASIOTimeStamp=8 ASIOClockSource=48/4 ASIOChannelInfo=52/4 ASIOBufferInfo=24/4
init=1
driverVersion=106
getChannels=0 inputs=0 outputs=2
channel0Before hr=0 active=0 group=0 type=16 name=X4 Output L
channel1Before hr=0 active=0 group=0 type=16 name=X4 Output R
clock countHr=0 count=1 fillHr=0 index=0 assoc=-1/-1 current=1 name=Internal set0=0 set1=-998
getSamplePosition before start=-996 expected=-996
createBuffers=0
ASIO buffers distinctNonNull=YES channelActiveAfterCreate=YES
start=0
startDurationMs=3.995 callbacksAtStartReturn=0 returnedBefore20=YES
...
B4B bufferSwitch callback=1 ... samplePosition=0 ...
B4B bufferSwitch callback=2 ... samplePosition=512 ...
...
B4B bufferSwitch callback=20 ... samplePosition=9728 ...
callbacksBeforeStop=20 callbackThread=25124 mainThread=12752
stop=0
stopMessage=B4B stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 positionErrors=0 timestampErrors=0 hostSampleWrites=20480 lastPosition=9728
getSamplePosition after stop=-996 expected=-996
disposeBuffers=0
channelActiveAfterDispose=0 expected=0
DllCanUnloadNow hr=0x00000000
STAGE B4B HOST QUERY RESULT: PASS (HOST QUERY CONTRACT + B4A ASYNC TRANSPORT)
```

## Hardware-proven conclusions

- ARM64 ASIO public query structures match the expected Windows pack-4 ABI.
- Two output channels report deterministic metadata and `ASIOSTInt16LSB`.
- channel `isActive` changes from 0 before buffers, to 1 after `createBuffers()`, back to 0 after dispose.
- one internal clock source is exposed; source 0 succeeds and an invalid reference is rejected.
- `getSamplePosition()` correctly returns `ASE_SPNotAdvancing` before RUN and after STOP.
- during RUN, ASIO logical sample position is block-aligned and advances exactly 512 frames per callback: `0, 512, ... 9728`.
- timestamps are monotonic and the smoke reports zero position/timestamp errors.
- the B4A asynchronous lifetime remains intact: `start()` returns before callback processing, work occurs on a distinct worker thread, worker is joined before KS STOP, and no callback occurs after joined stop.
- B3B PCM-to-WaveRT DMA transfer remains intact with 20 writes / 10240 frames and clean teardown.

The WaveRT presentation-position telemetry (`0, 464, 976, ...`) is intentionally kept distinct from the host-facing block-start ASIO logical sample position. Stage B4B does not redefine the hardware telemetry.

## Result

**PASS.** Stage B4B is the validated baseline for the next host-facing ASIO 2.x capability step.
