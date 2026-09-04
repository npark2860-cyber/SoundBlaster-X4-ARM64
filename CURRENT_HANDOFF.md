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

`exp/windows-arm64-asio-b5-capability-productization@1e4b9527269a84115f4aa43a09fdf3c9a7c31dd3`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_48K_RENDER_COALESCE_RECOVERY_V4.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_FAILSAFE_RUNTIME_192K_RENDER_PACKET_DISCONTINUITY.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
9. `NEXT_ACTION_ASIO.md`
10. older B5/B4D histories only as needed

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

A specifically measured forward Render `PACKETCOUNT delta=2` is now handled as an explicit one-block notification-coalescing xrun. Duplicate/backward/larger render jumps remain fatal.

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

384 at 192 kHz remains the directly measured allocation minimum on the Windows X4 `msft_wave` path. Do not raise the public minimum merely to hide a notification coalescing event now proven at 48 kHz too.

---

# Current runtime/build markers

- `dual-event-mux-v4-coalesce-recovery`
- `runtime-failsafe-v1`

The manual productization workflow refuses packaging unless both ARM64EC and Classic ARM64 DLLs contain both markers.

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

Multiple captured fatal runs directly proved:

`emergencySilence=OK`

---

# Exact notification evidence — key diagnosis

## Earlier 192 kHz run

At 192 kHz / 384 frames:

- callbacks=332
- render notifications=333
- lastPacket=334
- packetDiscontinuities=1
- position/index/copy errors=0

This identified a likely transition:

`332 -> 334`

Initially this suggested a possible 2.0 ms cadence problem.

## Latest 48 kHz run — superseding evidence

Product report generated `2026-09-04 15:26:42.62`.

48 kHz / 240 output-only:

- cycle1 PASS callbacks=139
- cycle2 PASS callbacks=142
- cycle3 failed after callbacks=74

Exact runtime message:

`B5 RENDER PACKET DISCONTINUITY previous=74 expected=75 current=76 delta=2`

The corresponding failure file recorded:

- rate=48000
- frames=240
- render notifications=75
- callbacks=74
- lastPacket=76
- packetDiscontinuities=1
- positionRegressions=0
- indexErrors=0
- renderCopyErrors=0
- `emergencySilence=OK`

This proves the same forward `+2` event at a 5.0 ms period. Therefore the problem is not primarily a 192 kHz buffer minimum.

The current diagnosis is WaveRT auto-reset notification coalescing / user-mode service delay: the event is not a counting semaphore, while absolute `PACKETCOUNT` continues advancing.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_48K_RENDER_COALESCE_RECOVERY_V4.md`

---

# Mux-v4 recovery — implemented, pending build/runtime validation

Exactly one forward Render transition `delta == 2` is now classified as `notification_coalesces` rather than an unrecoverable packet discontinuity.

For `74 -> 76` the mux:

1. synthesizes the missing ASIO callback for master packet 75;
2. uses the missing callback's correct alternating host buffer index;
3. discards its output because target hardware packet 76 is already completed;
4. immediately runs the normal current packet 76 callback;
5. writes only to future WaveRT packet 77;
6. continues streaming.

This preserves ASIO callback/index/sample timeline while accepting one unavoidable dropped/xrun block.

Duplex synthetic catch-up:

- zero-fills the missing input callback block;
- does not consume capture staging;
- resumes normal staged capture on the current callback.

Diagnostics now expose:

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

# 192 kHz cadence probe — no longer immediate

`probe_b5_192k_cadence.cmd` remains packaged as a diagnostic helper.

Do not run it first and do not use it to justify raising the 192 kHz public minimum unless later evidence again isolates a sample-rate-specific issue.

---

# ASIO control panel — still required, temporarily preempted

The B5 driver still has:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

The planned first-release control panel remains binding:

- own native Win32 UI
- no Creative control-panel binary reuse
- compact credible latency/buffer UI
- current sample rate
- frames + milliseconds
- sample-rate-aware settings
- 512 compatibility
- no WaveRT pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel
- safe setting persistence / host reopen-reset path
- lightweight diagnostics/save-report surface later

Do not forget this milestone after the runtime blocker closes.

---

# Immediate next action

1. run manual workflow `Build ASIO B5 Productization` on current B5 HEAD;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require `dual-event-mux-v4-coalesce-recovery` + `runtime-failsafe-v1` marker checks PASS;
4. install/register the resulting bundle with all other X4 clients closed;
5. run `install_and_validate_b5.cmd` once;
6. return `B5_PRODUCT_VALIDATION_REPORT.txt`;
7. do not run the dedicated 192 kHz cadence probe first;
8. inspect `renderCoalesces` / `renderDroppedBlocks`; a non-zero value with zero strict fatal counters is an explicitly recovered xrun, not a perfect-delivery claim;
9. if product validation passes, do one normal REAPER 48k/480 audible playback test;
10. if a fatal failure occurs, stop testing and return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately;
11. once runtime stability is restored, resume native ASIO control-panel implementation;
12. after panel PASS, finish real output + real stereo input validation at 48/96 kHz;
13. freeze B5 first release and resume deferred CTCDC/CTIntrfu work.
