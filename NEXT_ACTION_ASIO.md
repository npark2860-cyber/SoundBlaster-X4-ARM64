# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety:

- never bypass local/global BUSY gates
- never intentionally reproduce the active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@075010999bed5c433f03d7421f2fa3a18221bd98`

Runtime/build markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

---

# Previous full silent matrix

Report generated `2026-09-04 13:21:47.16` passed completely, including 192k/384 output x2.

That established the measured first-release public contract:

`192 kHz: min=384 max=4800 preferred=384 granularity=48`

---

# Latest fail-safe validation — NEW blocker

Report generated `2026-09-04 13:50:09.41` loaded `runtime-failsafe-v1` successfully and passed:

- registration / KS probes
- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output cycle 1

The second 192k/384 output cycle failed after 332 callbacks.

Observed failure state:

- `worker=1`
- render packet discontinuities `rPkt=1`
- render position regressions `rPos=0`
- callback index errors `idx=0`
- output copy errors `outCopy=0`
- input copy errors `inCopy=0`
- `stop=-999`

Worker message:

`B5 worker RENDER failed: B5 RENDER RUN entered emergencySilence=OK log=%TEMP%\B5_RUNTIME_FAILURE.txt`

Final result:

`B5 PRODUCT VALIDATION RESULT: FAIL code=28`

`B5 INSTALL + PRODUCT VALIDATION: FAIL`

The new `reaper-48-480-output` case was not reached because the validator stops on the first failure.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_FAILSAFE_RUNTIME_192K_RENDER_PACKET_DISCONTINUITY.md`

---

# Interpretation

The failure-only logger did not cause the discontinuity. It runs only after mux-v3 has already detected a fatal worker condition.

Current render processing treats any PACKETCOUNT transition other than exactly `previous + 1` as a strict discontinuity. The latest report proves one such non-sequential render PACKETCOUNT observation at 192 kHz / 384 frames.

At this geometry the notification period is 2.0 ms. Event coalescing/scheduling delay is a plausible cause, but it is not yet proven because the current failure record does not capture the exact `previous -> current` packet transition.

Do not disable or loosen packet continuity checks merely to make the test pass.

---

# Fail-safe result

`emergencySilence=OK`

This proves the new fail-safe executed on an actual worker fatal path and successfully overwrote both render cyclic slots with silence before file/debug logging.

The worker still performs no pin stop/dispose/close; joined-worker-before-teardown remains intact.

---

# Immediate action

Do **not** rerun validation yet.

Retrieve and return the existing file from the same Windows session:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

Preserve it before another failure run overwrites it.

The file should contain the pre-silence stats snapshot from this exact 192k failure.

After examining that file:

1. if it identifies the packet transition sufficiently, implement the measured fix;
2. if not, add exact render discontinuity diagnostics (`previousPacket`, `currentPacket`, `delta`, presentation position/QPC) before another run;
3. keep the current strict packet/copy/index/position protections enabled;
4. only after the 192k render runtime issue is understood/fixed, rerun the matrix including the 48k/480 5-second REAPER-matched case;
5. then return to the planned native ASIO control-panel implementation.

Control-panel work remains required, but this newly measured render runtime failure takes priority.
