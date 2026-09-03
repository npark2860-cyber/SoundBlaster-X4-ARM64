# Debug history — ASIO COM Stage B4A asynchronous stop-boundary race

Date: 2026-09-04 KST

## Tested source

Branch:

`exp/windows-arm64-asio-com-stage-b4a-async-worker`

Tested implementation HEAD before smoke-only correction:

`4c8e1c4380ae5b682d03b8b12378a85c509f1149`

## Runtime result

Stage B4A did not fail in the WaveRT engine, COM object, callback worker, DMA copy, or teardown path.

The runtime demonstrated the intended asynchronous lifetime:

```text
mainThread=24792
start=0
startMessage=B4A start OK: workerThreadId=10272; callbacks continue asynchronously
B4A worker START thread=10272
startDurationMs=4.000 callbacksAtStartReturn=0 returnedBefore20=YES
```

All callback/DMA activity occurred on worker thread 10272 rather than main thread 24792.

The host observed the diagnostic target before requesting stop:

```text
callbacksBeforeStop=20 callbackThread=10272 mainThread=24792
```

Between that observation and the worker seeing the stop event, one already-arriving WaveRT notification was serviced:

```text
B4A notification=21 packet=21 writePacket=22 slot=0 ... thread=10272
B4A bufferSwitch callback=21 index=0 directProcess=0 thread=10272 ...
B4A DMA writePacket=22 slot=0 frames=512 nonzeroSamples=1024
B4A callback=21 ... copy=OK thread=10272
```

The worker then observed stop and exited before KS teardown:

```text
B4A worker STOP requested thread=10272
B4A worker EXIT thread=10272
B4A KSSTATE 2 -> OK
B4A KSSTATE 1 -> OK
B4A KSSTATE 0 -> OK
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=21 cb=21 dmaWrites=21 dmaFrames=10752
callbackStats count=21 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=21504
B4A unregister notification -> OK
disposeBuffers=0
disposeMessage=B4A disposeBuffers OK: worker absent; host + WaveRT buffers detached
dispose cleared ASIO buffer pointers=YES
DllCanUnloadNow hr=0x00000000
```

The final smoke line was:

```text
STAGE B4A ASYNC RESULT: FAIL
```

## Diagnosis

The FAIL was caused only by an invalid synchronous-style smoke assertion:

- smoke waited until callback count reached exactly 20
- it read `callbacksBeforeStop=20`
- it then called `IASIO::stop()`
- asynchronous worker legitimately completed one notification that was already available before the stop event won the wait race
- smoke incorrectly required both pre-stop and final callback counts to remain exactly 20

This is a stop-boundary observation race in the test harness, not a driver failure.

The hardware/runtime result actually proves important B4A invariants already:

- `start()` returned in 4 ms before any callback completed
- callbacks ran on one non-main worker thread
- B3B PacketCount-derived callback/DMA mapping remained intact
- 21 callback/DMA cycles completed with zero index/direct/thread errors
- worker exited before `PAUSE -> ACQUIRE -> STOP`
- notification unregister, buffer disposal and COM unload remained clean

## Correction

Do not cap the real asynchronous worker at 20 callbacks just to satisfy the diagnostic harness. That would weaken the production lifetime model.

Change only the registry-free B4A smoke acceptance rule:

1. wait until at least 20 callbacks have occurred
2. call `stop()`
3. allow notifications already in flight between the observation and stop signalling to complete
4. require `workerJoined=YES`
5. require final callback/DMA counters in `stopMessage` to match the actual final callback count
6. after `stop()` returns, wait briefly and require callback count to remain unchanged
7. preserve zero callback-index, directProcess and worker-thread errors
8. preserve clean dispose and COM unload

The corrected smoke source is:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

No driver, worker, WaveRT, packet-slot or coexistence-gate code was changed by this correction.
