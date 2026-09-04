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

# Latest returned runtime

Product validation generated `2026-09-04 13:00:02.92` still showed:

PASS:

- registration
- property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2

and the known 192k/240 failure:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

A dedicated geometry probe report generated `2026-09-04 13:05:25.43` then measured the exact 192 kHz boundary.

192 kHz / stereo / 24-bit / NotificationCount=2:

- 48..336 frames per notification: FAIL Win32=87
- 384 frames / 2.00 ms: first PASS
- every tested 432..960 frame candidate: PASS
- accepted candidates returned `ActualBufferSize == RequestedBufferSize`

Therefore the first-release blocker is no longer unknown.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_192K_GEOMETRY_MEASURED_384_CONTRACT.md`

---

# Implemented first-release contract

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

512 remains accepted as the B4D-era compatibility exception.

192 kHz still exposes zero inputs.

Implementation changes are intentionally narrow:

- `getBufferSize()` is sample-rate aware
- `getLatencies()` uses the selected-rate preferred value before buffers are created
- `createBuffers()` rejects sub-384 sizes at 192 kHz before WaveRT pin preparation
- product validation checks the rate-specific public buffer contract
- 192 kHz validation now uses 384 frames

No WaveRT engine, mux-v3, BUSY gate, joined-worker safety, or validated B4D core was changed.

---

# Remaining 96 kHz observation

Mux-v3 fixed the previous false exact-phase failure and 96k/240 duplex passes lifecycle validation.

However capture still trails render and prior reports showed roughly 23..27 `capturePhaseMisses` per ~278 callbacks.

Do not reopen this during the 192 kHz validation unless the new full matrix exposes a concrete strict failure. Treat it as a separate capture cadence/latency quality follow-up.

---

# Immediate action

Run manual workflow:

`Build ASIO B5 Productization`

Required build outcome:

1. ARM64EC B5 DLL + helpers compile/link PASS;
2. Classic ARM64 B5 DLL compile/link PASS;
3. PE/ARM64X checks PASS;
4. both B5 DLLs contain `dual-event-mux-v3`;
5. productization ZIP produced.

After build PASS:

1. download the new ZIP;
2. close REAPER/media players/Creative App playback and other X4 users as before;
3. run `install_and_validate_b5.cmd` once;
4. return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.

Expected matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/384 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

The report must show the 192 kHz buffer contract as `min=384 max=4800 preferred=384 granularity=48` and both 192k/384 output cycles must stop cleanly.

Only after the full matrix passes should final REAPER validation cover audible 24-bit output plus real stereo input together.
