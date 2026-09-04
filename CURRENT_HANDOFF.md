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

`exp/windows-arm64-asio-b5-capability-productization@1d6c3a6f3229b0d4d7b18009073fc878621bedae`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`
6. `NEXT_ACTION_ASIO.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
8. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
9. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
10. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
11. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

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

Implemented narrow first-release contract:

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz exposes zero input channels
- buffers 96..4800 / step 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- full-duplex capture-start-before-render ordering
- Classic ARM64 + ARM64EC share the functional B5 driver/engine source

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

# Latest runtime result — major progress

Returned product validation report generated 2026-09-04 11:29:00.

Registration/public contract again passed.

## 48 kHz / 240 output-only

Three lifecycle cycles PASS:

- callbacks 140 / 139 / 142
- stop=ASE_OK
- workerJoined=YES
- no packet/callback/copy diagnostics

## 48 kHz / 240 full duplex

Two lifecycle cycles PASS:

- cycle 1: callbacks=139, renderNotif=140, captureNotif=139, outFrames=33360, inFrames=33360
- cycle 2: callbacks=138, renderNotif=139, captureNotif=138, outFrames=33120, inFrames=33120
- both stop=ASE_OK

`inputNonzeroSamples=0` means this silent matrix does not yet prove actual microphone/line signal content. Real input still belongs in the final REAPER test.

## 96 kHz / 240 full duplex

First cycle reached RUN and processed 259 callbacks, then strict stop validation failed with:

- worker=0
- idx=20
- outCopy=0
- inCopy=0
- rPkt=20
- rPos=0
- cPkt=0

The trace shows render PACKETCOUNT skipped packet numbers while capture packet numbers remained sequential.

This narrows the defect to realtime render scheduling at a 2.5 ms period, not BUSY, pin creation, KS state, copy, capture packet, or presentation-position failure.

The matrix stopped there, so 192 kHz and later buffer-size cases remain unproven in this report.

---

# 96/192 kHz scheduling fix implemented

Current B5 branch:

`1d6c3a6f3229b0d4d7b18009073fc878621bedae`

ARM64EC and Classic B5 driver adapters now execute the existing shared worker through an MMCSS trampoline:

- `AvSetMmThreadCharacteristicsW(L"Pro Audio", ...)`
- `AvSetMmThreadPriority(..., AVRT_PRIORITY_CRITICAL)`
- fallback only on MMCSS registration failure: `THREAD_PRIORITY_HIGHEST`
- `AvRevertMmThreadCharacteristics(...)` at worker exit
- B5 targets link `avrt.lib`

Existing detailed per-notification diagnostics are retained, but the B5 DLL static CRT stdout is now placed on a 2 MiB full buffer at DLL initialization and flushed only after the worker loop exits, so trace file I/O is removed from the realtime hot path.

Strict diagnostics were **not** weakened:

- render/capture packet discontinuities remain fatal
- repeated callback buffer index remains fatal
- copy errors remain fatal
- BUSY remains immutable

Validated B4D core remains unchanged.

---

# Immediate next action

The new MMCSS/log-buffering code changes the B5 DLL, so the previous ZIP must not be reused for the high-rate retest.

1. Run manual workflow `Build ASIO B5 Productization` from current B5 branch.
2. If compile/link fails, fix on the same B5 branch; no hardware test until package PASS.
3. After Actions PASS, download the new `SoundBlaster-X4-ASIO-B5-Productization.zip`.
4. With other X4 playback closed/default endpoint moved away if needed, run the new `install_and_validate_b5.cmd` once.
5. Return the new `B5_PRODUCT_VALIDATION_REPORT.txt`.

The next strict report must get through the entire matrix:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output
- 48k/4800 output
- 48k/512 compatibility output

Only after full matrix PASS should one final REAPER test cover audible 24-bit output plus real stereo input together.
