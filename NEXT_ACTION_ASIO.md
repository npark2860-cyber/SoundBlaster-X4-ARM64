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
- NotificationCount=2
- 512 frames / 2048 bytes per WaveRT packet
- ASIO buffer size 512 frames
- coexistence gate at COM init
- second coexistence gate immediately before every real `KsCreatePin`
- synchronous 20-notification diagnostic RUN
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister event before closing handles

## Stage B3A — hardware PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b3a-callback-abi@46c22ef00f85f3668d6851844fa1558d250cedb8`

Confirmed:

```text
ABI sizeof(ASIOBufferInfo)=24 align=4 sizeof(ASIOCallbacks)=32 align=4
init=1
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
createBuffers=0
ASIO buffers ... distinctNonNull=YES
bufferSwitch callbacks=20, alternating 0/1
start=0
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=40 hardwareBufferWrites=0
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B3A CALLBACK RESULT: PASS (ASIO CALLBACK ABI, NO DMA COPY)
```

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B3A_CALLBACK_RUNTIME_SUCCESS.md`.

## Stage B3B — implemented, build/runtime pending

Branch:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy`

Implementation HEAD:

`08ec4db74f6a5fcf49b301991628f458bb6d666e`

B3B starts from the exact validated B3A HEAD and adds the first host PCM -> mapped WaveRT data copy.

### Packet/slot rule

Every WaveRT notification still queries `KSPROPERTY_RTAUDIO_PACKETCOUNT`.

For `PacketCount=P`:

```text
writePacket = P + 1
slot = writePacket % 2
```

The same derived `slot` is:

1. passed to host `bufferSwitch(slot, ASIOFalse)`
2. filled by the host callback
3. copied after callback return into the matching 2048-byte WaveRT cyclic-buffer slot

This deliberately avoids choosing a DMA write location from callback parity alone and preserves resynchronization if PacketCount skips.

### Data format

- host: planar signed int16, L and R, 512 frames
- hardware buffer: interleaved signed int16 stereo
- one copy = 512 frames = 1024 int16 samples = 2048 bytes
- expected 20 copies = 10,240 frames
- if WaveRT `CallMemoryBarrier` is true, `MemoryBarrier()` is issued after each copy

### Deliberately excluded from B3B

- `KSPROPERTY_RTAUDIO_SETWRITEPACKET`
- production callback thread
- DAW registration
- capture
- 24-bit
- multichannel
- sample-rate / buffer-size expansion

The B3A files remain in the branch unchanged for comparison; B3B uses separate `driver_b3b.cpp`, `wavert_engine_b3b.cpp`, and `smoke_b3b.cpp` selected by CMake.

## Immediate next action — build B3B

Manual workflow:

`Build ASIO COM Stage B3B DMA Copy ARM64`

It is `workflow_dispatch` only.

Artifact contains:

- `x4-asio-arm64.dll`
- `x4-asio-stage-b3b-smoke.exe`

### First runtime test

1. stop all normal Windows playback through the X4
2. set speaker/headphone volume low
3. run `x4-asio-stage-b3b-smoke.exe`
4. do not bypass BUSY

The smoke generates a low-level 440 Hz stereo tone at peak sample 1200/32767 for the fixed 20-callback diagnostic run.

Expected FREE-path mapping begins:

```text
init=1
B3B PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B3B WaveRT ... ActualBufferSize=4096 ... packetBytes=2048
createBuffers=0
B3B notification=1 packet=1 writePacket=2 slot=0 ...
B3B bufferSwitch callback=1 index=0 ...
B3B DMA writePacket=2 slot=0 frames=512 nonzeroSamples=...
B3B notification=2 packet=2 writePacket=3 slot=1 ...
...
```

Required end-state:

```text
start=0
startMessage=B3B RUN notif=20 cb=20 dmaWrites=20 dmaFrames=10240 nonzero=...
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=20480
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B3B DMA COPY RESULT: PASS (HOST PCM COPIED TO WAVERT DMA)
```

Also record whether the short low-level tone was audible. Audible perception is supporting evidence; the structured counters/state cleanup remain the primary PASS criteria.

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
