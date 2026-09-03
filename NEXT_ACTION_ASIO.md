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
9. ASIO COM Stage B4A asynchronous `start()` / worker / joined `stop()` lifetime

Do not intentionally reproduce the known green-screen collision.

## Frozen hardware/streaming baseline

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

## Stage B3B — hardware PASS + audible output

Validated source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

The short low-level 440 Hz test signal was audibly confirmed through the Sound Blaster X4.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Stage B4A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

Runtime proof:

```text
start=0
startDurationMs=4.035 callbacksAtStartReturn=0 returnedBefore20=YES
callbacksBeforeStop=20 callbackThread=17304 mainThread=15264
B4A worker STOP requested thread=17304
B4A worker EXIT thread=17304
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4A_ASYNC_RUNTIME_SUCCESS.md`.

## Stage B4B — minimum host query contract implemented

Branch:

`exp/windows-arm64-asio-com-stage-b4b-host-query`

Implementation HEAD at documentation time:

`84646e7a5b8d7808e72bcf5fde545b78d34ced3c`

B4B changes only host inquiry behavior. The proven B4A WaveRT engine file is reused unchanged.

Implemented:

- native 64-bit `ASIOSamples` / `ASIOTimeStamp`
- 4-byte-packed `ASIOClockSource` / `ASIOChannelInfo`
- `getChannelInfo()` for two output channels
  - `X4 Output L`
  - `X4 Output R`
  - group 0
  - `ASIOSTInt16LSB`
  - `isActive` follows buffer lifetime
- one internal clock source, index 0
- `setClockSource(0)` success / invalid reference rejection
- `getSamplePosition()`
  - reset to 0 at start
  - callback-aligned sequence `0, 512, 1024, ...`
  - QPC-derived monotonic nanosecond timestamp
  - `ASE_SPNotAdvancing` outside active streaming

Not added yet:

- `bufferSwitchTimeInfo`
- ASIO time-info capability negotiation
- DAW registration
- MMCSS/AVRT

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4B_HOST_QUERY_IMPLEMENTED.md`.

## Immediate next action — build and run B4B

Manual workflow:

`Build ASIO COM Stage B4B Host Query ARM64`

Trigger: `workflow_dispatch` only.

Run the artifact with normal Windows playback on X4 idle:

```bat
x4-asio-stage-b4b-smoke.exe
```

Expected proof points include:

```text
getChannels=0 inputs=0 outputs=2
channel0Before hr=0 active=0 group=0 type=16 name=X4 Output L
channel1Before hr=0 active=0 group=0 type=16 name=X4 Output R
clock ... index=0 assoc=-1/-1 current=1 name=Internal set0=0 set1=-998
getSamplePosition before start=-996 expected=-996
...
B4B bufferSwitch callback=1 ... samplePosition=0 ...
B4B bufferSwitch callback=2 ... samplePosition=512 ...
...
callbackStats ... positionErrors=0 timestampErrors=0 ...
getSamplePosition after stop=-996 expected=-996
channelActiveAfterDispose=0 expected=0
STAGE B4B HOST QUERY RESULT: PASS (HOST QUERY CONTRACT + B4A ASYNC TRANSPORT)
```

A legitimate asynchronous stop-boundary overshoot above 20 callbacks remains acceptable if the final counts match and remain quiescent after joined stop.

If Windows playback owns the X4, BUSY is accepted only as a safe refusal. Never bypass BUSY.

## Still frozen

Do not add yet:

- DAW registration/testing
- `bufferSwitchTimeInfo` / ASIO time-info negotiation
- MMCSS/AVRT worker priority
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

After B4B passes, add time-info capability negotiation as the next isolated variable. Only after that should controlled registration / real DAW loading begin.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
