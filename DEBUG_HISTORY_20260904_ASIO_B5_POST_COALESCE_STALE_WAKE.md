# DEBUG HISTORY — B5 mux-v4 post-coalesce stale render wake

Updated: 2026-09-04 KST

## Initial evidence

After the packet-stats alias regression was fixed, a B5 product bundle returned a full product validation PASS.

The dedicated 192 kHz cadence run then reproduced the intended +2 recovery path and exposed a second edge case.

At 192k/432:

1. normal observed packet sequence reached 1941;
2. the next observed packet was 1943;
3. mux-v4 correctly emitted:
   `B5 RENDER COALESCE RECOVERED previous=1941 missed=1942 current=1943 droppedBlocks=1`
4. the immediately following render wake still returned PACKETCOUNT 1943;
5. the old strict duplicate path treated that as:
   `previous=1943 expected=1944 current=1943 delta=0`
6. worker failed and runtime failsafe silenced output.

The same pattern occurred at 192k/480.

This established a narrow measured sequence:

`forward +2 coalesce -> successful one-block catch-up -> immediately following auto-reset wake may report the same current PACKETCOUNT once`

It was not a reason to globally tolerate duplicate PACKETCOUNT values.

---

## Implemented fix

B5 runtime marker:

`post-coalesce-stale-v1`

Behavior:

- successful +2 recovery arms one stale-wake allowance;
- if the immediately following Render wake returns exactly the same PACKETCOUNT, it is consumed as `NoData`;
- no ASIO callback is issued;
- no Render write occurs;
- callback/sample timeline and hardware-notification stats do not advance for that stale wake;
- mux increments `renderStaleWakes`, clears the allowance and continues waiting;
- if the next wake advances normally, the allowance simply clears;
- a second duplicate, an unarmed duplicate, backward jump, or forward delta >2 remains fatal;
- Capture packet rules are unchanged;
- fail-safe silence and joined-worker-before-teardown remain unchanged.

Touched B5 files only:

- `src/asio-arm64-stage-b0/wavert_engine_b5.h`
- `src/asio-arm64-stage-b0/wavert_engine_b5_signaled.inl`
- `src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

Relevant runtime-code commit:

`64e34b48714789ab17fba57be34b054f2170b4e9`

Current documented B5 branch HEAD:

`4475fc17b70f372fe317fa201f201e8dc5543f9f`

---

# Latest validation of the fix

The user rebuilt the latest B5 bundle containing:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

and returned a new product report plus a new dedicated 192 kHz cadence report.

## Product matrix — PASS

Report generated `2026-09-04 16:32:53.73`:

`B5 INSTALL + PRODUCT VALIDATION: PASS`

Passed:

- registration / registry verification
- property-only KS idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- REAPER-matched 48k/480 output for 5 seconds: 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2
- 48k/96
- 48k/4800
- 48k/512 compatibility
- ASIO capability probe

No render coalescing happened during this short product matrix, but the new marker was loaded and the shared runtime path completed normally.

---

# Dedicated 192 kHz cadence — fix exercised successfully

## 432 frames — PASS x2

Cycle 1:

- `2925 -> 2927` +2 recovered
- same packet 2927 stale wake consumed
- later `3864 -> 3866` +2 recovered
- same packet 3866 stale wake consumed
- worker exit: `renderCoalesces=2 renderDroppedBlocks=2 renderStaleWakes=2`
- stop=0 / workerJoined=YES

Cycle 2:

- three +2 coalesces recovered
- three corresponding stale wakes consumed
- worker remained alive and stopped cleanly

This directly validates the one-shot post-coalesce stale-wake rule.

## 480 frames — PASS x2

Cycle 1:

- one +2 recovery
- one stale wake consumed
- normal clean stop

Cycle 2:

- four +2 recoveries
- three stale wakes consumed
- one recovered coalesce was followed by a normal advancing wake rather than a stale duplicate, proving the one-shot allowance also clears correctly when no duplicate appears
- normal clean stop

## 576 frames — PASS x2

Both cycles completed normally while also exercising +2/stale recovery:

- cycle 1: two coalesces, two stale wakes
- cycle 2: one coalesce, one stale wake

---

# New remaining cadence issue — 192k/384 delta=3

The same latest build did **not** fully pass the cadence probe.

At 192k/384:

`previous=1375 expected=1376 current=1378 delta=3`

The worker correctly treated this as unrecoverable under the current strict policy:

- callbacks=1375
- worker=1
- rPkt=1
- rPos=0
- outCopy=0
- inCopy=0
- index error=0
- `emergencySilence=OK`

This is not the post-coalesce stale-wake edge case and does not invalidate the stale-wake fix. It is a larger forward hardware packet jump that remains intentionally fatal.

Do not hide this by globally allowing arbitrary packet jumps or by changing the 192 kHz allocation contract without a separate runtime design/evidence cycle.

Current long-cadence summary on the latest build:

- 192k/384: FAIL on forward delta=3
- 192k/432: PASS x2, recovery path exercised
- 192k/480: PASS x2, recovery path exercised
- 192k/576: PASS x2, recovery path exercised

---

# Geometry remains unchanged

The user also re-ran the allocation-only geometry probe on the latest bundle.

Result is unchanged:

- 48..336 frames rejected with Win32 87
- 384 first accepted
- 432..960 accepted
- accepted requests returned exact requested sizes

So the 384 long-cadence failure is a RUN/cadence issue, not an allocation geometry failure.

Do not rerun geometry again without contradictory evidence.

---

# Handoff consequence

The `post-coalesce-stale-v1` patch is now **built and product-matrix validated**, and its intended stale-wake recovery behavior has been directly observed to work at 432/480/576.

The current B5 branch may therefore be used as the preferred integration base for the separate native control-panel branch, while freezing WaveRT/mux runtime files there.

The unresolved 192k/384 forward delta=3 remains a separate runtime closure task and must be resolved before B5 first release is declared fully frozen.
