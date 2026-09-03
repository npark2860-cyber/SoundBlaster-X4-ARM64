# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in both FREE and BUSY states
6. ASIO COM Stage B2 fixed WaveRT engine in both BUSY and FREE states
7. ASIO COM Stage B3A host double-buffer / `bufferSwitch` ABI with zero DMA sample copy

Do not intentionally reproduce the known green-screen collision.

## Frozen hardware baseline

Keep unchanged while adding sample transfer:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT cyclic buffer
- notification count = 2
- 512 frames per notification packet
- coexistence gate at COM init
- second coexistence gate immediately before every real `KsCreatePin`
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister event before closing handles

Known-good SDK baseline:

`exp/windows-arm64-asio-sdk-abi-baseline@a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

## Stage B3A — PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi@46c22ef00f85f3668d6851844fa1558d250cedb8`

Hardware/runtime result:

```text
ABI sizeof(ASIOBufferInfo)=24 align=4 sizeof(ASIOCallbacks)=32 align=4
init=1
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B2 WaveRT BufferAddress=... ActualBufferSize=4096 CallMemoryBarrier=0
createBuffers=0
ASIO buffers ... distinctNonNull=YES
B3A bufferSwitch callback=1 index=0 directProcess=0
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

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3A_CALLBACK_RUNTIME_SUCCESS.md`.

## Immediate next stage — B3B first host-to-WaveRT sample transfer

Create a new branch from the validated B3A source.

B3B adds exactly one new data-path behavior: after the host `bufferSwitch` callback fills one 512-frame planar buffer pair, interleave those samples into the corresponding safe 2048-byte packet of the mapped WaveRT cyclic buffer.

### Packet/slot rule

Do not derive the DMA write location only from notification parity.

`KSPROPERTY_RTAUDIO_PACKETCOUNT` returns the 1-based number of packets completely transferred from the WaveRT buffer into hardware. Therefore after reading `PacketCount=P`:

- packet `P` is in progress
- client writes ahead to packet `P+1`
- with NotificationCount=2, target cyclic slot is `(P + 1) % 2`
- each slot is 2048 bytes = 512 stereo int16 frames

This follows the Microsoft WaveRT packet-count rule and also resynchronizes correctly if a packet is skipped.

### B3B fixed test behavior

- preserve B3A ASIO pack-4 ABI
- preserve 2 output channels / 512 frames
- keep synchronous 20-notification smoke
- callback index must equal the WaveRT target slot derived from PacketCount
- smoke callback generates a low-amplitude deterministic stereo test tone in the host buffers
- driver interleaves L/R int16 into the mapped WaveRT packet after the callback returns
- if `CallMemoryBarrier` was requested by the driver, issue `MemoryBarrier()` after the packet copy
- do not add `KSPROPERTY_RTAUDIO_SETWRITEPACKET` in the same experiment; Microsoft documents that support as optional and B3B should isolate the mapped-buffer copy variable first
- do not add a production callback thread yet

Expected normal mapping:

```text
packet=1 -> writePacket=2 -> slot=0
packet=2 -> writePacket=3 -> slot=1
packet=3 -> writePacket=4 -> slot=0
...
```

The first two packets remain the pre-RUN zeroed data; audible host data begins only after a completed notification makes the corresponding cyclic slot safe to refill.

## Still frozen

Do not add yet:

- asynchronous/production ASIO callback thread
- `KSPROPERTY_RTAUDIO_SETWRITEPACKET`
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
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
