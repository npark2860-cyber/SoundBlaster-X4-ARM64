# Sound Blaster X4 native ARM64 ASIO — Stage B4A asynchronous worker

Stage B4A changes only the runtime lifetime model after the hardware-proven Stage B3B PCM-to-WaveRT path.

## Preserved from B3B

- native ARM64
- X4 `msft_wave`, Render Pin 1
- 48 kHz stereo signed 16-bit PCM
- ASIO 512-frame planar double buffers
- WaveRT 4096-byte cyclic buffer
- NotificationCount=2
- 2048 bytes / 512 stereo frames per WaveRT packet
- COM init coexistence gate
- second gate immediately before `KsCreatePin`
- `writePacket = PacketCount + 1`
- `slot = writePacket % 2`
- host `bufferSwitch(slot, ASIOFalse)` followed by planar -> interleaved copy into that slot
- no `KSPROPERTY_RTAUDIO_SETWRITEPACKET`

## New variable

B3B `start()` blocked while processing the whole 20-notification diagnostic run.

B4A changes this so:

1. `start()` creates a manual-reset stop event
2. the engine enters `ACQUIRE -> PAUSE -> RUN`
3. one worker thread is created
4. `start()` returns `ASE_OK` immediately
5. the worker waits on both the stop event and WaveRT notification event
6. each WaveRT event runs the same PacketCount / position / callback / DMA-copy path as B3B
7. `stop()` signals the stop event
8. `stop()` joins the worker
9. only after a successful join does it issue `RUN -> PAUSE -> ACQUIRE -> STOP`
10. `disposeBuffers()` refuses to close WaveRT handles if the worker could not be joined

The worker is deliberately a plain Win32 thread in B4A. MMCSS/AVRT priority is not added in the same experiment.

## Registry-free smoke

Run with normal Windows X4 playback idle:

```bat
x4-asio-stage-b4a-smoke.exe
```

The smoke keeps the same low-level 440 Hz / peak 1200 test tone.

It verifies:

- `start()` duration is below the diagnostic threshold and returns before callback 20
- callback thread ID differs from the main thread
- all callbacks use one worker thread
- callback indices still alternate according to PacketCount-derived slots
- 20 callbacks are reached before `stop()`
- `stop()` reports `workerJoined=YES`
- final counters still show 20 notifications, 20 DMA writes, and 10,240 copied frames
- `disposeBuffers()` occurs only after worker shutdown
- COM unload is clean

Expected key lines:

```text
start=0
startMessage=B4A start OK: workerThreadId=...; callbacks continue asynchronously
startDurationMs=... callbacksAtStartReturn=... returnedBefore20=YES
B4A worker START thread=...
...
callbacksBeforeStop=20 callbackThread=... mainThread=...
B4A worker STOP requested thread=...
B4A worker EXIT thread=...
B4A KSSTATE 2 -> OK
B4A KSSTATE 1 -> OK
B4A KSSTATE 0 -> OK
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

## Still excluded

- DAW registration/testing
- MMCSS/AVRT worker priority
- full ASIO channel metadata
- sample-position/time reporting to host
- capture
- 24-bit/multichannel
- dynamic sample-rate/buffer-size support
- Creative runtime dependency
