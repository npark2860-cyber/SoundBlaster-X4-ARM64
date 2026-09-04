# DEBUG HISTORY — ASIO B5 mux-v4 stats alias regression

Date: 2026-09-04 KST

## Returned runtime evidence

The first `dual-event-mux-v4-coalesce-recovery` bundle was runtime-tested and returned four reports generated around 15:48-15:49 KST.

### Product validation

`B5_PRODUCT_VALIDATION_REPORT(9).txt` loaded the intended runtime markers:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`

Registration, registry verification, the immutable property-only KS idle gate, and KS capability probe all passed.

The first lifecycle case then failed immediately:

- 48 kHz / 240 frames / output-only
- callbacks = 1
- worker = 1
- render packet discontinuities = 0
- render position regressions = 0
- callback-index errors = 0
- render/capture copy errors = 0
- stop = -999 / validation code 28

The worker printed only the stale engine message `B5 RENDER RUN entered` before the fail-safe silenced output.

### v4 cadence report

The v4 cadence run failed every candidate after exactly one callback:

- 192k / 384
- 192k / 432
- 192k / 480
- 192k / 576

Every case had `rPkt=0`, `rPos=0`, and no copy/index error. Therefore this v4 cadence report is invalid as hardware-stability evidence; all candidates were stopped by a software regression before a real cadence measurement occurred.

### Preserved v4 runtime failure

The preserved `%TEMP%\B5_RUNTIME_FAILURE.txt` from 192 kHz / 576 frames recorded:

- callbacks = 1
- render notifications = 2
- notificationCoalesces = 0
- recoveredCoalesces = 0
- droppedBlocks = 0
- packetDiscontinuities = 0
- positionRegressions = 0
- writes = 1
- lastPacket = 2
- emergencySilence = OK

This proves `process_signaled_notification()` itself accepted the second notification and found no packet/position error. The failure occurred in the mux-v4 post-check after that successful engine call.

### Geometry

The repeated 192 kHz geometry probe remained unchanged:

- 48..336 rejected with Win32 87
- 384 first accepted
- 432..960 accepted
- allocation-only geometry PASS

No geometry contract change is justified by the v4 regression.

## Root cause

Mux-v4 originally validated the just-returned hardware packet using:

`packet - before.last_packet`

However `X4WaveRtB5Stats::last_packet` was also being updated by `write_render_packet24()` to the write-ahead target packet.

Normal sequence example:

1. hardware PACKETCOUNT reports packet 1;
2. ASIO callback runs;
3. B5 writes future packet 2 (`writePacket = masterPacket + 1`);
4. `write_render_packet24()` changes internal `stats_.last_packet` to 2;
5. next hardware PACKETCOUNT correctly reports packet 2;
6. mux-v4 compares current 2 against `before.last_packet == 2` and computes delta 0;
7. mux-v4 returns failure even though the engine's private `previous_packet_` correctly saw normal `1 -> 2` continuity.

That exactly explains the runtime combination:

- two accepted notifications
- one callback
- zero packet discontinuities
- zero position/copy/index failures
- stale `RUN entered` reason

This was a software bookkeeping alias, not a newly discovered WaveRT hardware fault.

## Fix

B5 now exposes `stats()` as a snapshot whose externally visible `last_packet` is normalized to the engine's private last observed hardware packet (`previous_packet_`) whenever one exists.

This preserves the established write-ahead rule while separating two meanings:

- observed completed hardware packet
- future render packet written into the cyclic buffer

The mux-v4 coalescing detector can therefore compare against the previous observed PACKETCOUNT again.

Commits:

- `4acfadfc4131172d65e1877480b242c85c1416ce` — fix B5 render packet stats semantics for mux-v4
- `ca37f0e8427227733cd6082a50e20101312e3333` — embed `packet-stats-observed-v1` runtime marker

Current B5 branch after the fix:

`exp/windows-arm64-asio-b5-capability-productization@ca37f0e8427227733cd6082a50e20101312e3333`

## Build guard

The manual B5 productization workflow now requires all built ARM64EC and Classic ARM64 DLLs to contain:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`

This prevents the already-tested broken v4 binary from being mistaken for the fixed v4 bundle.

Main workflow commit:

`0bdb972dbe530fb398a75fd4e2fa0545c7d12719`

## Safety unchanged

No BUSY gate, pin ownership rule, teardown rule, packet discontinuity policy, position check, callback-index check, copy check, or fail-safe rule was weakened.

`delta=2` remains the only render discontinuity reclassified as a recoverable notification coalesce. Larger/duplicate/backward render transitions remain fatal. Capture packet discontinuity remains fatal.

## Next action

Do not rerun the old cadence bundle.

1. Run manual `Build ASIO B5 Productization` on current B5 head.
2. Require the three runtime markers above in both DLLs.
3. Run `install_and_validate_b5.cmd` once.
4. If the full product matrix passes, inspect `renderCoalesces` / `renderDroppedBlocks` in the worker summaries.
5. Then perform one normal REAPER 48 kHz / 480-frame audible playback test.
6. If REAPER remains stable, resume the native ASIO control-panel milestone.
7. If any fatal recurs, collect the new `%TEMP%\B5_RUNTIME_FAILURE.txt` once and stop retrying.
