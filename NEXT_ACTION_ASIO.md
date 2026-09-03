# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in both FREE and BUSY states
6. ASIO COM Stage B2 fixed WaveRT engine in both BUSY and FREE states

Do not intentionally reproduce the known green-screen collision.

## Frozen render baseline

Keep unchanged until later expansion:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- notification count = 2
- zero hardware buffer once before RUN
- 20 notifications for registry-free smoke
- no hardware-buffer writes during RUN
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister notification event before closing handles

Known-good SDK baseline:

`exp/windows-arm64-asio-sdk-abi-baseline@a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

## Coexistence rule — hardware confirmed

Render Pin 1 has one global instance.

- idle: C `0/1`, G `0/1` -> FREE
- Windows X4 playback active: C `0/1`, G `1/1` -> BUSY

Product safety rule:

**If either instance query is indeterminate or saturated, do not call `KsCreatePin`.**

A second gate must run immediately before every real render `KsCreatePin`, not only at COM `init()`.

Do not add Creative `TakeExclusiveControl` arbitration yet.

## Stage B0 — PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b0@53a1854167447338ca45606b6de2181ae6d8148d`

## Stage B1 — FREE + BUSY PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b1-preflight@9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B1_RUNTIME_SUCCESS.md`.

## Stage B2 fixed WaveRT behind COM — FREE + BUSY PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b2-wavert@a6d3201260056a46ae8bce57271132871904d6ee`

BUSY path cleanly blocks before pin creation.

FREE path hardware-confirmed:

```text
init=1
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B2 WaveRT BufferAddress=... ActualBufferSize=4096 CallMemoryBarrier=0
createBuffers=0
B2 KSSTATE 1 -> OK
B2 KSSTATE 2 -> OK
B2 KSSTATE 3 -> OK
...
B2 notification=20 packet=20 samplePosition=9680 ...
start=0
startMessage=Stage B2 RUN observed 20/20 notifications; packetDiscontinuities=0 positionRegressions=0
B2 KSSTATE 2 -> OK
B2 KSSTATE 1 -> OK
B2 KSSTATE 0 -> OK
stop=0
B2 unregister notification -> OK
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B2 COM/WAVERT RESULT: PASS (FIXED WAVERT LIFECYCLE)
```

See:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B2_BUSY_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B2_FREE_RUNTIME_SUCCESS.md`

## Stage B3A — ASIO host buffer/callback ABI implemented

Branch:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi`

Implementation HEAD:

`46c22ef00f85f3668d6851844fa1558d250cedb8`

B3A starts from the validated B2 source and adds only the host-facing callback layer.

The public ASIO SDK 2.3 interface header forces 4-byte packing for its API structures. B3A therefore uses `#pragma pack(push,4)` and ARM64 compile-time guards for:

- `sizeof(ASIOBufferInfo)=24`
- `alignof(ASIOBufferInfo)=4`
- `offsetof(ASIOBufferInfo,buffers)=8`
- `sizeof(ASIOCallbacks)=32`
- `alignof(ASIOCallbacks)=4`

`createBuffers()` now accepts exactly:

- 2 output channels
- channel 0 and 1
- 512 frames
- non-null `bufferSwitch`

The driver returns four distinct host-side planar signed-16-bit buffers: two channels x two double-buffer indices.

The proven B2 WaveRT loop gained only a notification observer hook. After each notification has already passed packet/presentation-position validation, B3A invokes:

```text
bufferSwitch(notificationIndex & 1, ASIOFalse)
```

for 20 alternating callbacks.

Critical isolation rule remains:

- host callback may write host-side ASIO buffers
- B3A does not copy those samples to WaveRT DMA
- WaveRT hardware buffer is still zeroed once before RUN
- hardware-buffer writes during RUN remain zero
- no callback thread is added yet; `start()` remains synchronous for this diagnostic stage

B2 vs B3A diff confirms the hardware engine change is limited to the observer hook plus the new B3A host-facing files and CMake target selection.

## Immediate next action — build and run B3A idle smoke

Manual workflow:

`Build ASIO COM Stage B3A Callback ARM64`

Artifact should contain:

- `x4-asio-arm64.dll`
- `x4-asio-stage-b3a-smoke.exe`

Run idle first.

Expected key result:

```text
ABI sizeof(ASIOBufferInfo)=24 align=4 sizeof(ASIOCallbacks)=32 align=4
init=1
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
createBuffers=0
ASIO buffers ... distinctNonNull=YES
B3A bufferSwitch callback=1 index=0 directProcess=0
B3A bufferSwitch callback=2 index=1 directProcess=0
...
B3A bufferSwitch callback=20 index=1 directProcess=0
start=0
startMessage=Stage B3A RUN notifications=20 callbacks=20 callbackIndexErrors=0 hardwareBufferWrites=0
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=40 hardwareBufferWrites=0
stop=0
disposeBuffers=0
dispose cleared ASIO buffer pointers=YES
DllCanUnloadNow hr=0x00000000
STAGE B3A CALLBACK RESULT: PASS (ASIO CALLBACK ABI, NO DMA COPY)
```

If Windows playback owns the X4, BUSY must still be accepted only as a clean safe refusal. Never bypass BUSY.

## Still frozen

Do not add yet:

- host-buffer -> WaveRT sample copy
- audible output
- asynchronous/production ASIO callback thread
- DAW registration/testing
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / WaveRT
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
