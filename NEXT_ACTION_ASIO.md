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
- never weaken packet/copy/index/position checks merely to make validation pass

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@a62df8395888298a4be96c7cf14ed782d905e188`

Runtime/build markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

---

# Latest measured blocker — 192 kHz / 384 render cadence

The fail-safe validation generated `2026-09-04 13:50:09.41`.

It passed registration, KS probes, 48k/240 output x3, 48k/240 duplex x2, 96k/240 duplex x2 and the first 192k/384 output cycle.

The second 192k/384 output cycle failed after 332 callbacks:

- `worker=1`
- `rPkt=1`
- `rPos=0`
- `idx=0`
- `outCopy=0`
- `inCopy=0`
- `stop=-999`

`runtime-failsafe-v1` reported:

`emergencySilence=OK`

The preserved `%TEMP%\B5_RUNTIME_FAILURE.txt` recorded:

- render notifications = 333
- callbacks = 332
- last render packet = 334
- render packet discontinuities = 1
- no position/index/copy error

Given the known 1-based WaveRT render PACKETCOUNT semantics, this identifies the transition as:

`332 -> 334`

Packet 333 was not observed as its own worker notification.

384 frames at 192 kHz is 2.000 ms. Current evidence is consistent with one coalesced/missed auto-reset render notification or equivalent user-mode service delay across two periods.

Do not relax the discontinuity rule.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_FAILSAFE_RUNTIME_192K_RENDER_PACKET_DISCONTINUITY.md`

---

# Diagnostic fix already implemented

The WaveRT signaled path now writes exact failure text for non-sequential packets:

`previous / expected / current / delta`

and records previous/current presentation positions on a render position regression.

Commit:

`dc1b16adb7788f27443be9efeb3bdc56ad51536d`

No strict check was changed.

---

# Immediate action — measure stable 192 kHz RUN cadence

A dedicated strict probe is now available through the existing product validation host:

`x4-asio-stage-b5-product-validation.exe --cadence-192`

Packaged runner:

`probe_b5_192k_cadence.cmd`

Candidates:

- 384 frames = 2.000 ms, 1 x 5 s
- 432 frames = 2.250 ms, 2 x 10 s
- 480 frames = 2.500 ms, 2 x 10 s
- 576 frames = 3.000 ms, 2 x 10 s

Every candidate keeps the current fatal packet-discontinuity check enabled.

Required sequence:

1. run manual workflow `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 build and both runtime markers PASS;
3. install/register the new bundle with REAPER and other X4 clients closed;
4. run `probe_b5_192k_cadence.cmd` once;
5. return `B5_192K_CADENCE_REPORT.txt`;
6. choose any 192 kHz contract change only from the measured first sustained stable cadence;
7. do not intentionally reproduce audible buzz.

The normal product matrix now runs the REAPER-matched `48k/480 output / 5 s` case before the 192 kHz case so it is no longer hidden by a later 192 kHz failure.

---

# After cadence result

If 384 is unstable but a larger candidate is consistently stable:

- update the 192 kHz min/preferred to the first measured stable candidate;
- update latency/buffer contract validation;
- rerun the full matrix;
- then do one normal REAPER audible test.

If larger candidates also show packet skips, do not raise the buffer blindly; diagnose the notification scheduling path instead.

After this runtime blocker is closed, resume the planned native ASIO control panel, then final real output + real stereo input validation, then freeze B5 first release and resume CTCDC/CTIntrfu.
