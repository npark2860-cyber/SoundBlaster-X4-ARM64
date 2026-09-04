# DEBUG HISTORY — ASIO B5 capability/reference probe implemented

Date: 2026-09-04 KST

## Status

Implementation complete; GitHub Actions build and hardware/runtime capability capture are still pending.

Do not treat this record as a runtime PASS.

## Source

Validated parent:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B5 implementation branch:

`exp/windows-arm64-asio-b5-capability-productization@bf5039e57ad0617db2e14269389f62c7e046bcb7`

Compare against B4D:

- status: ahead
- ahead by: 1
- behind by: 0
- B4D merge base: `a95a95d014bcc1c3a521be41325841ae96dc8a61`

## B4D core freeze

The B5-0 commit does not modify the validated transport/core files:

- `driver_b4c.cpp`
- `driver_b4d_arm64ec.cpp`
- `preflight.cpp`
- `preflight_b4d_arm64ec.cpp`
- `wavert_engine_b4a.cpp`
- `wavert_engine_b4d_arm64ec.cpp`

Only measurement tooling and CMake target declarations were added.

## Added B5-0 tools

### `capability_probe_b5_arm64ec.cpp`

ARM64EC ASIO host/reference probe. It discovers registered ASIO drivers through `SOFTWARE\\ASIO`, selects one exact substring match, and records:

- registry name / Description / CLSID / registry view
- `getDriverName()` / `getDriverVersion()`
- `getChannels()`
- `getBufferSize()` min/max/preferred/granularity
- `getSampleRate()`
- `canSampleRate()` candidates from 8 kHz through 384 kHz
- `getClockSources()`
- all `getChannelInfo()` records and raw ASIO sample types
- `getLatencies()`
- `future(kAsioCanTimeInfo)`
- repeated silent create/start/stop/dispose/reopen cycles

The lifecycle probe uses output buffers only and zero-fills them. It does not intentionally emit program audio.

### `ks_capability_probe_b5_arm64ec.cpp`

Property-only X4 KS probe. It finds the proven X4 `msft_wave` filter and queries:

- pin factory count
- pin dataflow
- `KSPROPERTY_PIN_CINSTANCES`
- `KSPROPERTY_PIN_GLOBALCINSTANCES`
- `KSPROPERTY_PIN_DATARANGES`
- `KSDATARANGE_AUDIO` channel / bit-depth / sample-frequency ranges

Returned data ranges are parsed on 64-bit boundaries as required by the KS property contract.

The program contains no `KsCreatePin` call.

### `probe_b5.cmd`

One consolidated user-side capture sequence:

1. X4 render Pin 1 local/global idle gate + KS data-range dump
2. ASIO registry enumeration
3. Creative `SB USB RT ASIO` report + three silent lifecycle cycles
4. wait and re-check X4 idle gate after Creative release
5. independent `Sound Blaster X4 ARM64` report + three silent lifecycle cycles

Creative name fallback `SB USB ASIO` is attempted only when the primary name does not resolve uniquely.

The sequence writes one file:

`B5_CAPABILITY_REPORT.txt`

Creative and independent drivers are never intentionally loaded concurrently.

## Safety

The old green-screen collision must not be recreated.

Before lifecycle probing, the tool requires the proven render ownership gate to be determinable and FREE. If either local/global gate is BUSY or indeterminate, the sequence stops.

The independent B4D driver also retains its own mandatory BUSY gate before `KsCreatePin`.

No B4D transport format or packet geometry was changed in B5-0.

## Manual build workflow

Main workflow added:

`.github/workflows/build-asio-b5-capability-arm64ec.yml`

Name:

`Build ASIO B5 Capability Probe ARM64EC`

Trigger:

- `workflow_dispatch` only

The workflow:

- checks out the B5 branch;
- verifies ancestry from the validated B4D SHA;
- fails if any frozen B4D core file differs from the validated parent;
- builds only the two B5 probe executables as ARM64EC;
- verifies PE machine `0x8664` plus Microsoft linker `ARM64X` identification;
- packages one ZIP artifact containing the two probes, `probe_b5.cmd`, and the B5 README.

## Next action

Run the manual B5 capability workflow. If the build passes, run the single packaged `probe_b5.cmd` on the X4 test system with REAPER/other X4 playback stopped and return `B5_CAPABILITY_REPORT.txt`.

Only after that report is available should B5 transport capability work be implemented. The next implementation batch should combine, as supported by the measured matrix:

- 24-bit output
- confirmed additional sample rates
- selectable buffer sizes
- lifecycle/stability hardening
- narrow stereo input

Do not split those into repeated A/B/C/D user-test stages.
