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

Keep unchanged for the next experiment:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT cyclic buffer
- NotificationCount=2
- 512 frames / 2048 bytes per WaveRT packet
- ASIO buffer size 512 frames
- coexistence gate at COM `init()`
- second coexistence gate immediately before every real `KsCreatePin`
- PacketCount-derived write-ahead mapping
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister notification event before closing handles

## Stage B3A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi@46c22ef00f85f3668d6851844fa1558d250cedb8`

Confirmed host-side callback ABI:

```text
callbacks=20
indexErrors=0
directProcessErrors=0
hardwareBufferWrites=0
STAGE B3A CALLBACK RESULT: PASS (ASIO CALLBACK ABI, NO DMA COPY)
```

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3A_CALLBACK_RUNTIME_SUCCESS.md`.

## Stage B3B — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

B3B used the same ASIO callback index as the PacketCount-derived safe WaveRT target slot:

```text
writePacket = PacketCount + 1
slot = writePacket % 2
```

Runtime-confirmed beginning:

```text
packet=1 -> writePacket=2 -> slot=0
bufferSwitch index=0
DMA copy slot=0 frames=512 copy=OK

packet=2 -> writePacket=3 -> slot=1
bufferSwitch index=1
DMA copy slot=1 frames=512 copy=OK
```

Final counters:

```text
start=0
startMessage=B3B RUN notif=20 cb=20 dmaWrites=20 dmaFrames=10240 nonzero=20444
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=20480
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B3B DMA COPY RESULT: PASS (HOST PCM COPIED TO WAVERT DMA)
```

This proves the native ARM64 path can transfer host ASIO PCM into the X4's mapped WaveRT render buffer while preserving packet/slot synchronization and clean teardown.

The smoke was designed to generate a short low-level 440 Hz tone, but perceptual audibility was not explicitly reported with the runtime output. Do not record tone audibility as hardware-proven until the tester explicitly reports it.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3B_DMA_RUNTIME_SUCCESS.md`.

## Immediate next stage — B4A asynchronous start/stop lifetime

The next missing architectural behavior is the ASIO runtime lifetime model.

Current B3B `IASIO::start()` performs the 20-notification loop synchronously and returns only after it finishes. A real ASIO host requires `start()` to return promptly while callbacks continue on a worker until `stop()` requests shutdown.

Create a new branch from the exact validated B3B HEAD.

B4A changes only threading/lifetime behavior:

1. preserve all B3B format, buffer, packet-slot and DMA-copy behavior
2. `start()` transitions the WaveRT pin to RUN, starts one worker thread and returns `ASE_OK` promptly
3. the worker waits for WaveRT notifications, reads PacketCount/presentation position, calls host `bufferSwitch`, and performs the same planar -> interleaved DMA packet copy
4. the registry-free smoke waits until exactly 20 successful callbacks/copies are observed
5. `stop()` signals worker shutdown, joins it, then performs `RUN -> PAUSE -> ACQUIRE -> STOP`
6. `disposeBuffers()` must never close event/pin/filter handles until the worker is fully joined
7. worker failure must be latched and surfaced to the smoke without unsafe handle teardown
8. preserve both FREE/BUSY coexistence gates
9. no DAW registration yet
10. no format expansion yet

B4A smoke must prove at minimum:

- `start()` returned before callback 20
- callbacks and DMA writes occur from the worker
- exactly 20 diagnostic callbacks/copies complete
- callback indices follow PacketCount-derived slots
- packet discontinuities = 0
- presentation position regressions = 0
- worker joins before pin/event/filter close
- normal STOP and COM unload remain clean

## Still frozen

Do not add yet:

- DAW registration/testing
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

After B4A passes, fill the remaining host-facing ASIO contract needed by real DAWs (`getChannelInfo`, sample-position/time reporting, clock-source behavior, host capability negotiation as required) before registering the driver for DAW testing.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
