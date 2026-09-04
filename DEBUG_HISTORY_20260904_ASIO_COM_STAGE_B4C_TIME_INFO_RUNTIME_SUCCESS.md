# Stage B4C ASIO 2.x Time-Info — Hardware Runtime Success

Date: 2026-09-04 KST

Validated source branch:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

## Result

Stage B4C is hardware-PASS.

The corrected registry-free smoke completed the FREE path with the Sound Blaster X4 idle and proved the ASIO 2.x time-info callback contract while preserving the already-validated B4A/B4B transport and lifetime behavior.

## Runtime proof

Key log lines:

```text
init=1
initMessage=B4C init FREE: C 0/1 G 0/1; ASIO2 time-info available
driverVersion=107
getChannels=0 inputs=0 outputs=2
future(kAsioCanTimeInfo)=1061701536 expected=1061701536
getSamplePosition before start=-996 expected=-996
B4A PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B4C host asioMessage kAsioSupportsTimeInfo -> 1
createBuffers=0
createMessage=B4C buffers ready timeInfo=YES; B4B query + B4A PCM preserved
timeInfoNegotiationCalls=1
ASIO buffers distinctNonNull=YES negotiation=YES
start=0
startMessage=B4C start OK workerThreadId=3620 timeInfo=YES position=0
startDurationMs=4.041 timeInfoCallbacksAtStartReturn=0 returnedBefore20=YES
...
B4C bufferSwitchTimeInfo callback=1 index=0 ... samplePosition=0 ...
B4C bufferSwitchTimeInfo callback=2 index=1 ... samplePosition=512 ...
...
B4C bufferSwitchTimeInfo callback=20 index=1 ... samplePosition=9728 ...
callbacksBeforeStop=20 legacyCallbacks=0 callbackThread=3620 mainThread=6128
stop=0
stopMessage=B4C stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats timeInfo=20 quiescentAfterStop=20 legacy=0 indexErrors=0 directProcessErrors=0 threadErrors=0 timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES hostSampleWrites=20480 lastPosition=9728
getSamplePosition after stop=-996 expected=-996
disposeBuffers=0
disposeMessage=B4C disposeBuffers OK: time-info detached; buffers closed
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

## Hardware-proven invariants

- `ASIOFuture(kAsioCanTimeInfo)` returns `ASE_SUCCESS`.
- Host negotiation via `asioMessage(kAsioSupportsTimeInfo)` succeeds exactly once.
- All streaming callbacks use `bufferSwitchTimeInfo()` after successful negotiation.
- Legacy `bufferSwitch()` callback count remains zero.
- `ASIOTime.timeInfo.samplePosition` advances exactly 512 frames per callback: `0, 512, 1024, ...`.
- `ASIOTime.timeInfo.systemTime` is positive, never regresses, and advances during the run.
- Equal adjacent `timeGetTime()`-derived timestamps are accepted as legitimate coarse-timer behavior.
- `getSamplePosition()` returns the same position/timestamp pair exposed in the current `ASIOTime` callback.
- Zero time-info, position, timestamp-regression, consistency, callback-index, direct-process and thread errors.
- Host wrote 20,480 samples for 20 callbacks.
- WaveRT DMA copied 20 blocks / 10,240 stereo frames.
- Worker thread joined before KS teardown.
- No callback occurred after joined stop.
- Post-stop sample position correctly reports `ASE_SPNotAdvancing`.
- Notification registration, buffer disposal and COM unload completed cleanly.

## Safety

The Creative-equivalent coexistence gate remains intact:

- idle: FREE path allowed
- active Windows playback: BUSY path refuses before `KsCreatePin`

Never bypass BUSY and never intentionally reproduce the old ungated active-playback green-screen collision.

## Stage conclusion

Stage B4C closes the ASIO 2.x host time-info requirement for the current frozen format:

- 48 kHz
- stereo output
- signed int16
- 512-frame ASIO buffers

Next isolated milestone: controlled ASIO registration and the first native ARM64 REAPER load/playback test. No transport, format, capture, MMCSS, or WaveRT changes should be mixed into that stage.
