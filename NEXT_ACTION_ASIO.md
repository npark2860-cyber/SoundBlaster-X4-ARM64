# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety:

- never bypass local/global BUSY gates
- never intentionally reproduce the active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@1ba2faabb922be0f002d698019c7be6e602ff3bc`

Runtime/build marker:

`dual-event-mux-v3`

---

# Latest returned runtime — silent product matrix closed

Product validation generated `2026-09-04 13:21:47.16`.

Final result:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

PASS matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/384 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

192 kHz public contract verified:

`min=384 max=4800 preferred=384 granularity=48`

192 kHz WaveRT runtime geometry:

- 384 frames/notification
- 2304 bytes/packet
- 4608-byte cyclic buffer
- 2.0 ms period

Both 192k/384 cycles stopped cleanly with joined worker and no strict packet/index/copy failure.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_FULL_MATRIX_PASS_192K_384.md`

---

# Remaining 96 kHz observation

96k/240 full duplex lifecycle passes, but capture still trails render in the short silent harness.

Latest cycles:

- render 282 / capture 255 / capturePhaseMisses 27
- render 280 / capture 254 / capturePhaseMisses 26

No strict failure was recorded.

Do not change mux-v3 merely to make these counters cosmetically match. Real-signal validation remains required later.

---

# Current missing product surface — ASIO control panel

The B5 driver currently implements:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

This is now the immediate missing first-release feature.

The previously agreed direction is to build our own native ARM64/ARM64EC-friendly control panel. Do not load or reuse Creative's control-panel binary; only use the official panel's compact latency-setting role and general visual credibility as reference.

First control-panel scope:

- launched by the host through `IASIO::controlPanel()`;
- native Win32 UI with no external runtime dependency;
- product identity: `Sound Blaster X4 ARM64 ASIO B5`;
- current sample rate shown clearly;
- buffer/latency setting is the primary editable control;
- 48/96 kHz contract: 96..4800 frames, step 48, preferred 240;
- 192 kHz contract: 384..4800 frames, step 48, preferred 384;
- 512 compatibility value remains selectable/accepted;
- current effective latency shown in frames and milliseconds;
- Apply/OK/Cancel behavior must be deterministic;
- no WaveRT pin creation merely from opening the panel;
- never change buffer geometry while ASIO buffers/RUN are active;
- if a host requires restart/reset for a changed buffer, request it through the ASIO host notification path rather than mutating an active engine underneath it;
- UI should be compact and credible, not a diagnostic/debug window.

The control panel must not weaken any BUSY, packet, copy, position, or joined-worker safety rule.

---

# Immediate action — implement control panel

1. keep the just-passed B5 streaming core frozen;
2. add a native control-panel implementation on the existing B5 productization branch;
3. wire `IASIO::controlPanel()` to open it instead of returning `ASE_NotPresent`;
4. keep buffer settings sample-rate aware;
5. persist the selected buffer/latency setting for the next buffer creation, without touching an already-active stream;
6. add a small host/helper validation that confirms the panel entry point exists and the selected setting is reflected by the driver after a safe reopen;
7. rebuild/package through the existing manual `Build ASIO B5 Productization` workflow.

Do not move to REAPER real-signal validation before this control-panel milestone is implemented and its setting path is verified.

---

# After control-panel PASS

Then perform REAPER ARM64EC real-signal validation:

1. audible 24-bit output at 48 kHz;
2. real stereo X4 input while output remains active;
3. short record/playback;
4. stop/reopen;
5. repeat at 96 kHz / 240 and specifically watch the known capture cadence quality observation.

Only after control panel + real output + real input pass should B5 first-release ASIO be considered closed and deferred CTCDC/CTIntrfu work resume.
