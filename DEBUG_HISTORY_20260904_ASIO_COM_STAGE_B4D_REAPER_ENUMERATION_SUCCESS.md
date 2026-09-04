# Stage B4D — REAPER ASIO Enumeration Success

Date: 2026-09-04 KST

## State before test

Validated B4D branch:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B4D ARM64EC build had already passed GitHub Actions ARM64X verification, registration verification, and a normal ARM64EC COM `CoCreateInstance()` host probe.

## REAPER observation

The Windows-on-Arm REAPER build successfully enumerated and selected the registered driver.

Observed in REAPER `Options -> Preferences -> Audio -> Device`:

- Audio system: `ASIO`
- ASIO Driver: `Sound Blaster X4 native ARM64 ASIO`
- no input channels populated, matching the current output-only driver
- output first: `1: X4 Output L`
- output last: `2: X4 Output R`
- 48 kHz shown in the sample-rate field

This proves the registered ARM64EC COM server is not only loadable by a synthetic host probe, but is also accepted by REAPER's ASIO enumeration path and its channel-query path.

## What this proves

Hardware/host-proven at this point:

1. ARM64EC/ARM64X DLL loads in a normal COM path.
2. REAPER sees the ASIO registration.
3. REAPER selects the driver without an immediate COM/DLL load failure.
4. REAPER successfully queries the current output-only channel contract.
5. Channel names from `getChannelInfo()` are visible in REAPER as `X4 Output L/R`.

## What is not yet proven

This screenshot alone does **not** yet prove:

- `createBuffers()` from REAPER with the intended 512-frame block
- REAPER transport `start()` / streaming callbacks
- actual REAPER PCM copied into WaveRT DMA
- audible project playback through X4
- repeated start/stop and REAPER close cleanup

## Frozen first playback configuration

For the first REAPER playback proof:

- disable inputs for this output-only driver
- request 48000 Hz
- use/request 512 frames, not the stale 256-frame value left from the previous Creative driver selection
- keep output range `1: X4 Output L` through `2: X4 Output R`
- avoid concurrent normal Windows playback through X4
- do not use the ASIO Configuration button as a variable in this first proof

The next milestone is actual REAPER transport and audible playback using this exact frozen configuration.
