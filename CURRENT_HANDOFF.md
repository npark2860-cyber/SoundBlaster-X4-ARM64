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

`exp/windows-arm64-asio-b5-capability-productization@7223257c0e86ea9c0a64b90f61968d00496011ab`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`
5. `NEXT_ACTION_ASIO.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
7. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
8. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
9. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
10. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

CTCDC remains deferred until the B5 first-release ASIO pass is closed.

---

# Proven fallback

Independent B4D real playback in REAPER ARM64EC remains hardware/user proven.

Known-good B4D:

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

# B5 capability/reference — FULL PASS

Creative measured reference remains:

- 2 inputs / 10 outputs
- all channels `Int24LSB` type 17
- buffer 96..4800 / preferred 240 / granularity 48
- 48 / 96 / 192 kHz
- Internal Clock
- preferred latency 240 in / 240 out
- time-info supported
- lifecycle x3 PASS

X4 KS evidence:

- Render Pin 1: 2/6/8ch, 16/24-bit, 48/96/192 kHz
- Capture Pin 4: stereo, 16/24-bit, 48/96 kHz

Independent B4D lifecycle x3 also re-passed.

---

# B5 productization implementation

Current B5:

`7223257c0e86ea9c0a64b90f61968d00496011ab`

B5 side-by-side identity:

`Sound Blaster X4 ARM64 ASIO B5`

Implemented narrow contract:

- 2 outputs, `Int24LSB`
- 2 inputs at 48/96 kHz
- output 48/96/192 kHz
- 192 kHz exposes zero input channels
- buffers 96..4800 / step 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- full-duplex capture-start-before-render ordering
- Classic ARM64 + ARM64EC shared functional source

Safety remains immutable:

- Render Pin 1 local/global preflight at `init()`
- Render Pin 1 re-check immediately before render `KsCreatePin`
- Capture Pin 4 re-check immediately before capture `KsCreatePin`
- joined worker before hardware teardown

Never bypass BUSY and never intentionally reproduce historical `WDF_VIOLATION 0x10D`, Parameter 1 = 5.

---

# Build/package status

The B5 ARM64EC DLL and helper executables are now proven to compile/load sufficiently to generate a real product validation report on the X4 system.

The returned runtime package successfully:

- registered B5 side-by-side;
- verified its registry/CLSID/DLL path;
- ran the property-only KS probe;
- instantiated B5 through COM;
- reported the exact B5 public ASIO contract.

The report therefore supersedes the earlier compile-only pending state for the ARM64EC validation path.

---

# Latest B5 runtime — BUSY race, not transport failure

Returned `B5_PRODUCT_VALIDATION_REPORT.txt` generated 2026-09-04 11:24:32 showed:

Initial immutable property-only gate:

- Render Pin 1 `C 0/1`
- Render Pin 1 `G 0/1`
- BUSY = NO

B5 public ASIO report then passed:

- init FREE
- 2 in / 2 out
- all exposed channels Int24LSB type 17
- buffer 96..4800 / preferred 240 / granularity 48
- 48/96/192 kHz
- Internal Clock
- latency 240/240
- time-info supported

Immediately afterward, the first product-matrix `init()` observed:

- Render Pin 1 `C 0/1`
- Render Pin 1 `G 1/1`
- BUSY
- `KsCreatePin` skipped

Result:

`B5 PRODUCT VALIDATION RESULT: BUSY_BLOCKED`

This is correct safety behavior. It is not evidence of a B5 transport crash.

The capability probe source only calls `init()` and public query APIs and releases the COM object. It does not call `createBuffers()`, `start()`, or `KsCreatePin`, so this report does not support blaming the capability probe itself for the global pin owner.

The supported interpretation is an external ownership race/window between separate validation processes.

---

# Script fix

`install_and_validate_b5.cmd` was updated on the B5 branch to:

1. register/verify;
2. run the immutable property-only idle gate;
3. immediately run the actual product lifecycle matrix while the FREE window is current;
4. classify matrix exit code 10 as BUSY_BLOCKED, not generic FAIL;
5. take one post-block property-only KS snapshot without opening a pin;
6. move the public ASIO capability report after lifecycle PASS.

No BUSY gate was weakened.

---

# Immediate next action

Do **not** rebuild just to test the missing matrix.

The current package already contains `x4-asio-stage-b5-product-validation.exe`, and that EXE performs its own B5 `init()` ownership gate before any pin creation.

With X4 not used by other playback and preferably not the Windows default playback endpoint, run the existing matrix executable **once directly**, skipping the preceding capability process.

Capture its stdout/stderr and return that result.

If it again immediately reports Render Pin 1 `G 1/1`, do not repeatedly retry. The next engineering step is ownership diagnostics.

If the direct matrix passes, proceed to one final REAPER B5 real-use validation covering audible output + stereo input together.
