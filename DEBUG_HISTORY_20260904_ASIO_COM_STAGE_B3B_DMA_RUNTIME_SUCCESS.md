# Debug history — ASIO COM Stage B3B host PCM -> WaveRT DMA runtime success

Date: 2026-09-04 KST

## Validated source

Branch:

`exp/windows-arm64-asio-com-stage-b3b-dma-copy`

Validated HEAD:

`08ec4db74f6a5fcf49b301991628f458bb6d666e`

Stage B3B starts from the hardware-validated Stage B3A callback ABI source and adds the first mapped WaveRT DMA sample copy.

## Runtime conditions

- native Windows ARM64
- Sound Blaster X4 `msft_wave`
- Render Pin 1
- 48 kHz stereo 16-bit PCM
- WaveRT cyclic buffer 4096 bytes
- notification count 2
- packet size 2048 bytes / 512 stereo frames
- registry-free smoke harness
- normal Windows X4 playback idle
- both coexistence gates preserved
- `KSPROPERTY_RTAUDIO_SETWRITEPACKET` deliberately not used

The smoke host generated a low-level 440 Hz stereo test signal at peak sample 1200/32767.

The tester explicitly reported that the short test tone was audible through the X4. Therefore B3B now has both structured runtime proof of mapped WaveRT DMA sample transfer and direct perceptual confirmation that the generated PCM reached the device's audible render output.

## Hardware/runtime result

COM creation and preflight succeeded:

```text
ABI sizeof(ASIOBufferInfo)=24 align=4 sizeof(ASIOCallbacks)=32 align=4
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
init=1
initMessage=B3B init FREE: C 0/1 G 0/1; buffers not created
driverVersion=104
B3B PRE-PIN GATE: C 0/1 G 0/1 busy=NO
```

WaveRT buffer acquisition and ASIO buffer setup succeeded:

```text
B3B WaveRT BufferAddress=... ActualBufferSize=4096 CallMemoryBarrier=0 packetBytes=2048
createBuffers=0
createMessage=B3B buffers ready: planar host -> interleaved WaveRT copy enabled
ASIO buffers ... distinctNonNull=YES
```

The proven state sequence entered RUN successfully:

```text
B3B KSSTATE 1 -> OK
B3B KSSTATE 2 -> OK
B3B KSSTATE 3 -> OK
```

All 20 notifications followed the PacketCount-derived write-ahead mapping. Representative beginning:

```text
B3B notification=1 packet=1 writePacket=2 slot=0 ...
B3B bufferSwitch callback=1 index=0 directProcess=0 tone=440Hz peak=1200
B3B DMA writePacket=2 slot=0 frames=512 nonzeroSamples=1022
B3B callback=1 notificationIndex=0 packet=1 writePacket=2 slot=0 copy=OK

B3B notification=2 packet=2 writePacket=3 slot=1 ...
B3B bufferSwitch callback=2 index=1 directProcess=0 tone=440Hz peak=1200
B3B DMA writePacket=3 slot=1 frames=512 nonzeroSamples=1022
B3B callback=2 notificationIndex=1 packet=2 writePacket=3 slot=1 copy=OK
```

The sequence continued without slot/callback mismatch through notification 20:

```text
B3B notification=20 packet=20 writePacket=21 slot=1 ...
B3B bufferSwitch callback=20 index=1 directProcess=0 tone=440Hz peak=1200
B3B DMA writePacket=21 slot=1 frames=512 nonzeroSamples=1022
B3B callback=20 notificationIndex=19 packet=20 writePacket=21 slot=1 copy=OK
```

Final run counters:

```text
start=0
startMessage=B3B RUN notif=20 cb=20 dmaWrites=20 dmaFrames=10240 nonzero=20444
callbackStats count=20 indexErrors=0 directProcessErrors=0 hostSampleWrites=20480
```

This proves:

- 20/20 WaveRT notifications were serviced
- 20/20 ASIO `bufferSwitch` callbacks occurred
- callback index matched the PacketCount-derived target slot
- 20 mapped DMA packet writes occurred
- 10,240 stereo frames were copied into mapped WaveRT memory
- copied data was non-zero (`nonzero=20444` sample values across the 20 packets)
- no callback-index errors were observed
- no `directProcess` ABI errors were observed
- the generated 440 Hz PCM was audibly reproduced by the Sound Blaster X4

The normal stop and cleanup path remained intact:

```text
B3B KSSTATE 2 -> OK
B3B KSSTATE 1 -> OK
B3B KSSTATE 0 -> OK
stop=0
stopMessage=B3B stop OK: RUN->PAUSE->ACQUIRE->STOP
B3B unregister notification -> OK
disposeBuffers=0
disposeMessage=B3B disposeBuffers OK: host + WaveRT buffers detached
dispose cleared ASIO buffer pointers=YES
DllCanUnloadNow hr=0x00000000
STAGE B3B DMA COPY RESULT: PASS (HOST PCM COPIED TO WAVERT DMA)
```

## Conclusion

Stage B3B is hardware/runtime PASS with audible output confirmation.

The independent native ARM64 ASIO path now demonstrates, in one controlled registry-free process:

`COM IASIO -> coexistence gates -> KsCreatePin -> WaveRT mapped buffer -> ASIO bufferSwitch -> planar int16 host PCM -> interleaved WaveRT DMA packet -> Microsoft usbaudio2.sys -> audible Sound Blaster X4 render output`

The next missing architectural behavior for a real ASIO host is not sample transport itself. It is asynchronous lifetime behavior: `IASIO::start()` must return while a worker continues waiting for WaveRT notifications and invoking host callbacks until `IASIO::stop()` requests shutdown.

Do not combine that thread/lifetime change with DAW registration or format expansion. Preserve 48k/2ch/16-bit/512-frame geometry and both coexistence gates for the next experiment.
