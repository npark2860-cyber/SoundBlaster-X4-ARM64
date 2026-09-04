# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Current validated baseline

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Known-good B4D transport remains:

- REAPER ARM64EC real playback
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

Do not rewrite or re-prove B4D unless B5 introduces a regression.

---

# B5-0 measurement state

B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

B5-0 capability tooling has now built and executed on the test system.

First runtime report:

`DEBUG_HISTORY_20260904_ASIO_B5_CAPABILITY_RUNTIME_BUSY_BLOCKED.md`

The property-only KS probe succeeded and the immutable Render Pin 1 gate observed:

- `C 0/1`
- `G 1/1`
- `BUSY=YES`
- `KsCreatePin` never called

The combined probe correctly stopped before Creative or independent ASIO lifecycle probing.

## Static X4 capability evidence now known

### Render Pin 1

Advertised PCM combinations include:

- 2 / 6 / 8 channels
- 16-bit / 24-bit
- 48 / 96 / 192 kHz

### Capture Pin 4

Advertised PCM combinations include stereo:

- 16-bit at 48 / 96 kHz
- 24-bit at 48 / 96 kHz

Pin 4 is the current static candidate for the first narrow stereo input path.

These KS ranges are capability evidence, not yet a complete ASIO product specification.

---

# Immediate action — one clean combined capture

Do not start productization from the partial report yet.

Make Render Pin 1 globally idle, then run the existing `probe_b5.cmd` once more.

Preferred preparation:

1. switch Windows default playback away from Sound Blaster X4 to another output device;
2. close REAPER, foobar/media players, Creative App, and any browser/media process actively using X4;
3. run `probe_b5.cmd` once;
4. return the new `B5_CAPABILITY_REPORT.txt`.

If the report still says `G 1/1`, do not bypass BUSY and do not repeatedly retry. The next engineering step is to add ownership diagnostics.

## Missing behavioral matrix required before implementation

The clean report must capture Creative `SB USB RT ASIO` first, then the independent driver, including:

- channel counts/names
- raw ASIO sample types, especially exact 24-bit packing
- buffer min/max/preferred/granularity
- `canSampleRate()` matrix
- latency reporting
- clock sources
- repeated create/start/stop/dispose/reopen behavior

---

# Then perform one B5 productization batch

Do not return to A/B/C/D micro-stages and do not request user testing after every internal change.

Once the clean Creative + KS matrix is available, implement on the same B5 branch in one coherent cycle:

1. **24-bit output** using the measured ASIO + X4 format contract.
2. **Additional sample rates** only from confirmed behavioral/static support.
3. **Selectable buffer sizes** from the measured Creative contract while keeping 512 frames as a regression option.
4. **Lifecycle/stability hardening** for repeated reopen/start/stop and longer playback.
5. **Narrow stereo input** using the confirmed capture pin/format and its own ownership gate.
6. **Classic ARM64 / ARM64EC maintainability** with shared functional source where practical.

Then produce one combined validation package.

## Immutable safety rule

Never bypass BUSY.

Never intentionally reproduce the historical active-render collision:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery

Every new render path must retain local/global ownership checks before pin creation. Input must establish and respect its own ownership state.
