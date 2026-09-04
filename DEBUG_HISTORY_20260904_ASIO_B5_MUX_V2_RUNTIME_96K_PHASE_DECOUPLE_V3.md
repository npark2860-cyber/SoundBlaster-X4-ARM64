# DEBUG HISTORY — ASIO B5 mux v2 runtime at 96 kHz and mux v3 phase decoupling

Date: 2026-09-04 KST

## Returned runtime report

`B5_PRODUCT_VALIDATION_REPORT(3).txt`

Generated: `2026-09-04 12:25:44.97`

The package compiled, installed, registered, and executed the intended runtime adapter. The report contains:

- `adapter=dual-event-mux-v2`
- MMCSS `Pro Audio`
- `priority=OK`

Therefore this report is valid runtime evidence for mux v2.

## PASS evidence

Registration and property-only idle gate passed.

KS capability probe passed.

48 kHz / 240 output-only passed three cycles:

- callbacks 141 / 139 / 139
- `stop=0`
- `workerJoined=YES`
- no packet/index/copy failures

48 kHz / 240 full duplex passed two cycles:

- callbacks 138 / 138
- render notifications 139 / 139
- capture notifications 138 / 138
- output/input DMA frames 33120 / 33120
- `stop=0`

## 96 kHz / 240 full-duplex failure

The first 96 kHz cycle produced one successful callback and then mux v2 stopped with:

`B5 worker DUPLEX failed: next render notification arrived before prior capture synchronization`

Final strict counters were:

- callbacks=1
- worker=1
- callback-index errors=0
- output copy errors=0
- input copy errors=0
- render packet discontinuities=0
- render position regressions=0
- capture packet discontinuities=0

This is critical: the failure was not caused by a WaveRT packet jump, position regression, or copy error. It was caused by the mux-v2 policy itself.

## Root cause

Mux v2 allowed only one pending render packet and required exact pairing:

`render N -> capture N-1`

If another render notification arrived before that exact capture packet had been observed, v2 declared a duplex synchronization failure immediately.

At 96 kHz / 240 frames the period is 2.5 ms. Render and capture WaveRT notification streams can have a stable phase offset even while both absolute packet sequences remain continuous. Therefore a second render wake before the prior capture wake is not, by itself, evidence of hardware failure.

Queuing render packets is also not a correct solution because delaying render callbacks risks missing the required render write-ahead deadline.

## Fix: dual-event-mux-v3

B5 branch commit:

`bb2a42e143cc0b48a60a131e44a06002e3594ec5`

Mux v3 changes full-duplex policy:

- Render remains the ASIO callback/master clock.
- Render callbacks never wait for exact capture packet pairing.
- Capture is an independent producer.
- Capture packets are copied into two fixed staging slots and tagged by absolute capture packet number.
- Before each render callback the worker opportunistically queries capture in addition to servicing capture notification events.
- The oldest unconsumed staged capture packet is copied into the current ASIO input buffer before the render callback.
- If no capture packet is ready for one render period, the current input buffer is zero-filled and a phase miss is counted rather than declaring immediate failure.
- More than four consecutive capture phase misses is treated as real capture starvation and remains fatal.

Strict hardware/data integrity failures remain fatal:

- render packet discontinuity
- capture packet discontinuity
- render presentation-position regression
- callback buffer-index repetition
- render copy failure
- capture copy failure
- capture staging overrun
- capture staging sequence mismatch
- sustained capture starvation
- worker failure

BUSY gates and joined-worker teardown rules are unchanged.

## Runtime/build marker

Mux marker advanced to:

`dual-event-mux-v3`

Main workflow now refuses to package unless this marker exists in both ARM64EC and Classic ARM64 B5 DLLs.

## Next action

Run `Build ASIO B5 Productization` again.

Do not reuse the mux-v2 ZIP.

After build/package PASS, run the new `install_and_validate_b5.cmd` once and return the new validation report.

The next report is accepted as mux-v3 evidence only if it contains:

`adapter=dual-event-mux-v3`
