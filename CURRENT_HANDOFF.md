# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 productization branch:

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit on that branch:

`64e34b48714789ab17fba57be34b054f2170b4e9`

The later branch HEAD only updates `README_B5_PRODUCTIZATION.md`.

At the start of a later chat, verify actual GitHub main/branch HEADs again. GitHub is source of truth; do not reconstruct state from conversation memory.

---

# Read order for the control-panel continuation

1. `CURRENT_HANDOFF.md`
2. `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
3. `PROMPT_ASIO_CONTROL_PANEL.md`
4. `NEXT_ACTION_ASIO.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
9. older histories only as needed

CTCDC/CTIntrfu remains deferred until the B5 first-release ASIO product surface and host-level validation are closed.

---

# Runtime validation state — keep the evidence classes distinct

## Earlier REAPER-proven reference

`ca37f0e8427227733cd6082a50e20101312e3333`

This earlier built bundle:

- returned a full B5 product-validation PASS;
- passed the REAPER-matched 48 kHz / 480-frame silent 5-second case;
- was then used by the user for ordinary REAPER playback, and the user explicitly reported no playback problem.

This remains the last explicitly confirmed **real audible REAPER** reference unless a later user report says otherwise.

## Latest built/product-validated B5 source

The current B5 branch at `4475fc17b70f372fe317fa201f201e8dc5543f9f`, including runtime code `64e34b48714789ab17fba57be34b054f2170b4e9`, has now also been built and tested.

Latest product report generated `2026-09-04 16:32:53.73`:

`B5 INSTALL + PRODUCT VALIDATION: PASS`

Passed on the latest `post-coalesce-stale-v1` bundle:

- registration / registry verification
- immutable property-only KS idle gate
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

So the latest runtime patch is no longer "unbuilt" or "unvalidated". It is **built and product-matrix validated**.

However, do not overstate this as complete runtime closure because the dedicated long 192 kHz cadence probe still has one strict failure at 384 frames, described below.

The user has not explicitly stated in this handoff that the latest `post-coalesce-stale-v1` build itself was also used for a new audible REAPER session. Do not silently convert the latest silent/product validation into real-audio evidence.

---

# Immutable runtime safety

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

Never weaken render position, callback-index, render/capture copy, capture packet integrity, worker-join, or unrecoverable packet checks merely to make validation pass.

`runtime-failsafe-v1` may overwrite Render cyclic contents with silence on a fatal worker path but must never stop/close/dispose pins from the worker itself.

Validated B4D remains frozen.

Control-panel work must not refactor or cosmetically clean the validated WaveRT/mux path.

---

# B5 public ASIO contract

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

Allocation-only 192 kHz geometry was rechecked again on the latest bundle and is unchanged:

- 48..336 rejected with Win32 87
- 384 first accepted
- 432..960 accepted
- accepted requests return exact requested buffer size

Do not rerun geometry without contradictory evidence.

The long-cadence problem below is not an allocation failure and does not by itself justify changing the public geometry contract from the control-panel tab.

---

# Current runtime/build markers

Both ARM64EC and Classic ARM64 DLLs are expected to contain:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`
- `post-coalesce-stale-v1`

The manual productization workflow enforces these markers. Push/PR auto-builds remain disabled; `workflow_dispatch` is retained.

---

# Runtime fail-safe and render coalescing diagnosis

The original real REAPER failure was a loud sustained drone/buzz while the host remained alive. The prior fatal worker path could leave WaveRT RUN with stale cyclic audio repeating.

`runtime-failsafe-v1` now:

1. snapshots failure state;
2. zeroes both Render cyclic slots first;
3. performs no worker-side pin teardown;
4. only then emits OutputDebugString / `%TEMP%\B5_RUNTIME_FAILURE.txt`.

Captured failures have directly shown `emergencySilence=OK`.

A strict 48 kHz / 240 run previously recorded:

`previous=74 expected=75 current=76 delta=2`

Earlier 192 kHz runs showed the same +2 pattern, proving it is not primarily a 192 kHz minimum-period issue.

Mux-v4 therefore treats exactly one forward Render `delta == 2` as an explicit one-block xrun/coalesced notification:

1. synthesize the missing ASIO callback to preserve host double-buffer/sample timeline;
2. discard that already-late block's output;
3. run the current callback normally;
4. write only to the next future WaveRT packet;
5. continue streaming.

The later `post-coalesce-stale-v1` rule permits exactly one same-PACKETCOUNT stale wake only immediately after a successful +2 recovery. That stale wake produces no callback and no Render write.

Unarmed duplicate, backward packet, capture discontinuity, position regression, callback-index/copy/staging/join failure remain strict fatal conditions.

---

# Latest 192 kHz cadence validation on the current build

Latest report generated `2026-09-04 16:35` on the `post-coalesce-stale-v1` build.

## 432 frames — PASS x2

The fix was exercised for real, not merely loaded.

Cycle 1 recovered two +2 events and consumed two post-coalesce stale wakes, then exited normally:

- `renderCoalesces=2`
- `renderDroppedBlocks=2`
- `renderStaleWakes=2`
- stop=0 / workerJoined=YES

Cycle 2 recovered three +2 events and consumed three stale wakes, then exited normally.

## 480 frames — PASS x2

Cycle 1:

- one +2 recovery
- one stale wake consumed
- stop=0

Cycle 2:

- four +2 recoveries
- three stale wakes consumed
- stop=0

This proves the one-shot stale-wake fix works for the measured `+2 -> same packet once` sequence.

## 576 frames — PASS x2

Both cycles completed normally while also exercising +2/stale recovery.

## 384 frames — strict FAIL remains

The first 384-frame cycle encountered:

`previous=1375 expected=1376 current=1378 delta=3`

Result:

- callbacks=1375
- worker=1
- rPkt=1
- rPos=0
- copy/index errors=0
- `emergencySilence=OK`

This is **not** the stale-wake edge case. It is a larger forward jump and remains fatal by design.

Do not weaken `delta > 2` from the control-panel work merely to make the cadence probe pass. Treat this as a separate runtime follow-up before final B5 release closure.

Current cadence summary:

- 192k/384: FAIL, strict forward delta=3
- 192k/432: PASS x2 with actual coalesce/stale recovery
- 192k/480: PASS x2 with actual coalesce/stale recovery
- 192k/576: PASS x2 with actual coalesce/stale recovery

---

# Current driver facts relevant to the control panel

Control-panel implementation target is `src/asio-arm64-stage-b0/driver_b5.cpp` plus narrowly scoped new shared panel source/header files if needed.

Current facts:

- `ASIOError controlPanel() override { return ASE_NotPresent; }`
- `HMODULE g_module` already exists and is assigned in `DllMain(DLL_PROCESS_ATTACH)`
- `init(void* sysHandle)` currently discards `sysHandle`; if an owner window is desired, inspect ASIO/host semantics before retaining it rather than assuming blindly
- `setSampleRate()` rejects changes while buffers/worker/RUN are active
- `createBuffers()` validates the host-supplied rate-specific size and rejects already-created/prepared state
- `createBuffers()` stores the host-provided `bufferSize` in `buffer_frames_`
- `createBuffers()` stores `callbacks_ = *callbacks`
- `disposeBuffers()` clears `callbacks_`
- `getLatencies()` reports active `buffer_frames_` when buffers exist, otherwise the rate-specific preferred value
- `getBufferSize()` currently reports fixed rate-specific preferred values

Important ASIO semantic constraint:

The host supplies the actual `bufferSize` argument to `createBuffers()`. The panel must not silently override an explicit host-provided buffer size. A panel-selected latency/buffer value therefore needs a deliberate next-open/preferred/reset/reopen contract rather than hidden substitution inside `createBuffers()`.

Any `kAsioResetRequest` use must be designed only after checking callback lifetime and reentrancy. Do not fire host callbacks casually from a modal dialog or while teardown is in progress.

---

# Native ASIO control panel — immediate product milestone

The user is moving control-panel work to a fresh chat now.

This milestone can proceed independently from the remaining 192k/384 cadence delta=3 issue, provided the control-panel branch does not alter WaveRT/mux/runtime safety code.

Required first-release panel:

- own native Win32 UI; no Creative control-panel binary reuse
- compact, credible production UI rather than a debug form
- opened through `IASIO::controlPanel()`
- identity: `Sound Blaster X4 ARM64 ASIO B5`
- current/effective sample rate display
- current/effective buffer display
- latency/buffer selection with frames and milliseconds
- 48/96 limits: 96..4800, granularity 48, default/preferred 240
- 192 limits: 384..4800, granularity 48, default/preferred 384
- 512 compatibility selectable
- if host currently has e.g. 480 frames active, display 480 as the effective current buffer; do not misleadingly show 240 as current
- opening the panel must not create WaveRT pins
- opening the panel must not probe hardware aggressively
- no live geometry mutation while ASIO buffers or RUN are active
- deterministic Apply / OK / Cancel
- persisted user preference for the next safe reopen path
- explicit handling of whether host reset/reopen is needed
- lightweight diagnostics surface retained as a product feature

Diagnostics should stay realtime-safe:

- no per-callback file writes
- existing fatal `%TEMP%\B5_RUNTIME_FAILURE.txt` / OutputDebugString remains failure-only
- panel may show/copy/save lightweight state such as driver version, sample rate, active buffer, worker status/last status and failure-log location
- verbose diagnostics, if added, must be opt-in and must not put blocking file I/O in the normal callback path

---

# Branching guidance for the fresh control-panel tab

Preferred isolation strategy now that the current B5 branch has been built and product-matrix validated:

- use the current B5 productization branch HEAD `4475fc17b70f372fe317fa201f201e8dc5543f9f` as the **preferred integration base** for the dedicated control-panel branch, because it contains the already product-validated `post-coalesce-stale-v1` runtime improvements;
- explicitly record that 192k/384 long cadence still has the separate strict delta=3 issue;
- create `exp/windows-arm64-asio-b5-control-panel` from that exact commit after re-verifying refs;
- freeze WaveRT/mux files on the UI branch; do not attempt to solve the delta=3 issue as part of panel work;
- if runtime work later advances the B5 productization branch, merge/cherry-pick that runtime fix explicitly after each side is validated.

The earlier `ca37f0e...` remains the last explicit real-audible REAPER reference, but it is no longer necessary to use it as the UI branch base merely to avoid an "unvalidated" stale-wake patch; that patch has now been built and product-tested.

Before creating the UI branch, verify current refs in GitHub and report the intended base/diff. No unrelated cleanup.

---

# Control-panel validation sequence

First validate the panel independently from streaming:

1. ARM64EC + Classic ARM64 compile/link PASS;
2. registration side-by-side remains correct;
3. host can invoke `controlPanel()` without crash;
4. opening/closing/canceling the panel creates no WaveRT pins;
5. panel shows correct current sample rate and effective buffer state;
6. rate-specific buffer choices and 512 compatibility are enforced;
7. Cancel changes nothing;
8. Apply/OK persistence is deterministic;
9. active-buffer/RUN state cannot be mutated unsafely;
10. if a reset/reopen mechanism is implemented, validate it separately and prove no callback-lifetime/reentrancy issue.

Only after panel behavior is stable, run the normal B5 product validation and one ordinary REAPER check. Do not turn panel development into another WaveRT stress campaign.

The unresolved 192k/384 `delta=3` cadence item remains a separate runtime closure task after/alongside panel work and must be resolved before calling B5 first release fully frozen.

After panel PASS and remaining runtime closure, finish real output + real stereo input validation at 48/96 kHz, freeze the B5 first release, then resume deferred CTCDC/CTIntrfu static analysis.
