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

`exp/windows-arm64-asio-b5-capability-productization@1ba2faabb922be0f002d698019c7be6e602ff3bc`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
8. `NEXT_ACTION_ASIO.md`
9. older B5/B4D histories only as needed

CTCDC remains deferred until the B5 first-release ASIO pass is closed.

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

# Build/runtime marker

Current runtime/build marker:

`dual-event-mux-v3`

The main workflow refuses to package unless both ARM64EC and Classic ARM64 DLLs contain this marker.

---

# Latest product runtime before 192 kHz contract fix

Returned product report generated `2026-09-04 13:00:02.92`.

PASS:

- B5 registration
- property-only Render Pin 1 idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2

The 192k/240 case still failed before worker creation:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

Mux-v3 remained loaded and active for the preceding cases.

## 96 kHz observation

96k/240 full duplex now lifecycle-passes without strict packet/index/copy errors.

Capture still trails render. Recent runs showed roughly 23..27 `capturePhaseMisses` per ~278 callbacks. This remains a separate cadence/latency-quality observation and is not the current 192 kHz blocker.

Do not weaken strict failure checks merely to hide it.

---

# 192 kHz WaveRT geometry — measured and closed diagnostically

Dedicated geometry report generated `2026-09-04 13:05:25.43`.

Probe conditions:

- X4 `msft_wave`
- Render Pin 1
- 192 kHz
- stereo
- 24-bit PCM
- `NotificationCount=2`
- property/buffer allocation only
- never entered KSSTATE_RUN
- local/global FREE required before every `KsCreatePin`

Measured results:

- 48 frames / 0.25 ms -> FAIL Win32=87
- 96 / 0.50 ms -> FAIL
- 144 / 0.75 ms -> FAIL
- 192 / 1.00 ms -> FAIL
- 240 / 1.25 ms -> FAIL
- 288 / 1.50 ms -> FAIL
- 336 / 1.75 ms -> FAIL
- **384 / 2.00 ms -> PASS**
- all tested 432..960 frame candidates -> PASS

First accepted request:

- 384 frames per notification
- 2304 bytes per notification
- 4608-byte cyclic buffer
- `ActualBufferSize=4608`

Every accepted test returned `ActualBufferSize == RequestedBufferSize`.

Conclusion:

The failure is not generic byte alignment. The measured boundary is a sample-rate-dependent minimum WaveRT notification duration on this X4 Windows path: 2.0 ms in the tested configuration.

---

# Implemented fix — sample-rate-dependent ASIO buffer contract

Current B5 branch now preserves the already-validated 1:1 ASIO-buffer-to-WaveRT-packet architecture instead of adding a new ring-buffer/timer scheduler for the first release.

Changes:

`driver_b5.cpp`

- 192 kHz min/preferred = 384
- `getBufferSize()` reports the selected-rate contract
- `getLatencies()` reports 384 before buffers at 192 kHz
- `createBuffers()` rejects sub-384 frames at 192 kHz before any WaveRT pin preparation
- 48/96 behavior unchanged

`product_validation_b5_arm64ec.cpp`

- validates `getBufferSize()` after `setSampleRate()`
- 192 kHz output test now runs 384 frames x2

`README_B5_PRODUCTIZATION.md`

- documents the measured 2.0 ms boundary and current first-release contract

No WaveRT engine file, mux-v3 file, BUSY gate, joined-worker rule, or validated B4D source was modified for this fix.

---

# B4D protection

Validated B4D core remains frozen. Do not alter or bypass it.

---

# Immediate next action

Run manual workflow:

`Build ASIO B5 Productization`

Required build result:

1. ARM64EC B5 DLL + helpers PASS;
2. Classic ARM64 B5 DLL PASS;
3. PE/ARM64X checks PASS;
4. both DLLs contain `dual-event-mux-v3`;
5. ZIP produced.

Then use the new ZIP and run:

`install_and_validate_b5.cmd`

once.

Return the new:

`B5_PRODUCT_VALIDATION_REPORT.txt`

Expected matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/384 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

The report must show the 192 kHz public buffer contract as:

`min=384 max=4800 preferred=384 granularity=48`

and both 192k/384 cycles must stop cleanly.

Only after the full matrix passes should final REAPER validation cover audible 24-bit output plus real stereo input together.
