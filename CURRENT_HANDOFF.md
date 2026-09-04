# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 branch HEAD:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit:

`64e34b48714789ab17fba57be34b054f2170b4e9`

The branch HEAD after that runtime-code commit only updates `README_B5_PRODUCTIZATION.md`.

At a later chat start, verify actual GitHub branch/main HEADs again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_48K_RENDER_COALESCE_RECOVERY_V4.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
7. `NEXT_ACTION_ASIO.md`
8. older histories only as needed

CTCDC/CTIntrfu remains deferred until B5 first-release ASIO product surface and host-level validation are closed.

---

# Immutable safety

Never bypass BUSY.

B5 retains:

1. Render Pin 1 local/global preflight at ASIO `init()`;
2. Render Pin 1 local/global re-check before Render `KsCreatePin`;
3. Capture Pin 4 local/global re-check before Capture `KsCreatePin`;
4. mandatory joined worker before hardware teardown.

Historical collision class must never be intentionally reproduced:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path

Never weaken render position, callback-index, render/capture copy, capture packet integrity, or joined-worker checks merely to make validation pass.

`runtime-failsafe-v1` may overwrite Render cyclic contents with silence on a fatal worker path but must never stop/close/dispose pins from the worker itself.

Validated B4D remains frozen.

---

# B5 public contract

Channels/sample type:

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- 192 kHz reports zero inputs
- output 48/96/192 kHz

Buffers:

- 48/96 kHz: min 96, max 4800, preferred 240, granularity 48
- 192 kHz: min 384, max 4800, preferred 384, granularity 48
- 512 compatibility exception remains accepted

Other:

- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- NotificationCount=2

The allocation-only 192 kHz geometry probe has repeatedly confirmed:

- 48..336 rejected with Win32 87
- 384 first accepted
- 432..960 accepted
- accepted requests return exact requested buffer size

Do not raise the 192 kHz minimum merely to hide notification behavior.

---

# Current runtime/build markers

Both ARM64EC and Classic ARM64 DLLs must contain:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

The manual productization workflow enforces all four markers. Push/PR auto-builds remain disabled; `workflow_dispatch` is retained.

---

# Real REAPER regression that started the runtime work

REAPER ARM64EC showed B5 active at:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples
- about 10 ms in / 10 ms out

During actual playback the output became a loud sustained drone/buzz while REAPER itself remained alive and left no useful log.

The prior fatal path could leave WaveRT RUN with stale cyclic audio repeating. `runtime-failsafe-v1` now snapshots diagnostics, zeroes both render slots first, then writes `%TEMP%\B5_RUNTIME_FAILURE.txt` / OutputDebugString. Captured failures have directly shown `emergencySilence=OK`.

---

# Render notification diagnosis

A strict 48 kHz / 240 run recorded:

`previous=74 expected=75 current=76 delta=2`

Earlier 192 kHz runs showed the same +2 pattern, proving it is not primarily a 192 kHz minimum-period issue.

Mux-v4 therefore treats exactly one forward Render `delta == 2` as an explicit one-block xrun/coalesced notification:

1. synthesize the missing ASIO callback to preserve host double-buffer/sample timeline;
2. discard that already-late block's output;
3. run the current callback normally;
4. write only to the next future WaveRT packet;
5. continue streaming.

Duplicate/backward/larger Render jumps remain fatal unless covered by the one narrow post-coalesce stale-wake rule below. Capture discontinuity remains fatal.

---

# First mux-v4 software regression — fixed

The first mux-v4 build failed after one callback even though engine packet/position counters were clean.

Root cause: external `stats().last_packet` had been polluted by Render write-ahead N+1 writes.

Fix:

- `stats()` now exposes the last packet actually observed from PACKETCOUNT/GETREADPACKET;
- write-ahead target statistics no longer masquerade as completed hardware packet state;
- marker `packet-stats-observed-v1` added.

Relevant commits:

- `4acfadfc4131172d65e1877480b242c85c1416ce`
- `ca37f0e8427227733cd6082a50e20101312e3333`

---

# Latest product validation — PASS

Report generated `2026-09-04 16:16:13.09`:

`B5 INSTALL + PRODUCT VALIDATION: PASS`

Passed:

- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- REAPER-matched 48k/480 output for 5 seconds: 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2
- 48k/96
- 48k/4800
- 48k/512 compatibility
- ASIO capability probe

96k duplex still shows roughly 27 capture phase misses in the short silent window, with no strict packet/index/copy failure. Do not cosmetically hide this; real-signal input validation is still required later.

---

# Latest cadence evidence — post-coalesce stale wake

The dedicated 192 kHz cadence run reproduced a real +2 and proved the first recovery step itself works.

At 432 frames:

`1941 -> 1943`

was recovered:

`B5 RENDER COALESCE RECOVERED previous=1941 missed=1942 current=1943 droppedBlocks=1`

The immediately following render wake then returned the same current PACKETCOUNT:

`1943 -> 1943`

and the old strict duplicate rule killed the worker.

The same pattern occurred at 480 frames:

`3485 -> 3487` recovered, then `3487 -> 3487` fatal.

384 passed its 5-second cadence cycle. 576 passed both 10-second cycles. These rate/frame differences are diagnostic only and do not justify changing the allocation contract.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`

---

# Implemented one-shot post-coalesce stale-wake fix

Latest runtime-code commit:

`64e34b48714789ab17fba57be34b054f2170b4e9`

New rule:

- only after a successful forward Render `delta == 2` recovery, arm exactly one stale-wake allowance;
- if the immediately following render wake returns the same PACKETCOUNT, classify it as `post-coalesce-stale-v1`;
- issue no ASIO callback;
- perform no Render write;
- do not advance callback/sample timeline or hardware-notification stats;
- increment worker diagnostic `renderStaleWakes`;
- clear the allowance and continue waiting.

If the next wake advances normally, the allowance is simply cleared.

Still fatal:

- any unarmed duplicate
- a second duplicate after the consumed stale wake
- backward Render packet
- Render delta >2
- Capture packet discontinuity
- render position regression
- callback-index/copy/staging/join failure

Touched only:

- `src/asio-arm64-stage-b0/wavert_engine_b5.h`
- `src/asio-arm64-stage-b0/wavert_engine_b5_signaled.inl`
- `src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

B4D core remains untouched.

---

# Immediate next action

1. run manual `Build ASIO B5 Productization` on current B5 branch HEAD;
2. require ARM64EC + Classic ARM64 compile/link PASS and all four markers;
3. install the new ZIP with all other X4 clients closed;
4. **do not rerun geometry**;
5. run `probe_b5_192k_cadence.cmd` once because it directly reproduced the stale-wake edge case;
6. return `B5_192K_CADENCE_REPORT.txt`;
7. desired evidence if +2 occurs: coalesce recovered, optional stale wake consumed, no worker fatal, strict counters zero, stop=0 / workerJoined=YES;
8. if cadence passes, run `install_and_validate_b5.cmd` once as final shared-worker regression;
9. then perform one normal REAPER 48k/480 audible playback test;
10. if fatal occurs, stop retries and return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately.

---

# ASIO control panel — still binding next product milestone

After runtime closure, resume the native control panel. Current driver still has:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

Required first-release panel:

- own native Win32 UI; no Creative binary reuse
- `IASIO::controlPanel()` entry point
- current/effective sample rate and active buffer
- buffer/latency frames + ms
- sample-rate-aware limits, including 192 kHz min/preferred 384 and 512 compatibility
- no WaveRT pin creation merely from opening panel
- no live geometry mutation while buffers/RUN active
- deterministic Apply/OK/Cancel
- safe persistence / host reset-reopen behavior
- lightweight diagnostics/save-report support

After panel PASS, finish real output + real stereo input validation at 48/96 kHz, freeze B5 first release, then resume CTCDC/CTIntrfu static-analysis work.