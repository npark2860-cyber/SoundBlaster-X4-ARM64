# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

## B5 state — keep these two references distinct

### Last runtime-validated B5 reference

`ca37f0e8427227733cd6082a50e20101312e3333`

This is the build that actually produced the returned 2026-09-04 16:16 product report and was subsequently used by the user in REAPER without a playback problem.

Measured on that built bundle:

- `B5 INSTALL + PRODUCT VALIDATION: PASS`
- 48k/240 output x3 PASS
- 48k/240 duplex x2 PASS
- 96k/240 duplex x2 PASS
- REAPER-matched 48k/480 output for 5 s: 500 callbacks, stop=0, workerJoined=YES
- 192k/384 output x2 PASS
- 48k/96 PASS
- 48k/4800 PASS
- 48k/512 compatibility PASS
- ASIO capability probe PASS
- user then performed ordinary REAPER playback and explicitly reported no problem

Do not describe the earlier loud sustained drone/buzz as still reproducing on this validated build. It motivated the fail-safe work, but the user's latest real REAPER observation on the validated bundle was normal playback.

### Current B5 branch HEAD — newer but not runtime-validated yet

`exp/windows-arm64-asio-b5-capability-productization@4475fc17b70f372fe317fa201f201e8dc5543f9f`

Latest runtime-code commit on that branch:

`64e34b48714789ab17fba57be34b054f2170b4e9`

The later branch HEAD only updates `README_B5_PRODUCTIZATION.md`.

The post-`ca37f0e` runtime delta adds narrow handling for an immediately repeated Render PACKETCOUNT wake after a successfully recovered `delta=2` coalesce. Runtime/build marker:

`post-coalesce-stale-v1`

**This newer stale-wake patch has not been built or runtime-validated yet.** Never call it proven. Do not make control-panel work depend on it.

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

Allocation-only 192 kHz geometry is already established:

- 48..336 rejected with Win32 87
- 384 first accepted
- 432..960 accepted
- accepted requests return exact requested buffer size

Do not rerun geometry without contradictory evidence and do not raise the 192 kHz minimum merely to hide notification behavior.

---

# Current driver facts relevant to the control panel

Control-panel implementation target is `src/asio-arm64-stage-b0/driver_b5.cpp` plus narrowly scoped new shared panel source/header files if needed.

Current facts from the validated `ca37f0e` source:

- `ASIOError controlPanel() override { return ASE_NotPresent; }`
- `HMODULE g_module` already exists and is assigned in `DllMain(DLL_PROCESS_ATTACH)`
- `init(void* sysHandle)` currently discards `sysHandle`; if an owner window is desired, inspect ASIO/host semantics before retaining it rather than assuming blindly
- `setSampleRate()` already rejects changes while buffers/worker/RUN are active
- `createBuffers()` rejects invalid rate-specific sizes and already-created/prepared buffers
- `createBuffers()` stores the host-provided `bufferSize` in `buffer_frames_`
- `createBuffers()` stores `callbacks_ = *callbacks`
- `disposeBuffers()` clears `callbacks_`
- `getLatencies()` reports the active `buffer_frames_` when buffers exist, otherwise the rate-specific preferred value
- `getBufferSize()` currently reports fixed rate-specific preferred values

Important ASIO semantic constraint for panel design:

The host supplies the actual `bufferSize` argument to `createBuffers()`. The panel must not silently override an explicit host-provided buffer size. A panel-selected latency/buffer value therefore needs a deliberate next-open/preferred/reset/reopen contract rather than hidden substitution inside `createBuffers()`.

Any `kAsioResetRequest` use must be designed only after checking callback lifetime and reentrancy. Do not fire host callbacks casually from a modal dialog or while teardown is in progress.

---

# Native ASIO control panel — immediate product milestone

The user is moving control-panel work to a fresh chat now. This milestone no longer waits for another REAPER retest because the currently built/validated `ca37f0e` bundle already passed the product matrix and the user's ordinary REAPER playback test.

The pending `post-coalesce-stale-v1` branch delta remains a separate runtime-validation item and must not be silently promoted to validated state by control-panel work.

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

Do not mutate the validated runtime baseline accidentally.

Preferred isolation strategy:

- use `ca37f0e8427227733cd6082a50e20101312e3333` as the **validated runtime reference/base** for control-panel behavior;
- create a dedicated control-panel branch rather than mixing UI implementation into the pending stale-wake validation work;
- suggested branch name: `exp/windows-arm64-asio-b5-control-panel`;
- do not merge/drop/rewrite the newer stale-wake commits as part of UI work; reconcile them later as a separate, explicit merge after each side is validated.

Before creating that branch, verify the current refs in GitHub and show the intended base/diff. Do not perform unrelated cleanup.

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

After panel PASS, finish real output + real stereo input validation at 48/96 kHz, freeze the B5 first release, then resume deferred CTCDC/CTIntrfu static analysis.
