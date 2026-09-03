# Sound Blaster X4 native ARM64 ASIO — Stage B3B host-to-WaveRT sample transfer

Stage B3B is the first experiment that copies host ASIO PCM samples into the mapped WaveRT render buffer.

## Fixed scope

Unchanged:

- native ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM
- ASIO buffer size 512 frames
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- two coexistence gates
- synchronous 20-notification smoke
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- no capture / 24-bit / multichannel / sample-rate expansion

## New B3B variable

The host callback fills planar signed-16-bit left/right ASIO buffers. After the callback returns, the driver interleaves those 512 frames into one 2048-byte WaveRT cyclic-buffer packet.

The target packet is **not** chosen from callback parity alone.

For every notification B3B reads `KSPROPERTY_RTAUDIO_PACKETCOUNT` and computes:

```text
writePacket = PacketCount + 1
slot = writePacket % 2
```

The same `slot` is passed to `bufferSwitch(slot, ASIOFalse)` and then copied to WaveRT.

This follows the Microsoft WaveRT rule that PacketCount is the number of packets already completely transferred and the client must write ahead of the packet currently in progress.

## SETWRITEPACKET deliberately excluded

B3B does not call `KSPROPERTY_RTAUDIO_SETWRITEPACKET`.

Microsoft documents SetWritePacket as optional driver optimization/information. This experiment isolates the mapped-buffer copy variable first.

## Audible smoke

The registry-free smoke host generates a low-level 440 Hz stereo sine wave:

- peak sample = 1200 / 32767 (about -29 dBFS)
- 512 frames per callback
- 20 callbacks
- both channels identical

Set the X4 output volume low before the first FREE-path run.

The initial WaveRT buffer remains zeroed before RUN. B3B starts filling safe cyclic slots only after notifications arrive, so the first packets are silence.

## Required FREE-path checks

```text
init=1
B3B PRE-PIN GATE: C 0/1 G 0/1 busy=NO
B3B WaveRT ... ActualBufferSize=4096 ... packetBytes=2048
createBuffers=0
...
B3B notification=1 packet=1 writePacket=2 slot=0 ...
B3B bufferSwitch callback=1 index=0 ...
B3B DMA writePacket=2 slot=0 frames=512 nonzeroSamples=...
...
B3B notification=20 packet=20 writePacket=21 slot=1 ...
...
start=0
startMessage=B3B RUN notif=20 cb=20 dmaWrites=20 dmaFrames=10240 nonzero=...
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=20480
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B3B DMA COPY RESULT: PASS (HOST PCM COPIED TO WAVERT DMA)
```

The user should also report whether the short low-level tone was audible, but audible perception alone is not the PASS criterion.

## BUSY safety

If Windows playback owns the X4, `init()` or the second pre-pin gate must fail closed and skip `KsCreatePin`. Never bypass BUSY.

## Not DAW-ready yet

B3B still uses a synchronous diagnostic `start()` and is not a production ASIO driver. Do not register it or use it in a DAW yet.

Still excluded:

- asynchronous real-time callback thread
- SetWritePacket
- DAW registration
- capture
- 24-bit
- multichannel
- dynamic sample rates / buffer sizes
- repeated reopen stress
- Creative runtime dependency
