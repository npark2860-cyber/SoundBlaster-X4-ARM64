# DEBUG HISTORY — Creative SB USB RT ASIO on REAPER ARM64EC

Updated: 2026-09-04 KST

## Result

The existing Creative `SB USB RT ASIO` driver is also usable in the current Windows-on-Arm REAPER environment.

Observed by the user in REAPER:

- `Creative SB USB RT ASIO` is listed as an ASIO driver.
- It can be selected.
- Its expected Creative input/output channel names are exposed.
- Actual playback works correctly.

## Significance

This corrects an earlier practical assumption that Windows ARM on this machine had no usable X4 ASIO path at all.

The current REAPER Windows-on-Arm build is ARM64EC, and the existing Creative ASIO path works in that host environment. Therefore the independent X4 ASIO implementation is not valuable merely because it makes ASIO possible today.

Its value is instead:

1. independent operation without Creative ASIO runtime dependencies;
2. a controllable/open implementation for capability and stability work;
3. preservation of a Classic ARM64 implementation path if a future host becomes pure ARM64 rather than ARM64EC;
4. a reference platform for understanding X4 WaveRT/ASIO behavior without modifying Creative binaries.

Do not infer from this test that the Creative driver would load in a future pure Classic ARM64 host. That scenario was not tested.

## Reference-driver role

Because the Creative driver works on the same PC, same X4, and same REAPER host, it can now be used as a behavioral reference while completing the independent driver.

High-value comparisons include:

- `getChannels()`
- `getBufferSize()`
- `getLatencies()`
- `getSampleRate()` / `canSampleRate()`
- clock source reporting
- channel names and sample types
- supported sample-rate set
- buffer-size behavior
- input/output exposure
- start/stop/reopen behavior

Use the Creative implementation as a black-box/static reference only. Do not copy proprietary implementation code, load Creative binaries from the independent driver, or redistribute Creative binaries.

## Relationship to B4D

The independent Stage B4D driver remains separately proven in real REAPER playback:

`REAPER ARM64EC -> independent ARM64EC ASIO DLL -> WaveRT -> usbaudio2.sys -> Sound Blaster X4 -> audible playback`

See:

- `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B4D_REAPER_PLAYBACK_RUNTIME_SUCCESS.md`
- `NEXT_ACTION_ASIO.md`
