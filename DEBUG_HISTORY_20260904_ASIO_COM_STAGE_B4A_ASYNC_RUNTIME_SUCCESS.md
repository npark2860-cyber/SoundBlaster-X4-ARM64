# Debug history — ASIO COM Stage B4A asynchronous worker runtime success

Date: 2026-09-04 KST

## Validated source

Branch:

`exp/windows-arm64-asio-com-stage-b4a-async-worker`

Validated HEAD:

`996025332bf17341b584095260c1abec93222d84`

This is the B4A driver/worker implementation with the corrected registry-free smoke assertion for asynchronous stop-boundary behavior. Only the smoke assertion changed after the first run; the driver/worker/WaveRT code did not.

## Hardware/runtime conditions

- native Windows ARM64
- Sound Blaster X4 `msft_wave`
- Render Pin 1
- 48 kHz stereo 16-bit PCM
- ASIO buffer size 512 frames
- WaveRT cyclic buffer 4096 bytes
- notification count 2
- coexistence gate at `init()`
- second gate immediately before real `KsCreatePin`
- PacketCount-derived write-ahead mapping preserved
- low-level 440 Hz host test tone path preserved from B3B

## Result

COM, preflight and buffer creation succeeded:

```text
init=1
initMessage=B4A init FREE: C 0/1 G 0/1; buffers not created
driverVersion=105
B4A PRE-PIN GATE: C 0/1 G 0/1 busy=NO
createBuffers=0
createMessage=B4A buffers ready: B3B PCM copy preserved; worker not started
ASIO buffers ... distinctNonNull=YES
```

The WaveRT state sequence entered RUN successfully:

```text
B4A KSSTATE 1 -> OK
B4A KSSTATE 2 -> OK
B4A KSSTATE 3 -> OK
```

`IASIO::start()` returned promptly before any callback completed:

```text
mainThread=15264
start=0
startMessage=B4A start OK: workerThreadId=17304; callbacks continue asynchronously
startDurationMs=4.035 callbacksAtStartReturn=0 returnedBefore20=YES
B4A worker START thread=17304
```

All callbacks and DMA writes occurred on the worker thread, not the main thread. The first notification followed the existing B3B mapping:

```text
B4A notification=1 packet=1 writePacket=2 slot=0 ... thread=17304
B4A bufferSwitch callback=1 index=0 directProcess=0 thread=17304 ...
B4A DMA writePacket=2 slot=0 frames=512 ...
B4A callback=1 ... copy=OK thread=17304
```

The run reached the 20-callback diagnostic target with no callback-thread migration:

```text
callbacksBeforeStop=20 callbackThread=17304 mainThread=15264
```

`stop()` then caused the worker to observe the stop request, exit, and only afterward did the driver transition the pin out of RUN:

```text
B4A worker STOP requested thread=17304
B4A worker EXIT thread=17304
B4A KSSTATE 2 -> OK
B4A KSSTATE 1 -> OK
B4A KSSTATE 0 -> OK
stop=0
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
```

The corrected smoke explicitly verified post-stop quiescence:

```text
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
```

Therefore no additional host callback occurred after joined `stop()` returned.

Cleanup remained clean:

```text
B4A unregister notification -> OK
disposeBuffers=0
disposeMessage=B4A disposeBuffers OK: worker absent; host + WaveRT buffers detached
dispose cleared ASIO buffer pointers=YES
DllCanUnloadNow hr=0x00000000
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

## Hardware-proven conclusions

Stage B4A is hardware/runtime PASS.

The independent native ARM64 ASIO path now proves all of the following together:

- `IASIO::start()` can return while streaming continues asynchronously
- WaveRT notifications are processed on one dedicated worker thread
- host `bufferSwitch` callbacks occur on that worker
- B3B host PCM -> mapped WaveRT DMA transport remains intact
- callback index / PacketCount slot synchronization remains intact
- `IASIO::stop()` signals and joins the worker before KS state teardown
- no callback occurs after joined `stop()` returns
- WaveRT notification event, pin and filter handles are closed only after worker lifetime ends
- normal COM unload remains clean

The first B4A run's 20 -> 21 stop-boundary overshoot was a smoke assertion race, not a driver lifetime failure. The corrected test validates the actual ASIO invariant: after `ASIOStop()` returns, the driver must not call the host callback again.

## Next

Keep the proven streaming/lifetime path frozen.

Before DAW registration, complete the minimum host-facing ASIO query contract on a new branch from this exact validated HEAD:

- concrete `ASIOChannelInfo` for output channels 0/1
- one internal `ASIOClockSource`
- `setClockSource(0)` acceptance / invalid reference rejection
- `getSamplePosition()` with a monotonic block-aligned logical sample counter and timestamp
- registry-free smoke coverage for these methods

Do not combine this with registration, MMCSS/AVRT, format expansion, capture, or repeated reopen stress.
