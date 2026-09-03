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

Keep unchanged for the next experiment:

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
- host callback then planar -> interleaved mapped-buffer copy
- asynchronous single worker thread
- `stop()` joins worker before `RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister notification event before closing handles

## Stage B3B — hardware PASS + audible output

Validated source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

The low-level 440 Hz smoke signal was audibly confirmed through the Sound Blaster X4.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Stage B4A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

Corrected runtime proof:

```text
mainThread=15264
start=0
startMessage=B4A start OK: workerThreadId=17304; callbacks continue asynchronously
startDurationMs=4.035 callbacksAtStartReturn=0 returnedBefore20=YES
B4A worker START thread=17304
...
callbacksBeforeStop=20 callbackThread=17304 mainThread=15264
B4A worker STOP requested thread=17304
B4A worker EXIT thread=17304
B4A KSSTATE 2 -> OK
B4A KSSTATE 1 -> OK
B4A KSSTATE 0 -> OK
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

Hardware-proven B4A invariants:

- `start()` returns before callback processing completes
- callback/DMA work occurs on one worker thread distinct from main
- B3B PacketCount-derived DMA mapping is preserved
- worker is joined before KS state teardown
- no callback occurs after joined `stop()` returns
- cleanup and COM unload remain clean

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4A_ASYNC_RUNTIME_SUCCESS.md`.

## Immediate next stage — B4B minimum host query contract

Create a new branch from exact validated B4A HEAD `996025332bf17341b584095260c1abec93222d84`.

Do not change the proven streaming/lifetime path.

Add only the minimum host-facing query contract that real ASIO hosts commonly inspect before/around buffer creation:

1. concrete 4-byte-packed `ASIOClockSource`, `ASIOChannelInfo`, `ASIOSamples`, and `ASIOTimeStamp` definitions matching the Windows ASIO ABI
2. `getChannelInfo()` for output channels 0 and 1
   - output only
   - channel group 0
   - sample type `ASIOSTInt16LSB`
   - deterministic channel names
   - active flag reflects whether buffers currently exist
3. `getClockSources()` returns one current internal clock source
   - index 0
   - no associated channel/group (`-1/-1`)
4. `setClockSource(0)` succeeds; any other reference returns `ASE_InvalidParameter`
5. `getSamplePosition()`
   - reset logical position to 0 at `start()`
   - advance exactly 512 frames per successful host callback
   - return a monotonic block-aligned sample position
   - return a monotonic timestamp associated with the current logical block
   - after the engine is not advancing, return `ASE_SPNotAdvancing`
6. registry-free smoke must validate metadata before streaming and sample-position advancement during the existing B4A worker run

This stage is inquiry/metadata only. Do not add `bufferSwitchTimeInfo` negotiation yet; keep the callback transport unchanged so any regression is attributable to the new query contract only.

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

After B4B passes, the next logical step is time-info capability negotiation (`bufferSwitchTimeInfo`) and then controlled driver registration / real DAW loading.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
