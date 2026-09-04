# DEBUG HISTORY — ASIO B5 fail-safe runtime exposed 192 kHz render packet discontinuity

Updated: 2026-09-04 KST

## Returned validation report

Generated:

`2026-09-04 13:50:09.41`

Build/runtime markers present:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

The runtime fail-safe build therefore loaded successfully.

## Result before failure

PASS before the failing cycle:

- registration
- registry verification
- KS property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output cycle 1

The 96 kHz duplex observation remained similar to prior runs:

- cycle1 render 281 / capture 255 / phase misses 27
- cycle2 render 278 / capture 252 / phase misses 27

No strict 96 kHz packet/index/copy failure was reported.

## Failing case

`preferred-192-output` cycle 2 failed after the stream had already been running.

Observed:

- rate = 192000
- frames = 384
- callback count = 332
- render packet discontinuities = 1
- render position regressions = 0
- callback index errors = 0
- render copy errors = 0
- capture copy errors = 0
- `worker_failed_ = 1`
- stop result = `ASE_HWMalfunction` (`-999`)

Worker output:

`B5 worker RENDER failed: B5 RENDER RUN entered emergencySilence=OK log=%TEMP%\B5_RUNTIME_FAILURE.txt`

Final product result:

`B5 PRODUCT VALIDATION RESULT: FAIL code=28`

`B5 INSTALL + PRODUCT VALIDATION: FAIL`

The newly added `reaper-48-480-output` case was not reached because validation stops on the first failing case.

## Important interpretation

This failure occurs before the failure-only file logger executes. The logger did not create the render discontinuity.

Current render signaled-notification logic reads `KSPROPERTY_RTAUDIO_PACKETCOUNT` and increments `packet_discontinuities` whenever the returned packet number is not exactly `previous_packet + 1`.

Mux-v3 compares the discontinuity counter before/after each render notification and treats any increase as fatal.

Therefore this report proves that the worker observed a non-sequential render PACKETCOUNT value during the second 192k/384 cycle.

At 192 kHz / 384 frames, each notification period is 2.0 ms. One plausible cause is notification event coalescing or scheduling delay allowing the completed render packet count to advance by more than one before user mode services the event. This is not yet proven because the current diagnostic record does not store the exact previous and current packet numbers at the discontinuity.

Do not weaken the strict discontinuity rule yet.

## Fail-safe result

`emergencySilence=OK`

This is the first direct proof that `runtime-failsafe-v1` executed on a real worker fatal path and successfully overwrote both render cyclic slots with silence before logging.

The worker-side pin teardown prohibition remained intact.

## Immediate evidence request

Retrieve:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

from the same machine/session and preserve it before another failing run overwrites it.

The current file should contain the pre-silence stats snapshot, rate/frames, last render packet, engine messages and error counters.

## Next engineering action after the file is captured

Improve render discontinuity diagnostics to record exact packet transition (`previous -> current`, delta) and corresponding presentation position/QPC if the current failure record is insufficient.

Then determine whether the correct fix is:

- event-drain/coalescing handling that processes an advanced PACKETCOUNT safely while preserving write-ahead semantics, or
- another measured scheduling/notification issue.

Do not mask the failure by increasing tolerances or disabling packet continuity checks.

Do not resume control-panel work until this concrete render runtime failure is understood and fixed or proven not to threaten the first-release host path.
