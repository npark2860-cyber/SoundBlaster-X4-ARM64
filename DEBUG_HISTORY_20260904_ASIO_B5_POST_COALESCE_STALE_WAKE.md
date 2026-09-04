# DEBUG HISTORY — B5 mux-v4 post-coalesce stale render wake

Updated: 2026-09-04 KST

## Evidence

The packet-stats alias regression was fixed and the next B5 bundle returned a full product validation PASS.

`B5_PRODUCT_VALIDATION_REPORT(10).txt` generated `2026-09-04 16:16:13.09`:

- registration / registry verification: PASS
- KS idle/capability probes: PASS
- 48k/240 output x3: PASS
- 48k/240 duplex x2: PASS
- 96k/240 duplex x2: PASS
- REAPER-matched 48k/480 output for 5 seconds: PASS, 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2: PASS
- 48k/96, 48k/4800, 48k/512: PASS
- ASIO capability probe: PASS
- final `B5 INSTALL + PRODUCT VALIDATION: PASS`

No render coalesce happened during that bundled matrix.

The dedicated 192 kHz cadence run then reproduced the intended +2 recovery path and exposed a second edge case.

At 192k/432 cycle 2:

1. normal observed packet sequence reached 1941;
2. the next observed packet was 1943;
3. mux-v4 correctly emitted:
   `B5 RENDER COALESCE RECOVERED previous=1941 missed=1942 current=1943 droppedBlocks=1`
4. the immediately following render wake still returned PACKETCOUNT 1943;
5. the unarmed strict engine path treated that as:
   `previous=1943 expected=1944 current=1943 delta=0`
6. worker failed and runtime failsafe silenced output.

The same pattern occurred at 192k/480:

- `3485 -> 3487` recovered
- immediate next wake returned `3487` again
- strict delta=0 fatal

192k/384 passed 5 seconds. 192k/576 passed both 10-second cycles. Those cadence differences are diagnostic only; the geometry allocation minimum remains 384 frames.

## Diagnosis

This is not a reason to globally tolerate duplicate PACKETCOUNT values.

The measured sequence is narrower:

`forward +2 coalesce -> successful one-block catch-up -> immediately following auto-reset wake may report the same current PACKETCOUNT once`

Therefore only the first render wake immediately after a successful +2 recovery may consume one same-packet stale wake without callback or hardware write.

All other duplicate/backward/larger jumps remain fatal.

## Implemented fix

B5 branch adds marker:

`post-coalesce-stale-v1`

Behavior:

- successful +2 recovery arms one stale-wake allowance;
- if the immediately following Render wake returns exactly the same PACKETCOUNT, WaveRT processing returns `NoData` without advancing callback or notification statistics;
- mux consumes that wake, increments `renderStaleWakes`, clears the allowance and continues waiting;
- if the next wake advances normally, the allowance is simply cleared;
- a second duplicate, an unarmed duplicate, backward jump, or forward delta >2 remains fatal;
- capture packet rules are unchanged;
- fail-safe silence and joined-worker-before-teardown remain unchanged.

Touched B5 files only:

- `src/asio-arm64-stage-b0/wavert_engine_b5.h`
- `src/asio-arm64-stage-b0/wavert_engine_b5_signaled.inl`
- `src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

The manual workflow now requires all runtime markers in both ARM64EC and Classic ARM64 DLLs:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

## Next validation

Do not rerun geometry; the allocation boundary is unchanged.

Build the new B5 bundle and run the dedicated cadence probe once because it directly reproduces this edge case. Desired evidence when a +2 occurs:

- `B5 RENDER COALESCE RECOVERED ...`
- optionally `B5 RENDER POST-COALESCE STALE WAKE consumed ...`
- worker remains alive
- strict `rPkt=0`, `rPos=0`, copy/index errors=0
- stop=0 / workerJoined=YES

If cadence completes without fatal, run the bundled product validation once as final regression, then perform one normal REAPER 48k/480 audible playback test. After that, resume the native ASIO control-panel milestone.