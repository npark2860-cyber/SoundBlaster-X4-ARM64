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

`exp/windows-arm64-asio-b5-capability-productization@1e4b9527269a84115f4aa43a09fdf3c9a7c31dd3`

Runtime/build markers:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`

---

# Latest measured blocker — not 192 kHz specific

The latest product report was generated `2026-09-04 15:26:42.62`.

Registration / registry / property-only idle gate / KS capability probing passed.

48 kHz / 240 output-only:

- cycle1 PASS, callbacks=139
- cycle2 PASS, callbacks=142
- cycle3 failed after callbacks=74

Exact failure:

`B5 RENDER PACKET DISCONTINUITY previous=74 expected=75 current=76 delta=2`

The preserved runtime file recorded:

- rate=48000
- frames=240
- render notifications=75
- callbacks=74
- lastPacket=76
- packetDiscontinuities=1
- positionRegressions=0
- index/copy errors=0
- `emergencySilence=OK`

This is the same forward `+2` pattern previously seen at 192 kHz (`332 -> 334`). Because it now occurs at 48 kHz / 240 frames = 5.0 ms, the earlier 192 kHz minimum-period hypothesis is superseded.

The shared issue is the auto-reset WaveRT notification primitive: event state is not a counting semaphore, so a hardware packet can advance without a distinct user-mode wake for every period.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_48K_RENDER_COALESCE_RECOVERY_V4.md`

---

# Implemented fix — mux v4 coalesce recovery

Exactly one measured forward Render transition `delta == 2` is now classified as a recoverable one-block xrun / notification coalesce.

For example `74 -> 76`:

1. invoke a synthetic ASIO callback for missed master packet 75;
2. preserve the alternating ASIO buffer index and callback/sample timeline;
3. deliberately discard that synthetic callback's output because hardware packet 76 has already completed;
4. invoke the normal current master packet 76 callback;
5. write only its output to future WaveRT packet 77;
6. continue streaming instead of killing the worker.

Duplex behavior:

- synthetic missed callback gets zero-filled input;
- it does not consume capture staging;
- the current callback resumes normal staged capture consumption.

New diagnostics:

- engine `notification_coalesces`
- worker `renderCoalesces`
- worker `renderDroppedBlocks`
- fatal record includes coalescing history

This is not a general tolerance increase.

Still fatal:

- render duplicate/backward transition
- render forward `delta > 2`
- capture packet discontinuity
- render position regression
- callback-index error outside explicit catch-up
- render/capture copy failure
- capture staging failure / sustained starvation
- worker failure

`runtime-failsafe-v1` remains the fatal fallback and still performs no worker-side pin teardown.

No `kAsioResyncRequest` is sent in this first recovery implementation; avoid adding host-reset side effects until runtime shows they are needed.

---

# 192 kHz cadence probe

`probe_b5_192k_cadence.cmd` remains packaged for diagnosis but is **not the immediate next test**.

Do not raise the 192 kHz min/preferred buffer merely to hide notification coalescing now proven at 48 kHz too.

---

# Immediate action

1. run manual workflow `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require both DLLs to contain `dual-event-mux-v4-coalesce-recovery` and `runtime-failsafe-v1`;
4. install the resulting ZIP with REAPER/other X4 clients closed;
5. run `install_and_validate_b5.cmd` **once**;
6. return `B5_PRODUCT_VALIDATION_REPORT.txt`;
7. do not run the dedicated cadence probe first;
8. inspect any `renderCoalesces` / `renderDroppedBlocks`; they may be non-zero on a successful recovered xrun, while strict fatal counters must remain zero.

If product validation passes, perform one ordinary REAPER test at the already-observed host geometry:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples

If a fatal failure occurs, do not repeatedly retry. Return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately.

---

# After runtime PASS

Resume the required native ASIO control-panel milestone:

- own native Win32 UI, no Creative binary reuse
- `IASIO::controlPanel()` entry point
- current sample rate
- buffer/latency setting + frames/ms
- sample-rate-aware limits
- no WaveRT pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel
- safe persistence/reopen/reset path
- lightweight diagnostics/save-report surface later

After control-panel PASS, finish real REAPER output + real stereo input validation at 48/96 kHz, freeze B5 first release, then resume deferred CTCDC/CTIntrfu work.
