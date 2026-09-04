# Stage B4C corrected-smoke BUSY runtime

Date: 2026-09-04 KST

## Source of truth

- main before this record: `e33297d792be1d167316d6a74528b927c42658b0`
- B4C branch: `exp/windows-arm64-asio-com-stage-b4c-time-info`
- corrected-smoke HEAD: `e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

## Context

The first B4C FREE-path hardware run exercised ASIO 2.x time-info successfully but the original smoke printed FAIL because it required every `timeGetTime()`-derived timestamp to be strictly greater than the previous callback timestamp. Repeated adjacent timestamps are legitimate with the coarse Windows multimedia timer.

A corrected smoke was built that accepts equal adjacent timestamps, rejects regressions, and requires at least one timestamp advance during the multi-callback run. The production B4C DLL was not changed for that correction.

## First run in the submitted log — old smoke, FREE path

Key results:

```text
init=1
initMessage=B4C init FREE: C 0/1 G 0/1; ASIO2 time-info available
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

Interpretation:

- ASIO 2.x capability query succeeded.
- host time-info negotiation succeeded exactly once.
- all streaming callbacks used `bufferSwitchTimeInfo()`.
- legacy `bufferSwitch()` remained unused.
- sample position advanced in exact 512-frame blocks from 0 through 10240.
- `ASIOTime` and `getSamplePosition()` remained consistent.
- B4A DMA copy, asynchronous worker, joined stop, quiescence, dispose, and COM unload all remained intact.
- only the old smoke's strict-timestamp assertion failed.

## Second run in the submitted log — corrected smoke, BUSY path

Key results:

```text
Sound Blaster X4 ARM64 ASIO Stage B4C ASIO2 time-info smoke (coarse-timer corrected)
init=0
initMessage=B4C init BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
driverVersion=107
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

This is a valid safety PASS for the coexistence gate:

- local pin instances: `0/1`
- global pin instances: `1/1`
- Windows playback owned the single global render instance
- `KsCreatePin` was skipped
- no second WaveRT stream was opened
- COM unload remained clean

This run does **not** validate the corrected smoke's FREE-path timestamp invariant because streaming was intentionally blocked before pin creation.

## Current status

B4C functional time-info transport is hardware-proven by the first FREE-path run, and the corrected smoke's BUSY safety path is hardware-proven by the second run.

One final validation remains before marking corrected B4C fully PASS:

1. stop/close normal Windows playback on X4 so global instances return to `G 0/1`
2. run the corrected `x4-asio-stage-b4c-smoke.exe` again
3. require FREE path and final corrected result:

```text
callbackStats ... timestampErrors=0 consistencyErrors=0 timestampAdvanced=YES ...
STAGE B4C TIME INFO RESULT: PASS (ASIO2 TIME-INFO CALLBACK + B4B TRANSPORT)
```

Do not bypass the BUSY gate.

After that FREE-path corrected-smoke PASS, the next isolated stage may proceed to controlled ASIO registration and first REAPER load/playback at the frozen 48 kHz / 16-bit / stereo / 512-frame configuration.
