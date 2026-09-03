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
8. ASIO COM Stage B3B host planar PCM -> interleaved mapped WaveRT DMA transfer

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

## Stage B3B — hardware PASS

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

The smoke generated a short low-level 440 Hz test signal, but perceptual audibility has not been explicitly reported and is not recorded as proven.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Stage B4A — asynchronous start/stop worker implemented

Branch:

`exp/windows-arm64-asio-com-stage-b4a-async-worker`

Implementation HEAD:

`4c8e1c4380ae5b682d03b8b12378a85c509f1149`

Stage B4A starts from the exact validated B3B HEAD and changes only threading/lifetime behavior.

### Runtime model

`start()` now:

1. creates a stop event
2. enters `ACQUIRE -> PAUSE -> RUN`
3. starts one Win32 worker
4. returns `ASE_OK` without waiting for the callback run to finish

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

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4A_ASYNC_IMPLEMENTED.md`.

## Immediate next action — build and run B4A idle smoke

Manual workflow:

`Build ASIO COM Stage B4A Async ARM64`

It is `workflow_dispatch` only.

Run artifact executable with X4 normal Windows playback idle:

```bat
x4-asio-stage-b4a-smoke.exe
```

Expected proof points:

```text
start=0
startMessage=B4A start OK: workerThreadId=...; callbacks continue asynchronously
startDurationMs=... callbacksAtStartReturn=... returnedBefore20=YES
B4A worker START thread=...
...
callbacksBeforeStop=20 callbackThread=... mainThread=...
B4A worker STOP requested thread=...
B4A worker EXIT thread=...
B4A KSSTATE 2 -> OK
B4A KSSTATE 1 -> OK
B4A KSSTATE 0 -> OK
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

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
