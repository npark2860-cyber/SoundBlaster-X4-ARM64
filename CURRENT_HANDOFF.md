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

`exp/windows-arm64-asio-b5-capability-productization@bb2a42e143cc0b48a60a131e44a06002e3594ec5`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`
7. `NEXT_ACTION_ASIO.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
9. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
10. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
11. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
12. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
13. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

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

# B5 first-release contract

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz reports zero inputs
- buffer 96..4800 / step 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

---

# Build status

The previous ARM64EC `cguid.h::__uuidof` compile failure was resolved enough for the productization package to build and run.

The returned runtime report generated `2026-09-04 12:25:44.97` contains:

`adapter=dual-event-mux-v2`

and shows MMCSS `Pro Audio` with priority OK. Therefore mux v2 was physically present in the loaded DLL and executed.

---

# Latest runtime evidence — mux v2

Registration: PASS.

Property-only Render Pin 1 idle gate: FREE.

KS capability probe: PASS.

## 48 kHz / 240 output-only

Three cycles PASS:

- callbacks 141 / 139 / 139
- stop=ASE_OK
- workerJoined=YES
- no packet/index/copy errors

## 48 kHz / 240 full duplex

Two cycles PASS:

- callbacks 138 / 138
- renderNotif 139 / 139
- captureNotif 138 / 138
- outFrames=inFrames=33120
- stop=ASE_OK

## 96 kHz / 240 full duplex

First cycle produced one callback, then mux v2 failed with:

`B5 worker DUPLEX failed: next render notification arrived before prior capture synchronization`

Final strict counters:

- callback-index errors = 0
- render copy errors = 0
- capture copy errors = 0
- render packet discontinuities = 0
- render position regressions = 0
- capture packet discontinuities = 0

Therefore this was not a WaveRT packet or copy failure. It was a false-positive synchronization failure caused by mux-v2 policy.

Mux v2 required exact `render N -> capture N-1` pairing and allowed only one pending render packet. At 96 kHz/240 the Render and Capture notification streams can have a stable phase offset while both hardware packet sequences remain continuous.

---

# Fix implemented — dual-event-mux-v3

Current B5:

`bb2a42e143cc0b48a60a131e44a06002e3594ec5`

Full-duplex policy is now:

- Render remains the ASIO callback/master clock.
- Render callback/write-ahead never waits for exact Capture phase.
- Capture is an independent producer.
- Capture packets are staged in two fixed slots and tagged by absolute packet number.
- Capture is serviced both from its notification event and opportunistically on every render wake.
- The oldest unconsumed staged Capture packet is copied to the current ASIO input buffer before callback.
- A render period with no ready Capture packet is a phase miss, not immediate failure; that input buffer is zero-filled.
- More than four consecutive Capture phase misses is treated as real starvation and remains fatal.

Strict failures remain:

- render packet discontinuity
- capture packet discontinuity
- render presentation-position regression
- callback buffer-index repetition
- render/capture copy failure
- capture staging overrun
- capture staging sequence mismatch
- sustained capture starvation
- worker failure

BUSY and joined-worker safety are unchanged.

Runtime/build marker is now:

`dual-event-mux-v3`

The main build workflow scans both ARM64EC and Classic ARM64 DLLs and refuses to package unless this marker is present.

---

# B4D protection status

Compared with validated B4D `a95a95d...`:

- ahead_by: 44
- behind_by: 0
- merge base: exactly `a95a95d014bcc1c3a521be41325841ae96dc8a61`
- validated B4D core remains untouched

---

# Immediate next action

Do not reuse the mux-v2 ZIP.

Run manual workflow:

`Build ASIO B5 Productization`

Required before hardware validation:

1. ARM64EC B5 compile/link PASS;
2. helper compile/link PASS;
3. Classic ARM64 B5 compile/link PASS;
4. PE/ARM64X checks PASS;
5. both DLLs contain `dual-event-mux-v3`;
6. ZIP produced.

After Actions PASS, run the new `install_and_validate_b5.cmd` once and return the new report.

A report counts as mux-v3 evidence only if it contains:

`adapter=dual-event-mux-v3`

The strict matrix still must reach:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

Only after full matrix PASS should final REAPER validation cover audible 24-bit output plus real stereo input together.
