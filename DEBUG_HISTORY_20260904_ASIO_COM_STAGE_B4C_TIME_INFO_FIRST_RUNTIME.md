# DEBUG HISTORY — ASIO COM Stage B4C Time Info first runtime

Date: 2026-09-04 KST

## Result

The first Stage B4C hardware run printed FAIL, but the transport and ASIO 2.x time-info contract itself were successful. The only failing invariant was an overly strict smoke-test requirement that `systemTime` increase on every callback.

Observed final state:

```text
stop=0
stopMessage=B4C stop OK workerJoined=YES notif=21 cb=21 dmaWrites=21 dmaFrames=10752
callbackStats timeInfo=21 quiescentAfterStop=21 legacy=0 indexErrors=0 directProcessErrors=0 threadErrors=0 timeInfoErrors=0 positionErrors=0 timestampErrors=6 consistencyErrors=0 hostSampleWrites=21504 lastPosition=10240
getSamplePosition after stop=-996 expected=-996
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B4C TIME INFO RESULT: FAIL
```

## Hardware-proven facts from this run

- ASIO 2.x `kAsioCanTimeInfo` capability query succeeded.
- Host `asioMessage(kAsioSupportsTimeInfo)` negotiation occurred exactly once and returned 1.
- `bufferSwitchTimeInfo()` replaced legacy `bufferSwitch()` for every callback.
- 21 time-info callbacks completed on one non-main worker thread.
- Legacy callback count remained zero.
- Logical sample position advanced exactly 512 frames per callback through 10240.
- `ASIOTime.timeInfo` and `getSamplePosition()` agreed on sample position and system time in every callback (`consistencyErrors=0`).
- Required time-info flags/sample rate/speed were valid (`timeInfoErrors=0`).
- Existing B4A/B4B mapped WaveRT DMA path remained intact: 21 writes / 10752 frames.
- Worker joined before KS teardown and no callbacks occurred after stop returned.
- Dispose and COM unload were clean.

## Why timestampErrors=6 is not a driver failure

ASIO 2.x defines Windows `systemTime` as nanoseconds derived from `timeGetTime()`. Windows may expose `timeGetTime()` at a default timer resolution around 15.6 ms. The frozen ASIO block period is 512 / 48000 = about 10.67 ms. Therefore two adjacent callbacks can legitimately observe the same `timeGetTime()` tick.

The first B4C smoke incorrectly required strict increase (`timestamp[n] > timestamp[n-1]`) on every callback. The correct invariant for this source is:

- timestamp must be positive
- timestamp must never regress (`timestamp[n] >= timestamp[n-1]`)
- over a multi-callback run, timestamp must advance at least once
- `ASIOTime.systemTime` must equal `getSamplePosition()`'s timestamp inside the same callback

Do not change the driver timestamp source merely to satisfy the old smoke assertion. Fix only `smoke_b4c.cpp`.
