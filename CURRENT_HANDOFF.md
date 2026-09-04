# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch:

`main`

Verified `main` immediately before this handoff update:

`2e8272d867ceac914e865cfa3621010148fee09c`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 productization source:

`exp/windows-arm64-asio-b5-capability-productization@de87abca5540a077e5a9d9bf9708738ccbefecd5`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`
3. `NEXT_ACTION_ASIO.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_PROBE_IMPLEMENTED.md`
6. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
7. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
8. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
9. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

CTCDC remains deferred until the B5 first-release ASIO pass is closed.

---

# Proven fallback

Independent B4D real playback in REAPER ARM64EC remains hardware/user proven.

B4D known-good transport:

- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- local + global ownership gates
- joined worker stop
- ASIO 2.x time-info

Do not modify validated B4D unless a concrete B5 regression requires it.

---

# B5 capability capture — FULL PASS

The clean combined capability report completed with:

`B5 COMBINED CAPABILITY PROBE RESULT: PASS`

Initial Render Pin 1 gate:

- `C 0/1`
- `G 0/1`
- BUSY = NO

Creative `SB USB RT ASIO` measured contract:

- 2 inputs / 10 outputs
- every channel `Int24LSB` type 17
- buffer min 96 / max 4800 / preferred 240 / granularity 48
- supported rates 48 / 96 / 192 kHz only
- one `Internal Clock`
- latency at preferred buffer 240 in / 240 out
- time-info supported
- create/start/stop/dispose/reopen x3 PASS

X4 static KS ranges:

- Render Pin 1: 2/6/8ch, 16/24-bit, 48/96/192 kHz
- Capture Pin 4: stereo, 16/24-bit, 48/96 kHz

Independent B4D also re-passed three lifecycle cycles in the same report.

---

# B5 productization implementation

B5 productization was implemented on the same branch in one bundled cycle rather than A/B/C/D user micro-tests.

Current B5 branch:

`de87abca5540a077e5a9d9bf9708738ccbefecd5`

B5 is ahead of validated B4D with B4D as merge base. Frozen B4D core source remains unchanged.

Validation identity is side-by-side:

`Sound Blaster X4 ARM64 ASIO B5`

with a separate CLSID, so the proven B4D ASIO registration remains available during validation.

Implemented narrow B5 contract:

- 2 output channels
- 2 input channels at 48/96 kHz
- `Int24LSB` host format
- output 48/96/192 kHz
- 192 kHz exposes zero inputs because Capture Pin 4 does not advertise 192 kHz
- buffers 96..4800, 48-frame granularity, preferred 240
- 512 accepted as a compatibility exception
- Internal Clock
- ASIO 2.x time-info retained
- Render Pin 1 + Capture Pin 4 WaveRT
- full-duplex capture-start-before-render ordering
- input-only capture callback path
- Classic ARM64 and ARM64EC share functional `driver_b5.cpp` / `wavert_engine_b5.cpp`

## Safety remains immutable

Never bypass BUSY.

B5 preserves:

1. Render Pin 1 local/global preflight at ASIO `init()`;
2. Render Pin 1 local/global re-check immediately before render `KsCreatePin`;
3. Capture Pin 4 local/global re-check immediately before capture `KsCreatePin`;
4. mandatory joined worker before hardware teardown.

Historical collision class must never be intentionally reproduced:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path

---

# Current validation status

The capability/reference capture is proven.

The new B5 productized transport is **implemented but not yet compile/runtime proven**.

Manual workflow on `main`:

`.github/workflows/build-asio-b5-productization.yml`

Workflow name:

`Build ASIO B5 Productization`

It builds and architecture-checks:

- ARM64EC/ARM64X B5 DLL + tools
- Classic ARM64 B5 DLL from the same functional source

and packages:

`SoundBlaster-X4-ASIO-B5-Productization.zip`

---

# Immediate next action

1. Run `Build ASIO B5 Productization` once.
2. If compile fails, fix on the same B5 branch; do not ask for hardware micro-tests.
3. After Actions PASS, download the productization ZIP.
4. With other X4 playback closed/default output moved away if necessary, run `install_and_validate_b5.cmd` once.
5. Return `B5_PRODUCT_VALIDATION_REPORT.txt`.

The one script covers registration, idle gate, public contract, preferred/min/max/512 buffers, 48/96/192 output, 48/96 full duplex, and repeated lifecycle.

If the initial gate is BUSY/indeterminate, it stops before lifecycle work and must not be overridden.

Only after the bundled runtime report passes should one final REAPER B5 real-use validation cover audible output + stereo input as a single combined test.
