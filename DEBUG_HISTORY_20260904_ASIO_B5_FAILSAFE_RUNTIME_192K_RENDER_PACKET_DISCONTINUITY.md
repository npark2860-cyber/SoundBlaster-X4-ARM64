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

The then-current `reaper-48-480-output` case was not reached because it was ordered after the 192 kHz case.

## Failure-only runtime record returned

`%TEMP%\B5_RUNTIME_FAILURE.txt` was preserved from the same failing session.

Record timestamp:

`2026-09-04 13:50:24.083`

Key contents:

- `direction=RENDER`
- `workerWin32=0`
- `emergencySilence=OK`
- `rate=192000`
- `frames=384`
- `renderRunning=1`
- `callbacks=332`
- `lastCallbackIndex=1`
- `indexErrors=0`
- `renderCopyErrors=0`
- `captureCopyErrors=0`
- `render notifications=333`
- `render packetDiscontinuities=1`
- `render positionRegressions=0`
- `render writes=332`
- `render framesCopied=127488`
- `render lastPacket=334`

The runtime record therefore independently confirms the validator stop summary and proves that the fatal path occurred while the render pin was still RUN.

## Exact packet transition inference

Known WaveRT render semantics for this driver are 1-based completed `PACKETCOUNT` values.

The worker dispatched 332 callbacks successfully. The discontinuous render notification was notification 333 and is rejected before callback dispatch. The failure record reports `lastPacket=334` on that notification.

Therefore the observed transition is:

`packet 332 -> packet 334`

with packet 333 not observed as a separate worker notification.

That is a one-period skip / coalesced notification at the 192 kHz / 384-frame geometry.

384 frames at 192 kHz is exactly 2.0 ms per notification.

The most consistent current explanation is therefore:

`2.0 ms render notification -> one auto-reset event is coalesced or user-mode service is delayed across two periods -> PACKETCOUNT advances 332 -> 334 -> strict discontinuity check fires`

Do not call this a generic WaveRT allocation failure. Allocation at 384 frames was already proven valid and the first 192k/384 cycle completed normally.

## Why the old reason string was misleading

The failure record contained:

`reason=B5 RENDER RUN entered`

and:

`renderMessage=B5 RENDER RUN entered`

This does not mean RUN entry failed. It is stale diagnostic text.

The signaled notification path incremented `packet_discontinuities`, but did not update the engine `last_message_` with the exact packet transition before mux-v3 returned fatal.

The strict counter is the actual cause indicator in this run.

## Fail-safe result

`emergencySilence=OK`

This is the first direct proof that `runtime-failsafe-v1` executed on a real worker fatal path and successfully overwrote both render cyclic slots with silence before logging.

The worker-side pin teardown prohibition remained intact. No worker-side KSSTATE transition, pin close or dispose was added.

The failure-only logger did not create the discontinuity; it runs only after the strict mux failure has already been detected and after emergency silence.

## Diagnostic improvement implemented

B5 now records the exact packet transition whenever a signaled render/capture packet is non-sequential.

The engine message now includes:

`previous=<n> expected=<n+1> current=<m> delta=<m-n>`

Render position regression diagnostics also record previous/current positions.

Commit:

`dc1b16adb7788f27443be9efeb3bdc56ad51536d`

This does not relax any strict packet or position check.

## Dedicated 192 kHz RUN cadence probe implemented

Do not change the public 192 kHz minimum/preferred based on one failure without measuring the stable RUN boundary.

The existing ARM64EC product-validation executable now supports:

`--cadence-192`

Strict candidates:

- 384 frames = 2.000 ms, 1 x 5 s
- 432 frames = 2.250 ms, 2 x 10 s
- 480 frames = 2.500 ms, 2 x 10 s
- 576 frames = 3.000 ms, 2 x 10 s

Every candidate uses the normal B5 driver and the existing fatal packet-discontinuity checks. No tolerance is increased and no discontinuity is ignored.

The probe continues to later candidates after a safely joined/teardown failure so the first sustained stable cadence can be measured in one run. BUSY still aborts.

Implementation commit:

`e6d2a54dfd8a9f3072be749f2c7633c0a1ccddac`

Runner:

`probe_b5_192k_cadence.cmd`

Runner commit/current B5 source at time of this note:

`a62df8395888298a4be96c7cf14ed782d905e188`

The normal product matrix was also reordered so `reaper-48-480-output` runs before the known-sensitive 192 kHz case; this ensures the actual REAPER 48k/480 geometry is exercised even if 192k later fails.

## Workflow packaging

The manual productization workflow now packages:

`probe_b5_192k_cadence.cmd`

The existing ARM64EC/Classic ARM64 marker checks remain:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

No automatic push/PR trigger was added.

## Immediate next action

1. build/package the current B5 branch through the manual `Build ASIO B5 Productization` workflow;
2. install/register the new B5 bundle with X4 otherwise idle;
3. run `probe_b5_192k_cadence.cmd` once;
4. return `B5_192K_CADENCE_REPORT.txt`;
5. do not intentionally reproduce audible buzz and do not weaken strict packet continuity;
6. choose a new 192 kHz minimum/preferred only from the measured first sustained stable cadence;
7. rerun the full product matrix after that measured contract change, if one is required;
8. once the runtime blocker is closed, resume the native ASIO control-panel milestone.

Do not resume CTCDC/CTIntrfu until B5 first-release ASIO is closed.
