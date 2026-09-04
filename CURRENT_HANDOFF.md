# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch:

`main`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 productization source:

`exp/windows-arm64-asio-b5-capability-productization@c69cfa98a497c0619ccdbe0fb7f40f0dd13ea687`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`
7. `NEXT_ACTION_ASIO.md`
8. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
9. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
10. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
11. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
12. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

CTCDC remains deferred until the B5 first-release ASIO pass is closed.

---

# Proven fallback

Independent B4D real playback in REAPER ARM64EC remains hardware/user proven:

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

# B5 measured contract

Creative reference measurement remains:

- 2 inputs / 10 outputs
- all channels Int24LSB type 17
- buffer 96..4800 / preferred 240 / granularity 48
- 48/96/192 kHz
- Internal Clock
- preferred latency 240 in / 240 out
- time-info supported
- lifecycle x3 PASS

X4 KS evidence:

- Render Pin 1: 2/6/8ch, 16/24-bit, 48/96/192 kHz
- Capture Pin 4: stereo, 16/24-bit, 48/96 kHz

B5 side-by-side identity:

`Sound Blaster X4 ARM64 ASIO B5`

Narrow first-release contract remains:

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz exposes zero input channels
- buffers 96..4800 / step 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

---

# Immutable safety

Never bypass BUSY.

B5 retains:

1. Render Pin 1 local/global preflight at ASIO `init()`;
2. Render Pin 1 local/global re-check immediately before render `KsCreatePin`;
3. Capture Pin 4 local/global re-check immediately before capture `KsCreatePin`;
4. mandatory joined worker before hardware teardown.

Historical collision class must never be intentionally reproduced:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path

---

# Latest returned runtime — 2026-09-04 11:50:11

`B5_PRODUCT_VALIDATION_REPORT(2).txt`

Registration, property-only idle gate, KS capabilities and public ASIO contract all passed again.

## 48 kHz / 240 output-only

Three cycles PASS:

- callbacks 139 / 139 / 140
- stop=ASE_OK
- workerJoined=YES
- no packet/index/copy errors

## 48 kHz / 240 full duplex

Two cycles PASS:

- callbacks=141 each
- renderNotif=142 each
- captureNotif=141 each
- outFrames=33840 each
- inFrames=33840 each
- stop=ASE_OK

`inputNonzeroSamples=0` still means the silent matrix proves capture lifecycle/copy survival only, not real external input signal content.

## 96 kHz / 240 full duplex

First cycle failed after 97 callbacks:

- worker=1
- idx=8
- outCopy=0
- inCopy=0
- rPkt=9
- rPos=0
- cPkt=1

The trace directly shows repeated render packet jumps such as 23->25, 35->37 and 43->45, followed later by capture discontinuity and `GETREADPACKET` Win32 21.

This proves the prior one-thread serial wait pattern is structurally invalid at the 2.5 ms 96 kHz/240 period. While the worker waited on capture, additional auto-reset render notifications could coalesce before the worker waited on render again.

The previous MMCSS-only change did not remove that event-coalescing window.

Microsoft WaveRT semantics also permit `GetReadPacket` to report device-not-ready when no new capture packet is available; Win32 21 is therefore treated as transient in the new worker, while capture packet-number discontinuity remains fatal.

---

# Dual-event mux fix implemented

Current B5:

`c69cfa98a497c0619ccdbe0fb7f40f0dd13ea687`

Added shared B5 runtime adapter:

`src/asio-arm64-stage-b0/driver_b5_mux_adapter.inl`

Both ARM64EC and Classic ARM64 B5 builds route B5 worker creation through `dual-event-mux-v1`.

Full-duplex worker now waits simultaneously on:

1. stop event
2. capture notification event
3. render notification event

Capture intentionally has lower wait index than render when both are signaled.

The worker:

- services render/capture events independently
- tags both capture WaveRT slots with absolute packet numbers
- pairs render packet N with exact capture packet N-1
- invokes ASIO callback only when that pair is available
- preserves render write-ahead `writePacket = renderPacket + 1`
- treats another render event before prior pair synchronization as a real duplex failure
- treats capture `ERROR_NOT_READY` as transient/no-data
- drains `MoreData=TRUE`

Strict failures remain strict:

- render packet discontinuity
- capture packet discontinuity
- presentation-position regression
- repeated callback buffer index
- render/capture copy error
- duplex synchronization failure

Realtime worker itself now enters MMCSS `Pro Audio` + `AVRT_PRIORITY_CRITICAL`, with `THREAD_PRIORITY_HIGHEST` fallback only if MMCSS registration fails.

The packet hot path no longer uses the original per-notification printf path.

---

# Runtime/build marker

The new runtime marker is:

`dual-event-mux-v1`

Expected runtime lines include:

`B5 worker realtime adapter=dual-event-mux-v1 ...`

and

`B5 worker START adapter=dual-event-mux-v1 ...`

The main `Build ASIO B5 Productization` workflow now scans both built DLLs and refuses to package them unless the marker is physically present in:

- ARM64EC B5 DLL
- Classic ARM64 B5 DLL

Therefore a future validation report without the runtime marker must be treated as an old/wrong DLL, not as a test of this fix.

---

# B4D protection status

Compare against validated B4D `a95a95d...`:

- status: ahead
- ahead_by: 34
- behind_by: 0
- merge base: exactly `a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated B4D core remains untouched.

---

# Immediate next action

Do not reuse the previous ZIP.

1. Run manual workflow `Build ASIO B5 Productization`.
2. Build must checkout current B5 branch and PASS compile/link/PE checks.
3. Workflow must print `B5 mux runtime marker verified in both DLLs` before packaging.
4. Only then download the new ZIP.
5. Close other X4 playback/default endpoint ownership as before.
6. Run the new `install_and_validate_b5.cmd` once.
7. Return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.

The next report is accepted as a mux test only if it contains `adapter=dual-event-mux-v1`.

The strict matrix still must reach:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output
- 48k/4800 output
- 48k/512 compatibility output

Only after full matrix PASS should final REAPER validation cover audible 24-bit output plus real stereo input together.
