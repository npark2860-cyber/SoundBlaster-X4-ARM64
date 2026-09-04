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
2. `DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
9. `NEXT_ACTION_ASIO.md`
10. older B5/B4D histories only as needed

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

# Latest product runtime — full matrix PASS

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

This closes the prior `BUFFER_WITH_NOTIFICATION Win32=87 requested=2880` blocker for the first-release architecture.

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

# 192 kHz geometry diagnosis — closed

Dedicated geometry report generated `2026-09-04 13:05:25.43` established:

- 48..336 frames per notification: FAIL Win32=87
- 384 frames / 2.0 ms: first PASS
- 432..960 tested candidates: PASS
- accepted candidates returned `ActualBufferSize == RequestedBufferSize`

The failure was not generic byte alignment. It was a sample-rate-dependent minimum WaveRT notification duration on this X4 Windows path.

The chosen first-release fix is a rate-specific 384-frame minimum/preferred at 192 kHz, preserving the already-proven 1:1 host-buffer-to-WaveRT-packet architecture.

---

# What is now proven

The B5 silent product harness now proves:

- side-by-side COM/ASIO registration
- immutable ownership gates
- Int24LSB public channel contract
- 48/96/192 output sample rates
- 48/96 input exposure
- 192 kHz rate-specific buffer contract
- 48/96 full-duplex lifecycle
- joined-worker shutdown
- strict packet/index/copy/position checks remain enabled
- min/max and 512 compatibility buffer cases

---

# Current missing product surface — ASIO control panel

The B5 driver still implements:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

This is the immediate next milestone and must be completed before final REAPER real-signal validation.

Previously agreed direction:

- build our own native control panel;
- do not load/reuse Creative's control-panel binary;
- use the official panel only as a visual/UX reference for a compact latency-setting dialog;
- make the UI look like a credible product control panel, not a debug utility.

First control-panel scope:

- `IASIO::controlPanel()` opens a native Win32 dialog;
- current sample rate is visible;
- buffer/latency setting is the main editable control;
- effective latency is shown in frames and milliseconds;
- 48/96 kHz obey 96..4800 / step48 / preferred240;
- 192 kHz obey 384..4800 / step48 / preferred384;
- 512 compatibility remains accepted;
- opening the panel never creates a WaveRT pin;
- active ASIO buffers/RUN are never mutated underneath the host;
- Apply/OK/Cancel are deterministic;
- a setting change is persisted for the next safe create/reopen path;
- if host reset/restart is required, use the ASIO host notification path instead of altering live hardware state.

Do not alter the just-passed streaming core merely to implement the UI.

---

# What is NOT yet proven

Do not infer these from the silent harness:

1. control-panel open/change/apply/reopen behavior;
2. audible real 24-bit program output through B5 in REAPER;
3. real non-zero stereo capture while output is active;
4. longer DAW-load stability;
5. whether the 96 kHz capture phase misses cause a real recording-quality problem.

`inputNonzeroSamples=0` in the validation report only means the validation had no external signal; it is not proof of capture content failure or success.

---

# B4D protection

Validated B4D core remains frozen. Do not alter or bypass it.

---

# Immediate next action

Implement the B5 native ASIO control panel on the existing productization branch while keeping the passed streaming core frozen.

Required sequence:

1. replace `controlPanel() -> ASE_NotPresent` with a native panel entry point;
2. implement sample-rate-aware buffer/latency selection and persistence;
3. never create/open WaveRT streaming pins merely from the panel;
4. refuse unsafe live mutation while buffers/RUN are active;
5. add a small helper/host validation for panel availability and safe setting persistence/reopen;
6. rebuild/package with the existing manual workflow;
7. validate the panel path;
8. only then do REAPER real output + real stereo input validation at 48 kHz and 96 kHz.

After control panel + real output + real input pass, freeze B5 first-release ASIO and resume deferred CTCDC/CTIntrfu native static analysis.
