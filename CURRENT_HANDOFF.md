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

`exp/windows-arm64-asio-b5-capability-productization@869307d44750af3e23c2de68dc84cc32d9b5e05f`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
7. `NEXT_ACTION_ASIO.md`
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

# Latest proven runtime before mux build

Returned report generated 2026-09-04 11:50:11.

48 kHz / 240 output-only:

- three cycles PASS
- callbacks 139 / 139 / 140
- stop=ASE_OK
- workerJoined=YES

48 kHz / 240 full duplex:

- two cycles PASS
- callbacks=141 each
- renderNotif=142 each
- captureNotif=141 each
- outFrames=inFrames=33840 each
- stop=ASE_OK

96 kHz / 240 full duplex failed because the original worker waited Render then Capture serially on one thread. Render auto-reset notifications coalesced while the worker blocked on Capture. Trace examples included 23->25, 35->37 and 43->45. Later Capture also showed a packet discontinuity and `GETREADPACKET` Win32 21.

This is why the B5 dual-event mux path was introduced.

---

# Latest build failure — mux v1 did not compile

Manual `Build ASIO B5 Productization` ARM64EC build failed before producing a DLL:

```text
Windows Kits\10\Include\10.0.26100.0\um\cguid.h(33,18):
error C2059: syntax error: '__uuidof'
```

The error occurred while compiling `driver_b5_arm64ec.cpp`.

Root cause: the first mux adapter placed `#define private public` around project headers. Those headers pulled Windows/COM SDK declarations while the C++ keyword macro was active, contaminating `cguid.h`.

This run provides no new hardware/runtime result.

---

# Mux v2 compile fix implemented

Current B5:

`869307d44750af3e23c2de68dc84cc32d9b5e05f`

## SDK header contamination removed

ARM64EC and Classic adapters now parse all Windows/COM/project headers normally before the translation-unit-local driver access macro is introduced.

The macro can no longer reach SDK declarations such as `cguid.h::__uuidof`.

## WaveRT mux access narrowed to public API

`wavert_engine_b5.h` now exposes:

`process_signaled_notification(...)`

Implementation:

`src/asio-arm64-stage-b0/wavert_engine_b5_signaled.inl`

Both ARM64EC and Classic engine adapters include it after the existing shared WaveRT implementation.

The mux no longer reads WaveRT private pin/state/stat fields. It uses:

- `notification_event()`
- `process_signaled_notification()`
- `stats()`
- existing render/capture copy methods

Capture `ERROR_NOT_READY` maps to `NoData` and is transient. Actual capture packet discontinuity remains strict/fatal.

## Runtime worker marker

Mux marker advanced to:

`dual-event-mux-v2`

Expected runtime lines:

`B5 worker realtime adapter=dual-event-mux-v2 ...`

`B5 worker START adapter=dual-event-mux-v2 ...`

The main build workflow now refuses to package unless this marker exists in both built DLLs.

---

# B4D protection status

Compare current B5 against validated B4D:

- status: ahead
- ahead_by: 41
- behind_by: 0
- merge base: exactly `a95a95d014bcc1c3a521be41325841ae96dc8a61`
- validated B4D core remains untouched

---

# Immediate next action

Do not hardware-test yet.

Run manual workflow:

`Build ASIO B5 Productization`

The build must now prove, in order:

1. ARM64EC B5 DLL compile/link PASS;
2. B5 register/product-validation/capability/KS helpers PASS compile;
3. Classic ARM64 B5 DLL compile/link PASS;
4. PE/ARM64X checks PASS;
5. both DLLs contain `dual-event-mux-v2`;
6. productization ZIP is produced.

If Actions fails, fix the exact compiler/linker/workflow error on this same B5 branch. Do not request a hardware micro-test.

Only after Actions PASS should the new ZIP be used with `install_and_validate_b5.cmd`.

The later strict runtime matrix still must cover:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output
- 48k/4800 output
- 48k/512 compatibility output

Only after full matrix PASS should final REAPER validation cover audible 24-bit output plus real stereo input together.
