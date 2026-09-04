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

Do not change mux-v3 merely to make these counters cosmetically match. First determine whether the difference causes a real input-quality problem in REAPER with an actual signal.

---

# Immediate action — REAPER real-signal validation

Do not rebuild or modify B5 first unless a concrete host-level failure appears.

Use the current B5 product ZIP and REAPER ARM64EC.

## Pass A — 48 kHz / 240

1. select `Sound Blaster X4 ARM64 ASIO B5`;
2. set 48 kHz / 240 frames;
3. play an actual project or audio file and confirm audible 24-bit output;
4. connect/select a real stereo X4 input source;
5. arm a stereo track and verify both L/R meters receive real signal while output continues;
6. record a short clip and play it back;
7. stop audio, close/reopen the ASIO device or REAPER, and repeat once.

Required result:

- no crash/green screen/BSOD
- no BUSY override
- no stuck device after stop/reopen
- audible output
- real non-zero stereo capture

## Pass B — 96 kHz / 240

Repeat the same test at 96 kHz / 240.

Specifically watch for:

- dropouts
- delayed or missing input blocks
- obvious drift against output
- one channel stopping
- input meter freezing while playback continues

If 48k passes but 96k input shows a concrete problem, capture evidence and fix only that measured issue. Do not weaken the existing strict packet/index/copy/position checks.

---

# Release closure rule

B5 first release can be considered closed only after:

- audible real 24-bit output PASS;
- real stereo input PASS at the same time;
- stop/reopen PASS;
- at least 48 kHz host-level PASS;
- preferably 96 kHz host-level PASS or a clearly documented limitation.

After that, freeze the first-release ASIO state and resume deferred CTCDC/CTIntrfu native static analysis.
