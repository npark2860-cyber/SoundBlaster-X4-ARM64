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

`exp/windows-arm64-asio-b5-capability-productization@9ae7ba97277ef2bfb11bb0dbce42f671ed20b20d`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
7. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
8. `NEXT_ACTION_ASIO.md`
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

# Mux v2 build status — second identical C2059

The next manual ARM64EC build again failed before DLL creation:

```text
Windows Kits\10\Include\10.0.26100.0\um\cguid.h(33,18):
error C2059: syntax error: '__uuidof'
```

while compiling `driver_b5_arm64ec.cpp`.

Therefore the previous explanation that relocating `#define private public` alone removed SDK contamination was incomplete.

This run has no runtime/hardware meaning.

---

# New compile fix — isolate KS headers from COM driver

Current B5:

`9ae7ba97277ef2bfb11bb0dbce42f671ed20b20d`

The mux-v2 driver translation units had added these headers before ASIO/COM declarations:

- `winioctl.h`
- `ks.h`
- `ksmedia.h`

Mux v2 no longer needs them directly because it uses the WaveRT engine's public API. They were removed from both ARM64EC and Classic B5 driver adapters.

Kernel Streaming headers remain in the WaveRT engine translation units, where KS IOCTL/property types are actually required.

No runtime logic or safety behavior changed:

- dual-event mux remains
- exact render N / capture N-1 pairing remains
- render write-ahead N+1 remains
- capture `ERROR_NOT_READY` remains transient
- real packet discontinuities remain fatal
- callback/copy/sync failures remain fatal
- MMCSS `Pro Audio` remains
- BUSY gates remain immutable

Runtime/build marker remains:

`dual-event-mux-v2`

---

# Remaining architecture risk if C2059 repeats

The ARM64EC adapter still temporarily defines `_M_ARM64` and undefines `_M_ARM64EC` around the shared `driver_b5.cpp` because that shared source currently rejects ARM64EC directly.

Microsoft documents that an ARM64EC compilation normally exposes x64-compatible architecture macros (`_M_AMD64`) plus `_M_ARM64EC`, not `_M_ARM64`.

Therefore, if the same `cguid.h::__uuidof` error repeats after the KS-header isolation change, do not do another include-order patch. The next engineering action is to make the B5 shared source directly ARM64EC-aware and remove the architecture macro shim entirely.

---

# Immediate next action

Do not hardware-test yet.

Run manual workflow:

`Build ASIO B5 Productization`

The build must prove:

1. ARM64EC B5 DLL compile/link PASS;
2. B5 helpers compile/link PASS;
3. Classic ARM64 B5 DLL compile/link PASS;
4. PE/ARM64X checks PASS;
5. both DLLs contain `dual-event-mux-v2`;
6. productization ZIP is produced.

If the identical `cguid.h::__uuidof` C2059 appears again, remove the ARM64EC architecture-spoof shim next; do not hardware-test or create a microbranch.

Only after full Actions PASS should the new ZIP be used with `install_and_validate_b5.cmd`.
