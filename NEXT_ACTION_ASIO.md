# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in FREE and BUSY states
6. ASIO COM Stage B2 fixed WaveRT lifecycle in FREE and BUSY states
7. ASIO COM Stage B3A host double-buffer / `bufferSwitch` ABI with zero DMA sample copy
8. ASIO COM Stage B3B host planar PCM -> interleaved mapped WaveRT DMA transfer with audible X4 output

Do not intentionally reproduce the known green-screen collision.

## Frozen hardware/format baseline

Keep unchanged for Stage B4A:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz stereo 16-bit PCM
- ASIO buffer size 512 frames
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- 2048 bytes / 512 stereo frames per packet
- coexistence gate at COM `init()`
- second coexistence gate immediately before every real `KsCreatePin`
- `writePacket = PacketCount + 1`
- `slot = writePacket % 2`
- host `bufferSwitch(slot, ASIOFalse)` then planar -> interleaved mapped-buffer copy
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister notification event before closing handles

## Stage B3A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi@46c22ef00f85f3668d6851844fa1558d250cedb8`

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3A_CALLBACK_RUNTIME_SUCCESS.md`.

## Stage B3B — hardware PASS + audible output confirmed

Validated source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

Runtime summary:

```text
start=0
startMessage=B3B RUN notif=20 cb=20 dmaWrites=20 dmaFrames=10240 nonzero=20444
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=20480
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B3B DMA COPY RESULT: PASS (HOST PCM COPIED TO WAVERT DMA)
```

This hardware-proves the independent native ARM64 path can transfer host ASIO PCM into the X4 mapped WaveRT render buffer with clean packet/slot synchronization and teardown.

The tester explicitly confirmed that the short low-level 440 Hz stereo smoke tone was audible through the Sound Blaster X4. B3B therefore has both structured DMA/packet proof and audible end-to-end render confirmation.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Stage B4A — asynchronous lifetime working; smoke boundary race diagnosed

Branch:

`exp/windows-arm64-asio-com-stage-b4a-async-worker`

Driver/worker implementation remains the original B4A implementation. The corrected registry-free smoke HEAD is:

`996025332bf17341b584095260c1abec93222d84`

Stage B4A starts from the exact validated B3B HEAD and changes only threading/lifetime behavior.

### Runtime model

`start()`:

1. creates a stop event
2. enters `ACQUIRE -> PAUSE -> RUN`
3. starts one Win32 worker
4. returns `ASE_OK` without waiting for callback completion

Worker:

- waits on stop event + WaveRT notification event
- reads PacketCount and presentation position
- preserves B3B packet/slot mapping
- invokes host `bufferSwitch`
- performs the same 512-frame planar -> interleaved WaveRT copy

`stop()`:

1. signals stop event
2. joins worker
3. if join fails, hardware teardown is explicitly withheld
4. only after join succeeds performs `RUN -> PAUSE -> ACQUIRE -> STOP`

`disposeBuffers()` does not close event/pin/filter while a worker remains unjoined.

No MMCSS/AVRT priority is added yet.

### First hardware run — engine/lifetime succeeded, old smoke assertion failed

The first B4A hardware run proved the async architecture itself:

```text
mainThread=24792
start=0
startMessage=B4A start OK: workerThreadId=10272; callbacks continue asynchronously
startDurationMs=4.000 callbacksAtStartReturn=0 returnedBefore20=YES
B4A worker START thread=10272
...
callbacksBeforeStop=20 callbackThread=10272 mainThread=24792
```

Before `stop()`'s stop event won the worker wait, one already-arriving notification completed. Final safe state was:

```text
B4A callback=21 ... copy=OK thread=10272
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
DllCanUnloadNow hr=0x00000000
```

The old smoke printed `FAIL` only because it incorrectly required exactly 20 callbacks both before and after `stop()`.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4A_ASYNC_STOP_BOUNDARY_RACE.md`.

### Corrected B4A smoke invariant

Do not artificially stop the real asynchronous worker at callback 20.

The corrected smoke now:

1. waits until at least 20 callbacks have occurred
2. requests `stop()`
3. permits any notification already in flight before stop signalling to finish
4. requires `workerJoined=YES`
5. requires the final `cb`, `dmaWrites`, and `dmaFrames` values reported by the driver to match the actual final callback count
6. waits 50 ms after `stop()` returns and requires callback count to remain unchanged
7. requires zero callback-index, directProcess and thread errors
8. requires clean buffer disposal and COM unload

Only `smoke_b4a.cpp` changed for this correction. Driver/worker/WaveRT code was not modified.

## Immediate next action — rebuild and rerun B4A smoke

Manual workflow:

`Build ASIO COM Stage B4A Async ARM64`

It is `workflow_dispatch` only and checks out the latest `exp/windows-arm64-asio-com-stage-b4a-async-worker` branch.

Run with normal Windows X4 playback idle:

```bat
x4-asio-stage-b4a-smoke.exe
```

Expected proof points now allow a legitimate stop-boundary overshoot such as 20 -> 21:

```text
start=0
startDurationMs=... callbacksAtStartReturn=... returnedBefore20=YES
callbacksBeforeStop=20 callbackThread=<worker> mainThread=<different>
...
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=21 cb=21 dmaWrites=21 dmaFrames=10752
callbackStats count=21 quiescentAfterStop=21 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=21504
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

The exact final count need not be 21; the required invariant is that it is at least the target count, all driver counters match it, and it remains quiescent after joined stop returns.

If Windows playback owns the X4, BUSY must still be accepted only as a safe refusal. Never bypass BUSY.

## Still frozen

Do not add yet:

- DAW registration/testing
- MMCSS/AVRT worker priority
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

After B4A passes, complete the remaining host-facing ASIO contract required by real DAWs (`getChannelInfo`, sample-position/time reporting, clock-source behavior and capability negotiation as required) before registration/DAW testing.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
