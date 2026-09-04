# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. Stage B1 coexistence preflight
6. Stage B2 fixed WaveRT lifecycle
7. Stage B3A ASIO double-buffer / `bufferSwitch` ABI
8. Stage B3B host PCM -> mapped WaveRT DMA with audible X4 output
9. Stage B4A asynchronous worker / joined stop lifetime
10. Stage B4B host query contract: channels, clock, block-aligned sample position/timestamp
11. Stage B4C ASIO 2.x time-info negotiation and `bufferSwitchTimeInfo` transport — FREE-path functionally proven; original smoke false-failed on coarse timer equality
12. corrected B4C smoke BUSY safety path — hardware PASS; final corrected FREE-path confirmation still pending

Do not intentionally reproduce the known green-screen collision.

## Frozen streaming baseline

Keep unchanged:

- native Windows ARM64
- X4 `msft_wave`, Render Pin 1
- 48 kHz stereo 16-bit PCM
- ASIO buffer 512 frames
- WaveRT cyclic buffer 4096 bytes, NotificationCount=2
- PacketCount-derived write-ahead slot
- coexistence gate at init and immediately before `KsCreatePin`
- single asynchronous worker thread
- joined worker before KS teardown
- no callback after joined `stop()` returns
- logical ASIO sample position `0, 512, 1024, ...`

## Validated Stage B3B

`exp/windows-arm64-asio-com-stage-b3b-dma-copy@08ec4db74f6a5fcf49b301991628f458bb6d666e`

Audible 440 Hz output through X4 confirmed.

## Validated Stage B4A

`exp/windows-arm64-asio-com-stage-b4a-async-worker@996025332bf17341b584095260c1abec93222d84`

Key proof:

```text
startDurationMs=4.035 callbacksAtStartReturn=0 returnedBefore20=YES
stopMessage=B4A stop OK workerJoined=YES notif=20 cb=20 dmaWrites=20 dmaFrames=10240
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 hostSampleWrites=20480
STAGE B4A ASYNC RESULT: PASS (ASYNC START/WORKER/STOP LIFETIME)
```

## Validated Stage B4B

`exp/windows-arm64-asio-com-stage-b4b-host-query@84646e7a5b8d7808e72bcf5fde545b78d34ced3c`

Key proof:

```text
getChannels=0 inputs=0 outputs=2
channel0Before hr=0 active=0 group=0 type=16 name=X4 Output L
channel1Before hr=0 active=0 group=0 type=16 name=X4 Output R
clock ... index=0 ... current=1 name=Internal set0=0 set1=-998
B4B bufferSwitch callback=1 ... samplePosition=0 ...
B4B bufferSwitch callback=2 ... samplePosition=512 ...
...
callbackStats count=20 quiescentAfterStop=20 indexErrors=0 directProcessErrors=0 threadErrors=0 positionErrors=0 timestampErrors=0 hostSampleWrites=20480 lastPosition=9728
STAGE B4B HOST QUERY RESULT: PASS (HOST QUERY CONTRACT + B4A ASYNC TRANSPORT)
```

## Stage B4C — ASIO 2.x time-info

Branch:

`exp/windows-arm64-asio-com-stage-b4c-time-info`

Current corrected-smoke HEAD:

`e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Implemented:

- pack-4 `AsioTimeInfo`, `ASIOTimeCode`, `ASIOTime`
- `ASIOFuture(kAsioCanTimeInfo) -> ASE_SUCCESS`
- host negotiation via `asioMessage(kAsioSupportsTimeInfo)`
- successful negotiation switches callbacks to `bufferSwitchTimeInfo()`
- legacy `bufferSwitch()` retained only as fallback
- `ASIOTime.timeInfo` carries logical block-start sample position, Windows ASIO `timeGetTime()`-derived system time, 48 kHz sample rate and speed 1.0
- `getSamplePosition()` returns the same block position/timestamp pair used in `ASIOTime`
- time code remains invalid/unused
- `wavert_engine_b4a.cpp` remains unchanged

### First B4C hardware run — FREE path, original smoke false FAIL

Observed:

```text
future(kAsioCanTimeInfo)=1061701536 expected=1061701536
B4C host asioMessage kAsioSupportsTimeInfo -> 1
timeInfoNegotiationCalls=1
...
callbacksBeforeStop=21 legacyCallbacks=0
stopMessage=B4C stop OK workerJoined=YES notif=21 cb=21 dmaWrites=21 dmaFrames=10752
callbackStats timeInfo=21 quiescentAfterStop=21 legacy=0 indexErrors=0 directProcessErrors=0 threadErrors=0 timeInfoErrors=0 positionErrors=0 timestampErrors=6 consistencyErrors=0 hostSampleWrites=21504 lastPosition=10240
getSamplePosition after stop=-996 expected=-996
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: FAIL
```

All ASIO2 negotiation, callback mode, sample-position, consistency, DMA, worker lifetime and cleanup invariants succeeded. `timestampErrors=6` came only from repeated adjacent `timeGetTime()` ticks.

Correct smoke invariant:

- timestamp > 0
- timestamp never regresses (`current >= previous`)
- timestamp advances at least once during the multi-callback run
- `ASIOTime.systemTime` equals `getSamplePosition()` timestamp inside the same callback

Production driver code was not modified for this correction.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_FIRST_RUNTIME.md`.

## Corrected B4C build

Workflow run: `33819803619`, attempt 2 job `100862010928`

Checkout:

`e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Build verified:

- `x4-asio-arm64.dll` ARM64 `0xAA64`
- `x4-asio-stage-b4c-smoke.exe` ARM64 `0xAA64`
- corrected source `smoke_b4c_monotonic.cpp` compiled

Hashes:

- DLL SHA256 `480CD573CDE4B61F852592FA566E772AD8AC9F4EDE286E6440AAF2AA09CB054B`
- smoke SHA256 `70177DA7DDF7A30E14EC816A23FC4A8C4B62FB1DF6EE9F8B09CD593ACE7230D7`
- distribution ZIP SHA256 `5F69C848DFA80C906DBA311221FF8943E56A54AEBA5725FD0BF92737A857FEFB`

## Corrected B4C smoke hardware run — BUSY safety PASS

The corrected executable was run, but normal Windows playback already owned the X4 global render instance.

Observed:

```text
Sound Blaster X4 ARM64 ASIO Stage B4C ASIO2 time-info smoke (coarse-timer corrected)
init=0
initMessage=B4C init BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
driverVersion=107
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

This proves the corrected artifact preserves the coexistence gate and safely refuses while Windows playback owns the single global render instance. It does not yet exercise the corrected timestamp assertion because no second WaveRT stream was created.

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`.

## Immediate next action

Close/stop all normal Windows playback using X4 and verify the global instance returns to FREE (`G 0/1`). Then run the same corrected registry-free executable again:

```bat
x4-asio-stage-b4c-smoke.exe
```

Required final corrected FREE-path proof:

```text
init=1
initMessage=B4C init FREE: C 0/1 G 0/1; ...
...
callbackStats ... timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES ...
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

A legitimate asynchronous stop-boundary overshoot above 20 callbacks remains acceptable if final counts match and stay quiescent after joined stop.

Never bypass BUSY.

## Still frozen

Do not add yet:

- actual ASIO registry registration / REAPER load
- MMCSS/AVRT worker priority
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- time code
- Creative runtime dependencies
- custom kernel driver

After corrected B4C FREE-path hardware PASS, the next controlled milestone is ASIO registration and the first real REAPER load/playback test at 48 kHz / 16-bit / stereo / 512 frames.

## Architecture

native ARM64 REAPER / DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / mapped WaveRT cyclic buffer
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
