# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated fallback

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Do not alter B4D unless B5 exposes a concrete regression.

Immutable safety:

- never bypass local/global BUSY gates
- never intentionally reproduce the active-render collision
- never tear hardware down before the worker is joined

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@075010999bed5c433f03d7421f2fa3a18221bd98`

Runtime/build markers:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

---

# Silent product matrix status

The prior full matrix generated `2026-09-04 13:21:47.16` and passed completely:

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

This included the measured 192 kHz first-release contract:

`min=384 max=4800 preferred=384 granularity=48`

and 192k/384 output x2 PASS.

96k/240 duplex also lifecycle-passed, while capture remained behind render by roughly 26..27 phase misses in the short silent harness. Do not weaken strict checks to hide that observation.

---

# New real-host regression — sustained buzz/drone

REAPER ARM64EC showed the B5 device active at:

- 48 kHz
- 24-bit
- 2 in / 2 out
- 480 samples
- approximately 10 ms input + 10 ms output

During actual audible playback, output later became a very loud sustained `drone/buzz` tone. REAPER left no useful diagnostic log.

480 frames at 48 kHz is valid and equals 10 ms. Do not assume the buffer size itself is invalid.

The previous worker fatal path could return while the WaveRT render pin remained RUN and the previous cyclic contents remained audible. That matches the symptom, but the exact fatal reason is not known yet.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_REAPER_BUZZ_RUNTIME_FAILSAFE_V1.md`

---

# Implemented runtime fail-safe v1

On every fatal mux/worker path the B5 adapter now:

1. snapshots the pre-failure render/capture stats and messages;
2. marks `worker_failed_`;
3. overwrites both WaveRT render notification slots with silence through the existing render copy API;
4. does **not** stop/dispose/close the pin inside the worker;
5. then writes a one-shot diagnostic record to `OutputDebugString` and:

`%TEMP%\B5_RUNTIME_FAILURE.txt`

The log includes:

- direction/reason/Win32 value
- rate and frames
- callback/index/copy counters
- render packet discontinuity and position regression counters
- render/capture last packet and notification counts
- captureNotReady / MoreData / phase misses / consumed packets
- render/capture engine messages
- emergency-silence success/failure

File I/O occurs only after the silence attempt.

The joined-worker-before-hardware-teardown invariant is unchanged.

---

# REAPER-matched validator case

The silent product validator now additionally runs:

`reaper-48-480-output`

- 48 kHz
- 480 frames
- output-only
- 5 seconds

This matches the observed REAPER host geometry and should pass before another audible host test.

---

# Workflow protection

The manual productization workflow now refuses packaging unless both ARM64EC and Classic ARM64 B5 DLLs contain:

- `dual-event-mux-v3`
- `runtime-failsafe-v1`

B4D ancestry/frozen-core checks remain unchanged.

---

# Immediate action

1. run manual workflow `Build ASIO B5 Productization`;
2. require complete build/package PASS and both runtime markers PASS;
3. install the new ZIP and run `install_and_validate_b5.cmd` once;
4. confirm the new `reaper-48-480-output` 5-second case passes;
5. only then do one normal REAPER 48k/480 audible playback test;
6. do **not** intentionally provoke or repeatedly retry the loud-buzz failure;
7. if the symptom occurs again, stop testing and return `%TEMP%\B5_RUNTIME_FAILURE.txt` immediately.

Do not start ASIO control-panel implementation until this concrete real-playback safety regression is either not reproduced on the fail-safe build or, if it recurs, its runtime failure record has been analyzed and the actual cause fixed.

After that, resume the planned native ASIO control-panel milestone, then final REAPER real-signal output/input validation, then freeze B5 first release and return to deferred CTCDC/CTIntrfu work.
