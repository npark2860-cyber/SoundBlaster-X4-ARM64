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
2. `DEBUG_HISTORY_20260904_ASIO_B5_FAILSAFE_RUNTIME_192K_RENDER_PACKET_DISCONTINUITY.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
9. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
10. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
11. `NEXT_ACTION_ASIO.md`
12. older B5/B4D histories only as needed

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

`runtime-failsafe-v1` may overwrite render cyclic contents with silence on worker failure but must never perform worker-side pin teardown.

Validated B4D remains frozen and must not be changed without a concrete B5 regression requiring it.

---

# B5 first-release public contract

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

192 kHz min/preferred 384 is based on the directly measured Windows X4 `msft_wave` 2.0 ms minimum notification geometry.

---

# Build/runtime markers

Current markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

The manual productization workflow refuses to package unless both ARM64EC and Classic ARM64 B5 DLLs contain both markers.

---

# Prior full silent matrix — historical PASS

Report generated `2026-09-04 13:21:47.16`:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

It passed:

- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility x1

This proved the public 192k/384 contract and lifecycle once, but later evidence below supersedes any conclusion that the render runtime is fully stable.

96k/240 duplex still showed roughly 26..27 capture phase misses without a strict packet/index/copy failure.

---

# Real REAPER regression — sustained buzz/drone

REAPER ARM64EC later showed B5 active at:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples
- about 10 ms input + 10 ms output

During actual playback, output became a very loud sustained drone/buzz. REAPER left no useful diagnostic log.

480 frames at 48 kHz is valid. Do not assume the buffer value itself is invalid.

This motivated `runtime-failsafe-v1`.

---

# Runtime fail-safe v1

On a fatal worker path B5 now:

1. snapshots pre-failure render/capture stats and engine messages;
2. sets `worker_failed_`;
3. overwrites both WaveRT render notification slots with silence via the existing render copy API;
4. performs no KSSTATE/pin close/dispose inside the worker;
5. only after silence, writes a one-shot diagnostic record to `OutputDebugString` and `%TEMP%\B5_RUNTIME_FAILURE.txt`.

The file logger is failure-only and does not run in the normal realtime callback path.

---

# Latest runtime — NEW 192 kHz render packet discontinuity

Report generated `2026-09-04 13:50:09.41` with the fail-safe build.

PASS before failure:

- registration / registry verification
- KS property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output cycle 1

96k remained similar:

- cycle1 render 281 / capture 255 / phase misses 27
- cycle2 render 278 / capture 252 / phase misses 27

No strict 96k packet/index/copy failure was reported.

## Failing cycle

`preferred-192-output` cycle 2:

- rate = 192000
- frames = 384
- callbacks = 332
- `worker=1`
- render packet discontinuities `rPkt=1`
- render position regressions `rPos=0`
- callback index errors `idx=0`
- render copy errors `outCopy=0`
- capture copy errors `inCopy=0`
- `stop=-999`

Worker output:

`B5 worker RENDER failed: B5 RENDER RUN entered emergencySilence=OK log=%TEMP%\B5_RUNTIME_FAILURE.txt`

Final result:

`B5 PRODUCT VALIDATION RESULT: FAIL code=28`

`B5 INSTALL + PRODUCT VALIDATION: FAIL`

The new 48k/480 five-second REAPER-matched validator case was not reached because validation stops at the first failing case.

## Interpretation

The failure logger did not cause this. It only runs after the strict mux failure has already been detected.

The render signaled path reads WaveRT `PACKETCOUNT` and increments `packet_discontinuities` whenever the observed packet is not exactly `previous_packet + 1`. Mux-v3 treats any increment as fatal.

Therefore the latest report proves a non-sequential render PACKETCOUNT observation at 192k/384.

At 384 frames / 192 kHz the notification period is 2.0 ms. Event coalescing or scheduling delay is a plausible explanation, but is not yet proven because the current record does not preserve the exact `previous -> current` packet transition.

Do not weaken the strict continuity check yet.

## Fail-safe validation

`emergencySilence=OK`

This is direct proof that runtime-failsafe-v1 executed on a real worker fatal path and successfully zeroed the render cyclic slots before logging.

---

# ASIO control panel — still required, temporarily preempted

The B5 driver still has:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

The planned control panel remains required:

- native Win32 UI
- no Creative binary reuse
- compact credible latency/buffer panel
- sample-rate-aware settings
- frames + ms display
- no pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel and safe reopen/reset behavior
- diagnostic section later for lightweight counters and save-report support

But the newly proven render runtime failure takes priority.

---

# Immediate next action

Do **not** rerun the validation yet.

Retrieve and return the already-created file from the same Windows session:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

Preserve it before another failure overwrites it.

After reading that file:

1. determine whether it contains enough evidence to identify the packet transition;
2. if not, add exact render discontinuity diagnostics: previous packet, current packet, delta, presentation position and QPC;
3. keep strict packet/copy/index/position protections enabled;
4. implement only the measured render-notification/coalescing fix;
5. rerun the full matrix including the 48k/480 five-second case;
6. only after runtime stability returns, resume the native ASIO control-panel milestone;
7. then perform final real output + real stereo input validation and freeze B5 first release;
8. CTCDC/CTIntrfu remains deferred until then.
