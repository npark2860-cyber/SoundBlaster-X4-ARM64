# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 productization source:

`exp/windows-arm64-asio-b5-capability-productization@ca37f0e8427227733cd6082a50e20101312e3333`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_48K_RENDER_COALESCE_RECOVERY_V4.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_FAILSAFE_RUNTIME_192K_RENDER_PACKET_DISCONTINUITY.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
9. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
10. `NEXT_ACTION_ASIO.md`
11. older B5/B4D histories only as needed

CTCDC remains deferred until the B5 first-release ASIO product surface and host-level pass are closed.

---

# Immutable safety

Never bypass BUSY.

B5 retains:

1. Render Pin 1 local/global preflight at ASIO `init()`;
2. Render Pin 1 local/global re-check before render `KsCreatePin`;
3. Capture Pin 4 local/global re-check before capture `KsCreatePin`;
4. mandatory joined worker before hardware teardown.

Historical collision class must never be intentionally reproduced:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path

Never weaken render position, callback-index, render/capture copy, capture packet integrity or joined-worker safety merely to make validation pass.

A specifically measured forward Render `PACKETCOUNT delta=2` is handled as an explicit one-block notification-coalescing xrun. Duplicate/backward/larger render jumps remain fatal.

`runtime-failsafe-v1` may overwrite render cyclic contents with silence on fatal worker failure but must never perform worker-side pin teardown.

Validated B4D remains frozen.

---

# B5 first-release public contract — current

Channels/sample type:

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- 192 kHz reports zero inputs
- output 48/96/192 kHz

48/96 kHz buffer contract:

- min 96
- max 4800
- preferred 240
- granularity 48

192 kHz buffer contract:

- min 384
- max 4800
- preferred 384
- granularity 48

Other:

- 512 compatibility exception remains accepted
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- NotificationCount=2

384 at 192 kHz remains the directly measured allocation minimum on the Windows X4 `msft_wave` path. Do not raise the public minimum merely to hide notification coalescing now proven at lower rates too.

---

# Current runtime/build markers

The fixed mux-v4 bundle must contain all three markers:

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`
- `packet-stats-observed-v1`

The manual productization workflow refuses packaging unless both ARM64EC and Classic ARM64 DLLs contain all three markers.

Automatic push/PR execution remains disabled; `workflow_dispatch` is retained.

---

# Historical silent matrix PASS

Report generated `2026-09-04 13:21:47.16`:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

It passed:

- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output x2
- 48k/96 output
- 48k/4800 output
- 48k/512 compatibility

96k/240 duplex still showed roughly 26..27 capture phase misses without strict packet/index/copy failure. Do not cosmetically hide those counters; real-signal validation remains required later.

---

# Real REAPER regression — sustained buzz/drone

REAPER ARM64EC showed B5 active at:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples
- approximately 10 ms input + 10 ms output

During actual playback, output became a very loud sustained drone/buzz while REAPER itself remained alive and left no useful log.

480 frames at 48 kHz is valid. Do not assume that host buffer value caused the failure.

The previous fatal worker path could leave WaveRT RUN with stale cyclic contents repeating. This motivated `runtime-failsafe-v1`.

---

# Runtime fail-safe v1

On fatal worker failure B5:

1. snapshots pre-failure render/capture stats and engine messages;
2. sets `worker_failed_`;
3. overwrites both WaveRT render notification slots with silence;
4. performs no KSSTATE/pin close/dispose inside the worker;
5. only after silence emits one-shot diagnostics to `OutputDebugString` and `%TEMP%\B5_RUNTIME_FAILURE.txt`.

The file logger is failure-only and is not in the normal realtime callback path.

Captured fatal runs directly proved:

`emergencySilence=OK`

---

# Render notification coalescing evidence

## 192 kHz evidence

At 192 kHz / 384 frames an earlier strict run observed a likely `332 -> 334` packet transition with no position/index/copy error.

A later mux-v3 cadence report showed:

- 384 frames: one 5 s cycle PASS
- 432 frames: later `+2` packet skip
- 480 frames: later `+4` packet skip
- 576 frames: one cycle PASS, another later `+2` skip

Therefore simply increasing the 192 kHz buffer does not eliminate the event behavior.

## 48 kHz superseding evidence

At 48 kHz / 240 frames = 5.0 ms, another strict run recorded:

`previous=74 expected=75 current=76 delta=2`

This proves the `+2` pattern is not primarily a 192 kHz minimum-period problem.

The current diagnosis is WaveRT auto-reset notification coalescing / user-mode service delay: event state is not a counting semaphore while absolute `PACKETCOUNT` continues advancing.

---

# Mux-v4 recovery policy

Exactly one forward Render transition `delta == 2` is classified as a recoverable one-block xrun / notification coalesce.

For `74 -> 76` the mux:

1. synthesizes the missing ASIO callback for master packet 75;
2. uses the missing callback's correct alternating host buffer index;
3. discards its output because target hardware packet 76 is already completed;
4. immediately runs the normal current packet 76 callback;
5. writes only to future WaveRT packet 77;
6. continues streaming.

Duplex synthetic catch-up:

- zero-fills the missing input callback block;
- does not consume capture staging;
- resumes normal staged capture on the current callback.

Diagnostics expose:

- engine `notification_coalesces`
- worker `renderCoalesces`
- worker `renderDroppedBlocks`
- coalesce history inside later fatal records

Still fatal:

- render duplicate/backward packet
- render forward `delta > 2`
- capture packet discontinuity
- render position regression
- callback-index repetition outside explicit catch-up
- render/capture copy error
- capture staging mismatch/overrun
- sustained capture starvation
- worker failure

No ASIO host reset/resync request is emitted in this first implementation.

---

# First mux-v4 runtime — software bookkeeping regression, now fixed

The first built mux-v4 bundle was tested around 15:48-15:49 KST.

Product validation loaded the correct v4/fail-safe marker and passed registration, property-only idle gating, and KS capability probing, but the first 48k/240 output case failed after exactly one callback:

- callbacks=1
- worker=1
- rPkt=0
- rPos=0
- idx=0
- outCopy=0
- inCopy=0

The v4 cadence run then failed 384/432/480/576 after one callback each with the same zero-fatal-counter pattern.

The preserved runtime record from 192k/576 proved:

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

This combination proves the WaveRT engine accepted the second notification. The mux-v4 post-check itself falsely rejected it.

## Exact root cause

Mux-v4 used `stats().last_packet` as the previous observed hardware packet.

But `write_render_packet24()` also updated internal `stats_.last_packet` to the future write-ahead packet.

Normal sequence therefore became:

1. hardware packet 1 observed;
2. callback writes future packet 2;
3. internal stats last_packet becomes 2;
4. next hardware packet 2 is correctly observed;
5. mux-v4 computes delta `2 - 2 = 0` and falsely exits.

The engine's private `previous_packet_` remained correct, which is why `packetDiscontinuities=0`.

## Correction

`X4WaveRtEngineB5::stats()` now returns a snapshot whose externally visible `last_packet` is normalized to the last hardware packet actually observed from PACKETCOUNT/GETREADPACKET whenever one exists.

Commits:

- `4acfadfc4131172d65e1877480b242c85c1416ce` — observed packet stats semantics fix
- `ca37f0e8427227733cd6082a50e20101312e3333` — embed `packet-stats-observed-v1`

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`

The broken v4 cadence report is not valid evidence about hardware cadence because it was terminated by this deterministic software regression.

---

# 192 kHz geometry remains unchanged

Repeated allocation-only geometry probe result:

- 48..336 frames: rejected with Win32 87
- 384 frames: first accepted
- every tested 432..960 frame candidate: accepted
- accepted requests returned exact requested buffer size

Therefore no geometry/public-contract change is justified by the broken first mux-v4 run.

---

# ASIO control panel — still required, temporarily preempted

The B5 driver still has:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

The planned first-release control panel remains binding:

- own native Win32 UI
- no Creative control-panel binary reuse
- compact credible latency/buffer UI
- current/effective sample rate and buffer
- frames + milliseconds
- sample-rate-aware settings
- 512 compatibility
- no WaveRT pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel
- safe setting persistence / host reopen-reset path
- lightweight diagnostics/save-report surface

Do not forget this milestone after the runtime blocker closes.

---

# Immediate next action

1. run manual workflow `Build ASIO B5 Productization` on current B5 HEAD `ca37f0e8427227733cd6082a50e20101312e3333`;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require all three marker checks PASS in both DLLs;
4. install/register the resulting bundle with all other X4 clients closed;
5. run `install_and_validate_b5.cmd` once;
6. return `B5_PRODUCT_VALIDATION_REPORT.txt`;
7. do not rerun the already-broken old v4 cadence bundle;
8. inspect `renderCoalesces` / `renderDroppedBlocks`; a non-zero value with zero strict fatal counters is an explicitly recovered xrun, not a perfect-delivery claim;
9. if product validation passes, do one normal REAPER 48k/480 audible playback test;
10. if a fatal failure occurs, stop testing and return the new `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately;
11. once runtime stability is restored, resume native ASIO control-panel implementation;
12. after panel PASS, finish real output + real stereo input validation at 48/96 kHz;
13. freeze B5 first release and resume deferred CTCDC/CTIntrfu work.
