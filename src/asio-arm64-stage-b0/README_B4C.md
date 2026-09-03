# Sound Blaster X4 ARM64 ASIO — Stage B4C

Registry-free native ARM64 smoke for ASIO 2.x time-info capability negotiation.

## Validated parent

Stage B4C starts from the exact Stage B4B hardware-PASS source:

`exp/windows-arm64-asio-com-stage-b4b-host-query@84646e7a5b8d7808e72bcf5fde545b78d34ced3c`

## One-variable scope

B4C adds only the ASIO 2.x time-info callback contract:

- public pack-4 `AsioTimeInfo`, `ASIOTimeCode`, `ASIOTime` ABI
- `ASIOFuture(kAsioCanTimeInfo) -> ASE_SUCCESS`
- driver calls host `asioMessage(kAsioSupportsTimeInfo)` during `createBuffers()`
- if host returns 1 and supplies `bufferSwitchTimeInfo`, streaming callbacks use `bufferSwitchTimeInfo()` instead of legacy `bufferSwitch()`
- each `ASIOTime` carries the same B4B logical block-start position and QPC-derived nanosecond timestamp
- sample rate is 48000 and speed is 1.0
- required time-info valid flags are set
- time-code data remains invalid/unused

The legacy `bufferSwitch` callback remains supplied and retained only as ASIO 1.x fallback. The B4C time-info smoke requires that fallback callback is not used after successful time-info negotiation.

## Frozen transport

Unchanged from B4A/B4B:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz stereo 16-bit PCM
- ASIO buffer 512 frames
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- PacketCount-derived write-ahead slot
- planar host buffers -> interleaved mapped WaveRT copy
- coexistence gate at init and immediately before `KsCreatePin`
- one asynchronous worker thread
- joined worker before KS teardown
- no callback after joined stop

`wavert_engine_b4a.cpp` is intentionally reused unchanged.

## Expected FREE-path proof

```text
ABI ASIOTimeInfo=48/4 ASIOTimeCode=84/4 ASIOTime=148/4 ASIOCallbacks=32/4
init=1
driverVersion=107
future(kAsioCanTimeInfo)=1061701536 expected=1061701536
getSamplePosition before start=-996 expected=-996
B4C host asioMessage kAsioSupportsTimeInfo -> 1
createBuffers=0
createMessage=B4C buffers ready timeInfo=YES; ...
timeInfoNegotiationCalls=1
ASIO buffers distinctNonNull=YES negotiation=YES
start=0
startMessage=B4C start OK ... timeInfo=YES position=0
B4C worker START ... timeInfo=YES
B4C bufferSwitchTimeInfo callback=1 index=0 ... samplePosition=0 ...
B4C bufferSwitchTimeInfo callback=2 index=1 ... samplePosition=512 ...
...
callbacksBeforeStop=20 legacyCallbacks=0 ...
stop=0
stopMessage=B4C stop OK workerJoined=YES ...
callbackStats timeInfo=20 quiescentAfterStop=20 legacy=0 ... timeInfoErrors=0 positionErrors=0 timestampErrors=0 consistencyErrors=0 ...
getSamplePosition after stop=-996 expected=-996
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

A legitimate asynchronous stop-boundary overshoot above 20 remains acceptable if all final driver/host counts match and callbacks remain quiescent after joined stop.

## BUSY behavior

If normal Windows playback currently owns the X4 render pin, init/pre-pin BUSY refusal remains a safe PASS condition. Never bypass BUSY.

## Still not enabled

- registry/DAW loading
- MMCSS/AVRT
- capture
- 24-bit
- extra sample rates
- variable buffer sizes
- multichannel
- time code
- direct monitoring

After B4C hardware PASS, controlled ASIO registration and the first real REAPER load/playback test can be planned as the next isolated stage.
