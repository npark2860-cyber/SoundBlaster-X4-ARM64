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

`exp/windows-arm64-asio-b5-capability-productization@c9ca17171edcc3eb1b6e2c7e2e36173cb3f66c0f`

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

Never weaken packet discontinuity, render position, callback-index, render/capture copy or joined-worker safety checks merely to make validation pass.

`runtime-failsafe-v1` may overwrite render cyclic contents with silence on worker failure but must never perform worker-side pin teardown.

Validated B4D remains frozen.

---

# B5 public contract — current, pending 192 kHz cadence re-measurement

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

192 kHz current contract:

- min 384
- max 4800
- preferred 384
- granularity 48

Other:

- 512 compatibility exception remains accepted
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

384 at 192 kHz was the directly measured allocation minimum on the Windows X4 `msft_wave` path. It is now known to be allocation-valid but not yet proven sustained-cadence stable.

Do not change the 192 kHz public contract until the dedicated RUN cadence probe returns.

---

# Build/runtime markers

Current markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

The manual productization workflow refuses packaging unless ARM64EC and Classic ARM64 DLLs contain both markers.

---

# Historical full matrix PASS

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

Later evidence below supersedes any conclusion that 192k/384 is fully stable for sustained runtime.

96k/240 duplex still showed roughly 26..27 capture phase misses without strict packet/index/copy failure.

---

# Real REAPER regression — sustained buzz/drone

REAPER ARM64EC showed B5 active at:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples
- about 10 ms input + 10 ms output

During actual playback, output became a very loud sustained drone/buzz. REAPER left no useful log.

480 frames at 48 kHz is valid. Do not assume that host buffer value itself caused the failure.

This motivated `runtime-failsafe-v1`.

---

# Runtime fail-safe v1

On fatal worker failure B5 now:

1. snapshots pre-failure render/capture stats and engine messages;
2. sets `worker_failed_`;
3. overwrites both WaveRT render notification slots with silence;
4. performs no KSSTATE/pin close/dispose inside the worker;
5. only after silence emits the one-shot record to `OutputDebugString` and `%TEMP%\B5_RUNTIME_FAILURE.txt`.

The file logger is failure-only and is not in the normal realtime callback path.

---

# Latest measured runtime blocker — exact 192 kHz packet skip

Fail-safe validation report generated `2026-09-04 13:50:09.41`.

PASS before failure:

- registration / registry verification
- KS property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 duplex x2
- 96k/240 duplex x2
- 192k/384 output cycle 1

The second 192k/384 output cycle failed after 332 callbacks:

- `worker=1`
- `rPkt=1`
- `rPos=0`
- `idx=0`
- `outCopy=0`
- `inCopy=0`
- `stop=-999`

The preserved runtime failure file from `2026-09-04 13:50:24.083` recorded:

- `render notifications=333`
- `callbacks=332`
- `render packetDiscontinuities=1`
- `render positionRegressions=0`
- `render writes=332`
- `render lastPacket=334`
- `emergencySilence=OK`

Given the established 1-based completed render PACKETCOUNT semantics, this identifies the discontinuous transition as:

`332 -> 334`

Packet 333 was not observed as a separate worker notification.

384 frames / 192 kHz = exactly 2.000 ms. Current evidence is most consistent with one coalesced auto-reset notification or equivalent scheduling delay across two render periods.

The logger did not cause the discontinuity; it runs after the strict mux failure and after emergency silence.

The stale runtime string `B5 RENDER RUN entered` was only a diagnostic gap: the packet discontinuity counter was the actual cause indicator.

`emergencySilence=OK` proves the new fail-safe successfully zeroed the cyclic render contents on a real worker fatal path.

---

# Exact discontinuity diagnostics — implemented

The signaled WaveRT path now records non-sequential packet failures as:

`previous=<n> expected=<n+1> current=<m> delta=<m-n>`

and render position regression as previous/current positions.

Commit:

`dc1b16adb7788f27443be9efeb3bdc56ad51536d`

No strict check was relaxed.

---

# 192 kHz strict RUN cadence probe — implemented

The existing ARM64EC product-validation executable now supports:

`--cadence-192`

Packaged runner:

`probe_b5_192k_cadence.cmd`

Candidates:

- 384 frames = 2.000 ms, 1 x 5 s
- 432 frames = 2.250 ms, 2 x 10 s
- 480 frames = 2.500 ms, 2 x 10 s
- 576 frames = 3.000 ms, 2 x 10 s

Every candidate uses the normal B5 driver with the existing fatal packet-discontinuity rule enabled. The probe continues after a safely joined/teardown candidate failure so later geometries can still be measured in one run. BUSY aborts.

Implementation commits:

- exact diagnostics: `dc1b16adb7788f27443be9efeb3bdc56ad51536d`
- cadence mode: `e6d2a54dfd8a9f3072be749f2c7633c0a1ccddac`
- runner: `a62df8395888298a4be96c7cf14ed782d905e188`
- current B5 HEAD including README documentation: `c9ca17171edcc3eb1b6e2c7e2e36173cb3f66c0f`

The normal matrix was reordered so `reaper-48-480-output` runs before the 192 kHz case; therefore the actual REAPER 48k/480 geometry will no longer be hidden by a later 192 kHz failure.

The manual workflow packages the cadence runner. Automatic push/PR builds remain disabled.

---

# ASIO control panel — still required, temporarily preempted

The B5 driver still has:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

The planned first-release control panel remains:

- our own native Win32 UI
- no Creative control-panel binary reuse
- compact credible latency/buffer UI
- sample-rate-aware settings
- frames + milliseconds display
- no WaveRT pin creation merely from opening the panel
- no live mutation of active buffers/RUN
- deterministic Apply/OK/Cancel and safe host reset/reopen behavior
- lightweight diagnostics / save-report support later

Do not resume control-panel implementation until the current 192 kHz render cadence blocker is measured and resolved.

---

# Immediate next action

1. run manual workflow `Build ASIO B5 Productization` on current B5 HEAD;
2. require ARM64EC + Classic ARM64 compile/link PASS and both runtime markers;
3. install/register the resulting bundle with REAPER and all other X4 clients closed;
4. run `probe_b5_192k_cadence.cmd` once;
5. return `B5_192K_CADENCE_REPORT.txt`;
6. do not intentionally reproduce audible buzz;
7. if 384 is unstable but a larger candidate is consistently stable, change 192 kHz min/preferred only to the first measured stable candidate and update validation/latency contract accordingly;
8. if larger candidates also skip packets, do not keep increasing buffers blindly; diagnose notification scheduling instead;
9. rerun the full product matrix after the measured fix;
10. once runtime stability returns, resume native ASIO control-panel implementation;
11. then final REAPER real output + real stereo input validation;
12. freeze B5 first release and resume deferred CTCDC/CTIntrfu work.
