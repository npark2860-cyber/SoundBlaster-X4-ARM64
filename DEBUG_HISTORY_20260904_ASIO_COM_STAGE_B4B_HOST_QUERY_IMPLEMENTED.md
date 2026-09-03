# Debug history — ASIO COM Stage B4B host query contract implemented

Date: 2026-09-04 KST

## Base

Stage B4B branch:

`exp/windows-arm64-asio-com-stage-b4b-host-query`

Exact base:

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

Stage B4A is hardware/runtime PASS. B4B preserves its streaming/lifetime path and adds only host inquiry behavior.

## Scope

Implemented:

- Windows ASIO ABI concrete query types
  - 64-bit `ASIOSamples`
  - 64-bit `ASIOTimeStamp`
  - 4-byte-packed `ASIOClockSource`
  - 4-byte-packed `ASIOChannelInfo`
- output channel metadata
  - channel 0 = `X4 Output L`
  - channel 1 = `X4 Output R`
  - output only
  - group 0
  - `ASIOSTInt16LSB`
  - `isActive` tracks ASIO buffer lifetime
- one internal clock source
  - index 0
  - associated channel/group = `-1/-1`
  - current source = true
- `setClockSource(0)` succeeds
- invalid clock source reference returns `ASE_InvalidParameter`
- block-aligned logical `getSamplePosition()`
  - reset at `start()`
  - callback 1 = sample 0
  - +512 samples per callback
  - QPC-derived nanosecond timestamp latched immediately before host callback
  - returns `ASE_SPNotAdvancing` when stream is not advancing

Not implemented in B4B:

- `bufferSwitchTimeInfo`
- ASIO time-info capability negotiation
- registration/DAW loading
- MMCSS/AVRT
- new formats/sample rates/buffer sizes

## Streaming path frozen

The B4B CMake target deliberately reuses the exact Stage B4A WaveRT engine file:

`wavert_engine_b4a.cpp`

The B4B branch does not modify that file.

Preserved:

- both coexistence gates
- Render Pin 1
- 48k / stereo / int16
- 512-frame ASIO host buffers
- 4096-byte mapped WaveRT cyclic buffer
- PacketCount-derived write-ahead slot
- host planar -> interleaved mapped DMA copy
- asynchronous worker
- worker join before KS teardown
- post-stop callback quiescence

## Registry-free smoke

`x4-asio-stage-b4b-smoke.exe` validates:

- ABI structure sizes
- channel count
- channel metadata inactive before `createBuffers`
- internal clock source
- clock-source setter behavior
- pre-start `ASE_SPNotAdvancing`
- channel active state after `createBuffers`
- B4A asynchronous start behavior
- per-callback sample positions `0, 512, 1024, ...`
- strict timestamp monotonicity
- callback thread remains distinct from main thread
- PCM/DMA transport remains successful
- joined stop and post-stop quiescence
- post-stop `ASE_SPNotAdvancing`
- channel inactive state after `disposeBuffers`
- COM unload

A legitimate asynchronous stop-boundary overshoot above 20 callbacks remains allowed, using the B4A corrected invariant.

## Branch diff discipline

Relative to validated B4A HEAD, only these files change/add:

- `src/asio-arm64-stage-b0/CMakeLists.txt`
- `src/asio-arm64-stage-b0/asio_compat.h`
- `src/asio-arm64-stage-b0/driver_b4b.cpp`
- `src/asio-arm64-stage-b0/smoke_b4b.cpp`
- `src/asio-arm64-stage-b0/README_B4B.md`

No B4A WaveRT engine or preflight code is modified.

## Build

Manual workflow on `main`:

`Build ASIO COM Stage B4B Host Query ARM64`

Workflow trigger is `workflow_dispatch` only.

The workflow checks out the B4B branch, builds native ARM64 DLL + smoke EXE, verifies PE machine `0xAA64`, and packages the registry-free test artifact.

## Next

Build and run the B4B smoke with normal Windows X4 playback idle.

If B4B passes, preserve this query contract and add ASIO time-info negotiation (`bufferSwitchTimeInfo`) as the next isolated host-contract variable before controlled registration / real DAW loading.
