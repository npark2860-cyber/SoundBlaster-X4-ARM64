# Debug history — ASIO COM Stage B4A asynchronous worker implemented

Date: 2026-09-04 KST

## Base

Stage B4A was branched from the exact hardware-validated Stage B3B source:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

New branch:

`exp/windows-arm64-asio-com-stage-b4a-async-worker`

Implementation HEAD:

`4c8e1c4380ae5b682d03b8b12378a85c509f1149`

## Single changed variable

Stage B3B proved host ASIO PCM -> mapped WaveRT DMA transfer, but its `IASIO::start()` synchronously processed the whole diagnostic notification loop.

Stage B4A changes only runtime threading/lifetime behavior.

Preserved unchanged in intent:

- native ARM64
- X4 `msft_wave`, Render Pin 1
- 48 kHz / stereo / signed 16-bit
- ASIO 512-frame double buffers
- WaveRT 4096-byte cyclic buffer / NotificationCount=2
- both CINSTANCES/GLOBALCINSTANCES safety gates
- PacketCount-derived `writePacket=P+1`, `slot=writePacket%2`
- host `bufferSwitch(slot, ASIOFalse)`
- planar -> interleaved 512-frame DMA packet copy
- no `SETWRITEPACKET`
- state ordering and notification unregister ordering

## New B4A lifetime

`start()` now:

1. creates a manual-reset stop event
2. enters `ACQUIRE -> PAUSE -> RUN`
3. creates one Win32 worker thread
4. returns `ASE_OK` without waiting for the diagnostic callbacks to finish

The worker waits with `WaitForMultipleObjects()` on:

- stop event
- WaveRT notification event

For each WaveRT notification it performs the existing B3B path:

1. read `KSPROPERTY_RTAUDIO_PACKETCOUNT`
2. read presentation position
3. validate packet continuity / monotonic position
4. derive `writePacket=P+1` and cyclic slot
5. call host `bufferSwitch(slot, ASIOFalse)`
6. copy the corresponding 512-frame planar host buffers to interleaved WaveRT memory

`stop()` now:

1. signals stop event
2. joins the worker with a bounded wait
3. if join fails, returns failure and explicitly withholds hardware teardown
4. only after successful join performs `RUN -> PAUSE -> ACQUIRE -> STOP`

`disposeBuffers()` does not close event/pin/filter handles while a worker remains unjoined.

The destructor uses a final stop-event + infinite join safeguard before object teardown.

## Smoke assertions

The registry-free B4A smoke verifies:

- `start()` returns in under the diagnostic threshold and before callback 20
- callbacks occur on a thread different from the main thread
- all callbacks remain on one worker thread
- exactly 20 callbacks are reached before the smoke requests `stop()`
- B3B callback-index and DMA-copy counters remain clean
- `stop()` reports `workerJoined=YES`
- 20 notifications / 20 DMA writes / 10,240 copied frames are preserved
- buffer pointers are cleared only after worker shutdown and WaveRT cleanup
- COM unload remains clean

## Workflow

Main contains a manual-only workflow:

`.github/workflows/build-asio-com-stage-b4a-async-arm64.yml`

Workflow name:

`Build ASIO COM Stage B4A Async ARM64`

It uses only `workflow_dispatch`, checks out the B4A branch, builds native ARM64, verifies PE machine `0xAA64`, and packages the registry-free DLL + smoke executable.

## Status

Implementation complete. Build/runtime pending.

Do not add MMCSS/AVRT, DAW registration, ASIO metadata expansion, capture, or format expansion before this asynchronous lifetime passes hardware smoke.
