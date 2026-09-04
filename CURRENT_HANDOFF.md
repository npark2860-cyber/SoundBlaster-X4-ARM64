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

`exp/windows-arm64-asio-b5-capability-productization@075010999bed5c433f03d7421f2fa3a18221bd98`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
9. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
10. `NEXT_ACTION_ASIO.md`
11. older B5/B4D histories only as needed

CTCDC remains deferred until the B5 first-release ASIO product surface and host-level pass are closed.

---

# Proven fallback

B4D remains hardware/user proven in REAPER ARM64EC:

- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- Render Pin 1
- local + global BUSY gates
- joined worker stop
- ASIO 2.x time-info

Do not modify validated B4D unless a concrete B5 regression requires it.

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

New runtime fail-safe v1 must also obey the joined-worker rule. It may overwrite render cyclic contents with silence on worker failure, but it must never perform worker-side pin teardown.

---

# B5 first-release contract — current

Channels/sample type:

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- 192 kHz reports zero inputs
- output 48/96/192 kHz

Buffer contract:

48/96 kHz:

- min 96
- max 4800
- preferred 240
- granularity 48

192 kHz:

- min 384
- max 4800
- preferred 384
- granularity 48

Other:

- 512 compatibility exception remains accepted
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

The 192 kHz contract intentionally differs from Creative's public 240-frame preferred value because the Windows X4 `msft_wave` path was directly measured to reject notification periods below 2.0 ms at 192 kHz.

---

# Build/runtime markers

Current markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

The main workflow refuses to package unless both ARM64EC and Classic ARM64 B5 DLLs contain both markers.

---

# Latest completed silent runtime — full matrix PASS

Returned report generated `2026-09-04 13:21:47.16`.

Final result:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

Registration/capability:

- B5 registration PASS
- registry verification PASS
- property-only Render Pin 1 idle gate `C 0/1 G 0/1 busy=NO`
- KS capability probe PASS
- ASIO capability probe PASS

Lifecycle matrix:

## 48 kHz / 240 output-only x3

PASS all cycles.

Callbacks:

- 140
- 141
- 139

All stop with `stop=0`, `workerJoined=YES`.

## 48 kHz / 240 full duplex x2

PASS both cycles.

- cycle1: callbacks 141, renderNotif 141, captureNotif 140, capturePhaseMisses 1
- cycle2: callbacks 141, renderNotif 141, captureNotif 142, capturePhaseMisses 0

No strict packet/index/copy failure.

## 96 kHz / 240 full duplex x2

PASS both cycles.

- cycle1: callbacks 282, renderNotif 282, captureNotif 255, capturePhaseMisses 27, captureConsumed 255
- cycle2: callbacks 280, renderNotif 280, captureNotif 254, capturePhaseMisses 26, captureConsumed 254

No strict packet/index/copy failure.

The 96 kHz capture cadence still trails render by roughly 26..27 callback periods over the short validation window. Treat this as a remaining latency/cadence quality observation, not as a product lifecycle blocker. Do not weaken strict checks to hide it.

## 192 kHz / 384 output-only x2

PASS both cycles.

Public contract verified each cycle:

`min=384 max=4800 preferred=384 granularity=48`

WaveRT geometry:

- 384 frames/notification
- 2304 bytes/packet
- 4608-byte cyclic buffer
- 2.0 ms period

Cycle1:

- callbacks 350
- renderNotif 350
- latencyOut 384
- stop=0
- outFrames 134400

Cycle2:

- callbacks 348
- renderNotif 348
- latencyOut 384
- stop=0
- outFrames 133632

This closed the prior `BUFFER_WITH_NOTIFICATION Win32=87 requested=2880` blocker for the first-release architecture.

## Boundary / compatibility

48 kHz / 96 output:

- PASS
- callbacks 250

48 kHz / 4800 output:

- PASS
- callbacks 9

48 kHz / 512 compatibility output:

- PASS
- callbacks 65

---

# New real REAPER regression — sustained buzz/drone

After the full silent matrix passed, actual REAPER playback exposed a new host-level failure.

Observed REAPER device state:

- 48 kHz
- 24-bit
- 2 inputs / 2 outputs
- 480 samples
- approximately 10 ms input + 10 ms output

480 frames / 48 kHz = exactly 10 ms and is a valid B5 buffer size.

During audible playback, output later became a very loud sustained `drone/buzz` tone. REAPER itself left no useful log.

Do not infer that 480 frames is invalid.

The previous mux-v3 fatal path set `worker_failed_` and returned from the worker but did not overwrite the WaveRT render cyclic contents. A plausible mechanism is therefore:

`worker fatal exit -> render pin remains RUN -> last cyclic audio contents repeat`

This matches the host symptom, but it is still a hypothesis. The exact fatal reason is not known until a new runtime record captures it.

---

# Runtime fail-safe v1 — implemented

B5 source now adds a failure-only safety/diagnostic path in:

`src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

On fatal worker failure:

1. snapshot pre-failure render/capture stats and engine messages;
2. snapshot callback/index/copy counters and capture phase counters;
3. set `worker_failed_`;
4. overwrite both WaveRT render notification slots with silence through the existing `write_render_packet24()` API;
5. do not perform KSSTATE transition, pin close, dispose, or hardware teardown inside the worker;
6. then emit the diagnostic record to `OutputDebugString` and:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

The failure record includes:

- direction/reason
- captured Win32 value
- emergency-silence result
- rate / frames
- selected/running state
- callback and last-index state
- callback-index/render-copy/capture-copy error counters
- render/capture notification, packet, position, frame and nonzero counters
- captureNotReady / captureMoreData / capturePhaseMisses / captureConsumed
- render/capture engine messages

The diagnostic stats are captured before the two emergency zero writes so the safety action does not contaminate the record.

File/debug logging occurs only after the emergency silence attempt.

---

# REAPER-matched silent validator added

The current B5 validator additionally includes:

`reaper-48-480-output`

- 48 kHz
- 480 frames
- output-only
- 5 seconds

This matches the actual REAPER host geometry observed before the buzz failure.

It cannot prove audible-content stability, but it must pass before the next REAPER playback test.

---

# 192 kHz geometry diagnosis — closed

Dedicated geometry report generated `2026-09-04 13:05:25.43` established:

- 48..336 frames per notification: FAIL Win32=87
- 384 frames / 2.0 ms: first PASS
- 432..960 tested candidates: PASS
- accepted candidates returned `ActualBufferSize == RequestedBufferSize`

The failure was not generic byte alignment. It was a sample-rate-dependent minimum WaveRT notification duration on this X4 Windows path.

The chosen first-release fix is a rate-specific 384-frame minimum/preferred at 192 kHz, preserving the already-proven 1:1 host-buffer-to-WaveRT-packet architecture.

---

# ASIO control panel — still required, temporarily preempted

The B5 driver still implements:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

The planned first-release control panel remains:

- our own native Win32 UI;
- no reuse of Creative's control-panel binary;
- compact credible latency/buffer UI inspired only by the role/look of the official panel;
- sample-rate-aware buffer settings;
- frames + milliseconds display;
- no WaveRT pin creation merely from opening the panel;
- no live mutation of active ASIO buffers/RUN;
- deterministic Apply/OK/Cancel and safe host reset/reopen behavior.

However the concrete real-playback sustained-buzz regression now takes priority over UI implementation.

Do not resume control-panel work until the runtime-failsafe build passes its new validation and one normal REAPER playback check has been completed, or until a recurring failure log has been analyzed and fixed.

---

# B4D protection

Validated B4D core remains frozen. Do not alter or bypass it.

---

# Immediate next action

1. run manual workflow `Build ASIO B5 Productization`;
2. require ARM64EC + Classic ARM64 compile/link PASS;
3. require PE/ARM64X checks PASS;
4. require both `dual-event-mux-v3` and `runtime-failsafe-v1` in both DLLs;
5. install the new ZIP and run `install_and_validate_b5.cmd` once;
6. confirm the new 48k/480 5-second `reaper-48-480-output` case passes;
7. then do one normal REAPER 48k/480 audible playback test;
8. do not intentionally provoke or repeatedly reproduce the loud buzz;
9. if it occurs again, stop testing and return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately.

Once this runtime safety regression is closed, resume the native ASIO control-panel milestone, then final real output + real stereo input validation, then freeze B5 first release and resume deferred CTCDC/CTIntrfu native static analysis.
