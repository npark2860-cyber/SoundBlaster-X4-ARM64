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

Registry-free COM class-factory / vtable / unload smoke PASSed.

## Stage B1 — FREE + BUSY PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b1-preflight@9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B1_RUNTIME_SUCCESS.md`.

## Stage B2 fixed WaveRT behind COM — FREE + BUSY PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b2-wavert@a6d3201260056a46ae8bce57271132871904d6ee`

### BUSY

With Windows playback active:

```text
init=0
initMessage=Stage B2 init preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
STAGE B2 COM/WAVERT RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

### FREE

With X4 idle:

```text
init=1
initMessage=Stage B2 init preflight FREE: C 0/1 G 0/1; engine not prepared
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

Hardware/runtime conclusions:

- COM `init()` FREE/BUSY arbitration works.
- second pre-pin gate remains intact.
- fixed COM -> `KsCreatePin` -> WaveRT lifecycle works.
- 20/20 notification sequence is continuous.
- presentation position is monotonic.
- STOP/unregister/close/unload are clean.

See:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B2_BUSY_RUNTIME_SUCCESS.md`
- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B2_FREE_RUNTIME_SUCCESS.md`

## Immediate next stage — B3A ASIO host buffer/callback ABI only

Create a new branch from the validated B2 source.

B3A must add only the ASIO host-facing buffer/callback layer while preserving the exact B2 hardware lifecycle.

Required scope:

1. define the standard ABI layout needed for `ASIOBufferInfo` and `ASIOCallbacks`
2. accept exactly two output channels, channel 0 and 1
3. accept only the fixed 512-frame buffer size
4. allocate host-side planar 16-bit double buffers for each channel
5. return those pointers in `ASIOBufferInfo::buffers[0/1]`
6. during the already-proven 20 WaveRT notifications, invoke `bufferSwitch` exactly 20 times with alternating double-buffer index 0/1
7. keep `start()` synchronous for this registry-free diagnostic stage; do not add the production callback thread yet
8. do **not** copy host ASIO samples into the WaveRT DMA buffer yet
9. hardware WaveRT buffer remains zero after the one pre-RUN zeroing operation
10. preserve both coexistence gates and all B2 state/cleanup ordering

The smoke harness should verify:

- returned ASIO buffer pointers are non-null and distinct
- callback count = 20
- callback indices alternate 0/1 without discontinuity
- directProcess is false for this diagnostic path
- B2 packet/presentation-position checks remain clean
- no hardware-buffer writes are introduced during RUN

This deliberately separates ASIO callback ABI risk from planar-to-interleaved audio-transfer risk.

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
