# DEBUG HISTORY — ASIO B5 48 kHz render notification coalescing / mux-v4 recovery

Updated: 2026-09-04 KST

## New returned evidence

The fail-safe product validation generated `2026-09-04 15:26:42.62`.

It loaded:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

Registration, registry verification, property-only KS idle gate and KS capability probing passed.

48 kHz / 240 output-only:

- cycle 1: PASS, callbacks=139
- cycle 2: PASS, callbacks=142
- cycle 3: FAIL after callbacks=74

Exact worker failure:

`B5 RENDER PACKET DISCONTINUITY previous=74 expected=75 current=76 delta=2`

Final strict state:

- `worker=1`
- `idx=0`
- `outCopy=0`
- `inCopy=0`
- `rPkt=1`
- `rPos=0`
- `stop=-999`

The corresponding `%TEMP%\B5_RUNTIME_FAILURE.txt` recorded:

- rate = 48000
- frames = 240
- render notifications = 75
- callbacks = 74
- render packet discontinuities = 1
- render position regressions = 0
- render writes = 74
- last render packet = 76
- `emergencySilence=OK`

This is a direct, exact forward packet transition:

`74 -> 76`

Packet 75 was not observed as a separate worker notification.

## Why this supersedes the 192 kHz-only hypothesis

An earlier fail-safe run showed the same pattern at 192 kHz / 384 frames, inferred as:

`332 -> 334`

At that time a 2.0 ms 192 kHz notification period looked like a possible root cause and a dedicated cadence probe was prepared.

The new 48 kHz / 240 result occurs at a 5.0 ms period and shows the same exact forward `delta=2` transition. Therefore this is not primarily a 192 kHz minimum-buffer problem.

The shared mechanism matches the already-documented WaveRT notification behavior: the notification event is an auto-reset event, not a counting semaphore. If two hardware periods elapse before user mode services separate waits, the event state can collapse while absolute `PACKETCOUNT` advances by two.

The strict v3 assumption `one event wake == exactly one completed packet` is therefore too strong for the Windows WaveRT notification primitive.

## Fail-safe result

`emergencySilence=OK` again proves the failure-only safety path zeroed both render cyclic slots before diagnostic file I/O.

The worker still performed no KSSTATE transition, pin close or dispose. Joined-worker-before-hardware-teardown remains intact.

## Mux-v4 measured recovery design

New runtime marker:

`dual-event-mux-v4-coalesce-recovery`

The recovery is intentionally limited to one measured condition:

- Render only
- previous packet exists
- forward `delta == 2`
- no render position regression

That condition is reclassified as one `notification_coalesce`, not a packet-corruption discontinuity.

Recovery sequence for an example `74 -> 76`:

1. hardware has already completed packet 76;
2. synthesize the missing ASIO callback for master packet 75 using host buffer index `(75 + 1) % 2 = 0`;
3. deliberately discard the synthetic callback's render output because target packet 76 is already too late;
4. this restores the host callback/index sequence from buffer 1 -> buffer 0;
5. immediately invoke the normal current master packet 76 callback on buffer 1;
6. write its output to future WaveRT packet 77;
7. continue normal event processing.

This preserves:

- ASIO double-buffer alternation
- monotonically advancing callback/sample timeline
- the existing WaveRT write-ahead rule for the first still-future packet

It accepts the unavoidable one-block xrun instead of converting it into permanent worker death / repeated cyclic buzz.

In duplex mode the synthetic missing callback receives zero-filled input and does not consume capture staging. The following current callback resumes normal capture staging consumption.

## What remains fatal

Mux-v4 does not generally relax packet integrity.

Still fatal:

- render duplicate packet
- render backward transition
- render forward `delta > 2`
- capture packet discontinuity
- render position regression
- callback-index repetition outside the explicit catch-up sequence
- render/capture copy failure
- capture staging mismatch/overrun
- sustained capture starvation
- worker failure

The existing `runtime-failsafe-v1` remains the fatal-path fallback.

## Diagnostics

`X4WaveRtB5Stats` now includes:

`notification_coalesces`

Worker exit reports:

- `renderCoalesces`
- `renderDroppedBlocks`

Fatal records also include those counters so a later failure can show whether recoverable xruns preceded it.

A successful product validation may therefore show non-zero coalescing counters while strict packet discontinuity/position/index/copy/worker failure remain zero. This is an explicitly observed and recovered xrun, not a claim of perfect real-time delivery.

## 192 kHz cadence probe status

The dedicated `--cadence-192` / `probe_b5_192k_cadence.cmd` remains available for diagnosis, but it is no longer the immediate decision tool. Do not raise the 192 kHz minimum merely to hide an auto-reset notification coalesce now proven at 48 kHz as well.

## Source changes

B5 branch changes:

- `wavert_engine_b5.h`: add render notification-coalesce counter
- `wavert_engine_b5_signaled.inl`: classify exactly one forward render `delta=2` as recoverable coalescing; all other non-sequential transitions remain strict discontinuities
- `driver_b5_mux_adapter.inl`: mux-v4 synthetic missed callback + discarded late output + current-packet resume; duplex synthetic input zero-fill; coalesce diagnostics
- `README_B5_PRODUCTIZATION.md`: updated product/runtime contract

Validated B4D source remains untouched.

Main workflow now requires both built DLLs to contain:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`

Automatic push/PR execution remains disabled; `workflow_dispatch` is retained.

## Next action

1. build current B5 branch through manual `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 compile/link and v4/failsafe marker checks PASS;
3. install the resulting bundle with other X4 clients closed;
4. run `install_and_validate_b5.cmd` once;
5. return `B5_PRODUCT_VALIDATION_REPORT.txt`;
6. do not run the dedicated 192 kHz cadence probe first;
7. inspect whether any `renderCoalesces` / `renderDroppedBlocks` were recovered and require strict fatal counters to remain zero;
8. after product-matrix PASS, perform one ordinary REAPER 48 kHz / 480 audible playback check;
9. if a fatal failure occurs, stop testing and return `%TEMP%\B5_RUNTIME_FAILURE.txt`.

After this runtime blocker is closed, resume the native ASIO control-panel milestone. The control panel is still required before final B5 first-release closure.
