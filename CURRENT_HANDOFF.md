# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Default branch:

`main`

Verified `main` immediately before this handoff update:

`f79e9f995895b16cab537d570908d2d65ae8a875`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 measurement/productization branch:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

At the start of the next chat, verify actual GitHub heads again. Do not reconstruct state from old conversation context.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`
3. `NEXT_ACTION_ASIO.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_PROBE_IMPLEMENTED.md`
5. `DEBUG_HISTORY_20260904_CREATIVE_SB_USB_RT_ASIO_ARM64EC_RUNTIME.md`
6. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
7. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_TIME_INFO_RUNTIME_SUCCESS.md`
8. `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4C_CORRECTED_SMOKE_BUSY_RUNTIME.md`
9. `DEBUG_HISTORY_20260904_ASIO_ACTIVE_PLAYBACK_COLLISION_RUNTIME.md`
10. `DEBUG_HISTORY_20260903_ASIO_WDF_CRASH_FINGERPRINT.md`

CTCDC remains deferred until the first B5 ASIO capability/productization pass is closed.

---

# Proven baseline

Independent B4D real playback in REAPER ARM64EC is hardware/user proven.

Known-good transport:

- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- X4 `msft_wave`, Render Pin 1
- WaveRT cyclic buffer 4096 bytes
- NotificationCount=2
- local + global BUSY ownership gates
- joined worker stop
- ASIO 2.x time-info callbacks

Do not modify the validated B4D source unless a concrete B5 regression requires it.

Creative `SB USB RT ASIO` is also proven to work in the same ARM64EC REAPER environment and is the black-box behavioral reference only.

---

# B5-0 build/runtime state

B5 measurement tooling is implemented at:

`bf5039e57ad0617db2e14269389f62c7e046bcb7`

The B5 Actions workflow shallow-checkout failure was fixed on main by changing checkout to full history and current checkout action. The workflow subsequently produced the packaged capability tools used for the first runtime capture.

First runtime report status:

**PARTIAL / SAFELY BUSY-BLOCKED**

The property-only KS probe completed and reported for Render Pin 1:

- local `C 0/1`
- global `G 1/1`
- BUSY = YES
- `KsCreatePin` never called

The combined script therefore stopped before Creative and independent ASIO COM/lifecycle measurement. This is correct behavior and must not be bypassed.

## Static KS capability evidence captured

### Render Pin 1

Advertised PCM ranges include the combinations represented by:

- channels: 2 / 6 / 8
- bits: 16 / 24
- rates: 48 / 96 / 192 kHz

### Capture Pin 4

Advertised stereo PCM ranges:

- 16-bit: 48 / 96 kHz
- 24-bit: 48 / 96 kHz

Pin 4 is the strongest current static candidate for first narrow stereo input work.

These ranges do **not** resolve the Creative ASIO sample packing, buffer contract, latency/clock behavior, or lifecycle contract.

---

# Immediate next action

Do not begin B5 transport implementation from this partial report.

Make the X4 Render Pin 1 globally idle and run the existing packaged `probe_b5.cmd` **one more time only**.

Preferred preparation:

1. switch Windows default playback away from the Sound Blaster X4;
2. close REAPER, foobar/media players, Creative App, and browser/media processes actively using X4;
3. run `probe_b5.cmd` once;
4. return the new `B5_CAPABILITY_REPORT.txt`.

The desired clean report must include Creative and independent behavioral data:

- input/output channel counts/names
- raw ASIO sample types, especially exact 24-bit packing
- buffer min/max/preferred/granularity
- `canSampleRate()` matrix
- latency reporting
- clock sources
- repeated create/start/stop/dispose/reopen behavior

If Render Pin 1 remains `G 1/1`, do not retry repeatedly and do not weaken BUSY. Add ownership diagnostics instead.

After that clean matrix is available, implement 24-bit + confirmed rates + measured buffer selection + lifecycle hardening + narrow stereo input + dual-target maintainability in one coherent B5 batch, followed by one combined validation package.

---

# Immutable safety rule

Never bypass BUSY and never intentionally reproduce the historical active-render green-screen collision.

Known failure class:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery

All render changes must preserve local/global ownership gates before pin creation. Input must use its own relevant ownership gate.
