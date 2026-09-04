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

`exp/windows-arm64-asio-b5-capability-productization@1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_PRODUCTIZATION_COMPILE_SDK_FIX.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_PASS_PRODUCTIZATION_IMPLEMENTED.md`
4. `NEXT_ACTION_ASIO.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
6. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
7. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
8. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
9. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

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

Clean combined capability report completed with:

`B5 COMBINED CAPABILITY PROBE RESULT: PASS`

Creative measured contract:

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

Independent B4D also re-passed lifecycle x3.

---

# B5 productization implementation

Current B5:

`1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

B5 remains descended from validated B4D with:

- ahead: 22
- behind: 0
- merge base: `a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated B4D core files remain untouched.

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

# Latest compile status

First productization build failed in new B5 WaveRT source on SDK compatibility and was fixed.

Second build proved that the ARM64EC B5 DLL itself compiles and links:

`x4-asio-arm64ec-b5.dll`

The build then failed only in `register_b5_arm64ec.cpp` because four standalone `_countof` uses remained.

Non-fatal linker warnings also showed that reusing B4D `driver.def` for B5 caused:

- LNK4104 on COM exports;
- LNK4070 due the B4D `LIBRARY "x4-asio-arm64"` name.

Second fix is now implemented:

- all register-helper `_countof` uses removed;
- B5-only `driver_b5.def` added;
- ARM64EC and Classic B5 targets use the B5-only export definition;
- original B4D `driver.def` remains unchanged.

The full productization workflow has not yet completed end-to-end.

---

# Immediate next action

Run manual workflow again:

`Build ASIO B5 Productization`

The rebuild must check out current B5:

`1821f4ff514aa1ee7bf2aa7a1091d6d09a20ef01`

Remaining proof targets in this one run:

- register helper compile/link;
- product validation helper compile/link;
- Classic ARM64 B5 DLL;
- PE architecture checks;
- final productization ZIP.

If it fails, inspect the exact next compiler/linker error and continue fixing on this same B5 branch. Do not request hardware micro-tests.

After Actions PASS, run packaged `install_and_validate_b5.cmd` once and return `B5_PRODUCT_VALIDATION_REPORT.txt`.

Only after that bundled silent runtime PASS should one final REAPER validation cover audible output + stereo input together.
