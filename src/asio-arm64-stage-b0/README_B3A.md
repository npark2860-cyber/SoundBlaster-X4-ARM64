# Sound Blaster X4 native ARM64 ASIO — Stage B3A callback ABI smoke

Stage B3A validates the host-facing ASIO double-buffer callback ABI while preserving the already-hardware-proven Stage B2 WaveRT lifecycle.

## Fixed hardware scope

Unchanged from Stage B2:

- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- notification count 2
- 20-notification registry-free diagnostic run
- coexistence gate at COM `init()`
- second coexistence gate immediately before `KsCreatePin`
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- notification unregister before handle close

## New variable in B3A

B3A completes only these ASIO ABI structures:

- `ASIOBufferInfo`
- `ASIOCallbacks`

The public ASIO SDK forces 4-byte packing for its interface structures. The ARM64 compile-time guards therefore require:

```text
sizeof(ASIOBufferInfo)=24 align=4
sizeof(ASIOCallbacks)=32 align=4
```

`createBuffers()` accepts exactly:

- 2 output channels
- channel 0 and channel 1
- 512 frames
- a non-null `bufferSwitch` callback

The driver allocates four host-side planar buffers:

- left buffer 0
- left buffer 1
- right buffer 0
- right buffer 1

Each buffer contains 512 signed 16-bit samples.

For every already-validated WaveRT notification, the engine observer invokes:

```text
bufferSwitch(notificationIndex & 1, ASIOFalse)
```

Therefore the expected callback index sequence is:

```text
0,1,0,1,...
```

for exactly 20 callbacks.

## Critical safety / isolation rule

Stage B3A does **not** copy ASIO host samples into the WaveRT DMA buffer.

The WaveRT buffer is still zeroed exactly once before RUN and is not written during RUN.

The smoke callback writes sentinel values only into the returned host-side ASIO buffers to prove that those pointers are valid and writable. Expected count:

```text
hostSampleWrites=40
hardwareBufferWrites=0
```

This intentionally isolates callback ABI risk from audio sample-transfer risk.

## Test order

Run idle first:

```bat
x4-asio-stage-b3a-smoke.exe
```

If Windows playback already owns the X4, a clean BUSY refusal is also an expected safe result. Never bypass BUSY.

## Expected FREE result

Important lines include:

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

## Still not DAW-ready

Do not register or use this driver in a DAW yet.

Stage B3A still lacks:

- host-buffer to WaveRT sample copy
- audible ASIO output
- asynchronous production callback thread
- capture
- 24-bit transport
- multichannel
- dynamic sample-rate or buffer-size support

Creative binaries remain reference-only and are not runtime dependencies.
