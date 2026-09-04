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

`exp/windows-arm64-asio-b5-capability-productization@bb2a42e143cc0b48a60a131e44a06002e3594ec5`

Runtime/build marker:

`dual-event-mux-v3`

---

# Latest returned runtime

Report generated `2026-09-04 12:25:44.97` proved mux v2 was built and loaded.

PASS:

- registration
- property-only idle gate
- KS capability probe
- 48k/240 output x3
- 48k/240 full duplex x2

96k/240 full duplex failed after one callback with:

`next render notification arrived before prior capture synchronization`

At failure:

- render packet discontinuities=0
- capture packet discontinuities=0
- render position regressions=0
- callback-index errors=0
- render/capture copy errors=0

Therefore the failure was mux-v2's exact render/capture phase policy, not a hardware packet failure.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`

---

# Fix now implemented — mux v3

Full-duplex behavior:

- Render is the callback/master clock.
- Render callback and write-ahead never wait for exact Capture phase.
- Capture runs as an independent producer into two tagged staging slots.
- Capture is queried on its own event and opportunistically at render wakes.
- Oldest unconsumed Capture packet is copied into the current ASIO input buffer.
- A single missing Capture packet at render time is treated as phase offset and zero-fills that input buffer.
- More than four consecutive misses is fatal capture starvation.

Still fatal:

- Render packet discontinuity
- Capture packet discontinuity
- Render presentation-position regression
- callback index repetition
- copy failures
- capture staging overrun/sequence mismatch
- sustained capture starvation
- worker failure

BUSY and joined-worker safety are unchanged.

---

# Immediate action

Run manual workflow:

`Build ASIO B5 Productization`

Do not reuse the mux-v2 ZIP.

Required build outcome:

1. ARM64EC DLL + helpers compile/link PASS;
2. Classic ARM64 DLL compile/link PASS;
3. PE/ARM64X checks PASS;
4. both DLLs contain `dual-event-mux-v3`;
5. ZIP is produced.

After build PASS only:

1. download the new ZIP;
2. close other X4 playback/default endpoint ownership as before;
3. run `install_and_validate_b5.cmd` once;
4. return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.

A report counts as mux-v3 evidence only when it contains:

`adapter=dual-event-mux-v3`

The matrix must reach:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

After full matrix PASS, perform final REAPER validation with audible 24-bit output and real stereo input together.
