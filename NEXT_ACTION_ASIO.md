# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression requiring it.

Immutable safety:

- never bypass local/global BUSY gates
- never intentionally reproduce the active-render collision
- never tear hardware down before the worker is joined
- never weaken copy/index/position or unrecoverable packet checks merely to make validation pass

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@ca37f0e8427227733cd6082a50e20101312e3333`

Required runtime/build markers:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`

---

# Latest returned reports — v4 software regression identified

The first mux-v4 bundle loaded the intended v4/fail-safe markers and passed registration, registry, immutable property-only idle gate, and KS capability probing.

The very first 48 kHz / 240-frame output lifecycle then failed after exactly one callback:

- callbacks=1
- worker=1
- rPkt=0
- rPos=0
- idx=0
- outCopy=0
- inCopy=0

The v4 192 kHz cadence run also failed 384/432/480/576 after exactly one callback each with the same zero-fatal-counter pattern.

The preserved v4 runtime record at 192 kHz / 576 frames was decisive:

- render notifications=2
- callbacks=1
- notificationCoalesces=0
- recoveredCoalesces=0
- droppedBlocks=0
- packetDiscontinuities=0
- positionRegressions=0
- writes=1
- lastPacket=2
- emergencySilence=OK

Therefore this was not a new hardware cadence failure. `process_signaled_notification()` accepted the second notification with no packet or position error, but mux-v4 rejected it afterward.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`

---

# Root cause

Mux-v4 used `before.stats().last_packet` as the previous observed hardware PACKETCOUNT.

But `write_render_packet24()` also updated internal `stats_.last_packet` to the future write-ahead target packet.

Normal sequence therefore became:

1. PACKETCOUNT 1 observed;
2. callback writes future packet 2;
3. stats last_packet becomes 2;
4. next PACKETCOUNT correctly returns 2;
5. mux-v4 computes `2 - 2 = 0` and falsely fails.

The engine's private `previous_packet_` still tracked `1 -> 2` correctly, which is why `packetDiscontinuities=0`.

---

# Implemented correction

`X4WaveRtEngineB5::stats()` now returns a snapshot whose externally visible `last_packet` is the last hardware packet actually observed from PACKETCOUNT/GETREADPACKET whenever one exists.

The future render write target remains separate from that observable packet timeline.

Commits:

- `4acfadfc4131172d65e1877480b242c85c1416ce` — fix observed packet stats semantics
- `ca37f0e8427227733cd6082a50e20101312e3333` — embed `packet-stats-observed-v1`

The main workflow now refuses packaging unless both ARM64EC and Classic ARM64 DLLs contain all three required runtime markers.

The measured mux-v4 recovery policy itself is unchanged:

- render forward `delta == 2` -> recover exactly one collapsed notification as one explicit xrun block
- render duplicate/backward or `delta > 2` -> fatal
- capture packet discontinuity -> fatal
- render position regression -> fatal
- callback-index/copy/staging/join failure -> fatal

---

# Historical cadence evidence retained

The older mux-v3 cadence report remains useful background:

- 192k/384: one 5 s cycle PASS
- 192k/432: later `+2` skip
- 192k/480: later `+4` skip
- 192k/576: one cycle PASS, another later `+2` skip

This confirms that simply increasing the 192 kHz buffer does not eliminate notification coalescing. Do not raise the public 192 kHz buffer contract merely to hide this issue.

The repeated allocation-only geometry probe is unchanged:

- 48..336 frames rejected
- 384 first accepted
- 432..960 accepted

Current public 192 kHz contract therefore remains min/preferred 384 pending normal runtime closure.

---

# Immediate action

1. run manual workflow `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require both DLLs to contain all three markers listed above;
4. install the resulting ZIP with REAPER and other X4 clients closed;
5. run `install_and_validate_b5.cmd` **once**;
6. return `B5_PRODUCT_VALIDATION_REPORT.txt`;
7. do not run the old/broken v4 cadence bundle again;
8. successful recovery may show non-zero `renderCoalesces` / `renderDroppedBlocks`, but fatal counters must remain zero.

If the full product matrix passes, perform one ordinary REAPER test at the observed host geometry:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples

If a fatal failure occurs, stop retries and return the new `%TEMP%\B5_RUNTIME_FAILURE.txt`.

---

# After runtime PASS

Resume the required native ASIO control-panel milestone:

- own native Win32 UI, no Creative binary reuse
- `IASIO::controlPanel()` entry point
- current/effective sample rate and buffer
- buffer/latency setting + frames/ms
- sample-rate-aware limits
- no WaveRT pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel
- safe persistence/reopen/reset path
- lightweight diagnostics/save-report surface

After control-panel PASS, finish real REAPER output + real stereo input validation at 48/96 kHz, freeze B5 first release, then resume deferred CTCDC/CTIntrfu work.
