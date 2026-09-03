# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in FREE and BUSY states
6. ASIO COM Stage B2 fixed WaveRT lifecycle in FREE and BUSY states
7. ASIO COM Stage B3A host double-buffer / `bufferSwitch` ABI with zero DMA copy
8. ASIO COM Stage B3B host planar PCM -> interleaved mapped WaveRT DMA transfer with audible X4 output
9. ASIO COM Stage B4A asynchronous `start()` / worker / joined `stop()` lifetime
10. ASIO COM Stage B4B host query contract: channels, clock, block-aligned sample position/timestamp

Do not intentionally reproduce the known green-screen collision.

## Frozen hardware/streaming baseline

Keep unchanged for Stage B4C:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz stereo 16-bit PCM
- ASIO buffer size 512 frames
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- PacketCount-derived write-ahead slot
- both coexistence gates
- B4A single worker thread
- worker join before KS teardown
- no callback after joined `stop()` returns
- B4B logical sample position `0, 512, 1024, ...`
- QPC-derived monotonic nanosecond timestamp

## Stage B3B — hardware PASS + audible output

Validated source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

The low-level 440 Hz smoke signal was audibly confirmed through the Sound Blaster X4.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Stage B4A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

Key proof:

```text
startDurationMs=4.035 callbacksAtStartReturn=0 returnedBefore20=YES
callbacksBeforeStop=20 callbackThread=17304 mainThread=15264
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4A_ASYNC_RUNTIME_SUCCESS.md`.

## Stage B4B — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b4b-host-query@84646e7a5b8d7808e72bcf5fde545b78d34ced3c`

Runtime proof:

```text
ABI ASIOSamples=8 ASIOTimeStamp=8 ASIOClockSource=48/4 ASIOChannelInfo=52/4 ASIOBufferInfo=24/4
getChannels=0 inputs=0 outputs=2
channel0Before hr=0 active=0 group=0 type=16 name=X4 Output L
channel1Before hr=0 active=0 group=0 type=16 name=X4 Output R
clock countHr=0 count=1 fillHr=0 index=0 assoc=-1/-1 current=1 name=Internal set0=0 set1=-998
getSamplePosition before start=-996 expected=-996
...
B4B bufferSwitch callback=1 ... samplePosition=0 ...
B4B bufferSwitch callback=2 ... samplePosition=512 ...
...
B4B bufferSwitch callback=20 ... samplePosition=9728 ...
stopMessage=B4B stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 positionErrors=0 timestampErrors=0 hostSampleWrites=20480 lastPosition=9728
getSamplePosition after stop=-996 expected=-996
channelActiveAfterDispose=0 expected=0
STAGE B4B HOST QUERY RESULT: PASS (HOST QUERY CONTRACT + B4A ASYNC TRANSPORT)
```

Hardware-proven B4B invariants:

- two deterministic output channels report `ASIOSTInt16LSB`
- `isActive` tracks buffer lifetime
- one internal clock source works
- `getSamplePosition()` returns `ASE_SPNotAdvancing` outside RUN
- during RUN, logical block-start sample position advances exactly 512 frames per callback
- timestamp is monotonic
- B4A worker/DMA lifetime remains intact

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4B_HOST_QUERY_RUNTIME_SUCCESS.md`.

## Stage B4C — ASIO 2.x time-info implemented; build/runtime pending

Branch:

`exp/windows-arm64-asio-com-stage-b4c-time-info`

B4C starts from the exact validated B4B HEAD and changes only the ASIO 2.x time-info callback contract.

Implemented:

- pack-4 `AsioTimeInfo`, `ASIOTimeCode`, `ASIOTime`
- `ASIOFuture(kAsioCanTimeInfo) -> ASE_SUCCESS`
- at `createBuffers()`, driver calls host `asioMessage(kAsioSupportsTimeInfo)`
- if host returns 1 and supplies `bufferSwitchTimeInfo`, callbacks switch from legacy `bufferSwitch()` to `bufferSwitchTimeInfo()`
- `ASIOTime.timeInfo` carries:
  - B4B logical block-start sample position
  - B4B QPC-derived nanosecond timestamp
  - 48000 Hz sample rate
  - speed 1.0
  - system-time/sample-position/sample-rate/speed valid flags
- time code remains invalid/unused
- legacy `bufferSwitch` remains present only as fallback
- B4A `wavert_engine_b4a.cpp` remains unchanged

Registry-free B4C smoke additionally requires:

- time-info negotiation exactly once
- `future(kAsioCanTimeInfo)` success
- `bufferSwitchTimeInfo` used for every streaming callback
- legacy `bufferSwitch` count remains zero after negotiation
- ASIOTime sample position/timestamp exactly match `getSamplePosition()` inside the callback
- zero time-info/position/timestamp/consistency errors
- existing DMA/worker/stop invariants remain intact

Manual workflow:

`Build ASIO COM Stage B4C Time Info ARM64`

Trigger: `workflow_dispatch` only.

After a successful build, run with normal Windows playback on X4 idle:

```bat
x4-asio-stage-b4c-smoke.exe
```

Expected proof points:

```text
ABI ASIOTimeInfo=48/4 ASIOTimeCode=84/4 ASIOTime=148/4 ASIOCallbacks=32/4
future(kAsioCanTimeInfo)=1061701536 expected=1061701536
B4C host asioMessage kAsioSupportsTimeInfo -> 1
createMessage=B4C buffers ready timeInfo=YES; ...
timeInfoNegotiationCalls=1
startMessage=B4C start OK ... timeInfo=YES position=0
B4C bufferSwitchTimeInfo callback=1 index=0 ... samplePosition=0 ...
B4C bufferSwitchTimeInfo callback=2 index=1 ... samplePosition=512 ...
...
callbacksBeforeStop=20 legacyCallbacks=0 ...
stopMessage=B4C stop OK workerJoined=YES ...
callbackStats timeInfo=20 quiescentAfterStop=20 legacy=0 ... timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 ...
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

A legitimate asynchronous stop-boundary overshoot above 20 remains acceptable if final counts match and remain quiescent after joined stop.

If Windows playback owns X4, BUSY remains a safe refusal. Never bypass BUSY.

## Still frozen

Do not add yet:

- actual ASIO registry registration / REAPER load
- MMCSS/AVRT worker priority
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- time code
- Creative runtime dependencies
- custom kernel driver

After B4C hardware PASS, the next controlled milestone is driver registration and the first real REAPER load/playback test at the frozen 48 kHz / 16-bit / stereo / 512-frame configuration.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
