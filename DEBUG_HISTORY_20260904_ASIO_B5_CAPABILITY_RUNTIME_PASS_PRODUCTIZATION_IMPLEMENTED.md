# DEBUG HISTORY — ASIO B5 capability runtime PASS + productization implemented

Date: 2026-09-04 KST

## Source of truth

Repository: `npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

B5 productization branch after the bundled implementation:

`exp/windows-arm64-asio-b5-capability-productization@de87abca5540a077e5a9d9bf9708738ccbefecd5`

The B5 branch remains descended directly from the validated B4D history with B4D as merge base. Frozen B4D transport/core source files remain unchanged; B5 functionality is implemented in new B5-specific shared source.

---

# Capability capture result

The second combined B5 runtime capture completed with:

`B5 COMBINED CAPABILITY PROBE RESULT: PASS`

The initial property-only gate was idle:

- Render Pin 1 local `C 0/1`
- Render Pin 1 global `G 0/1`
- BUSY = NO
- property-only probe did not call `KsCreatePin`

## X4 KS ranges

### Render Pin 1

Advertised PCM combinations:

- channels: 2 / 6 / 8
- bits: 16 / 24
- sample rates: 48 / 96 / 192 kHz

### Capture Pin 4

Advertised stereo PCM combinations:

- 16-bit: 48 / 96 kHz
- 24-bit: 48 / 96 kHz

This fixes Pin 4 as the narrow first-release stereo capture path. 192 kHz capture is not advertised and must not be assumed.

---

# Creative behavioral reference — measured

Registry entry:

`Creative SB USB RT ASIO Device`

COM instantiation/init: PASS.

Measured public contract:

- inputs: 2
- outputs: 10
- every exposed input/output channel type: `17 (Int24LSB)`
- buffer min: 96 frames
- buffer max: 4800 frames
- preferred: 240 frames
- granularity: 48 frames
- supported sample rates from `canSampleRate()`: 48000 / 96000 / 192000 only
- clock sources: one `Internal Clock`
- latency at preferred buffer: 240 input / 240 output frames
- `future(kAsioCanTimeInfo)`: supported

Creative channel naming observed:

- input 0/1: `Audio-In L/R`
- output 0/1: `Front L/R`
- output 2/3: `Rear L/R`
- output 4/5: `Center/Sub`
- output 6/7: `Side L/R`
- output 8/9: `SPDIF-Out L/R`

Creative repeated lifecycle:

- createBuffers: PASS
- start: PASS
- stop: PASS
- three complete reopen cycles: PASS

---

# Independent B4D behavioral reference — measured in same run

Current independent driver still reports:

- inputs: 0
- outputs: 2
- sample type: `16 (Int16LSB)`
- buffer: fixed 512
- current/supported rate: 48000 only
- output latency: 512 frames
- time-info: supported

Three create/start/stop/reopen cycles all passed. Each cycle joined the worker and cleanly returned the WaveRT pin to STOP.

This re-confirms the B4D lifecycle baseline immediately before B5 productization.

---

# B5 productization implementation

B5 was implemented in one bundled cycle instead of A/B/C/D-style user micro-tests.

## Public first-release contract

B5 side-by-side validation identity:

`Sound Blaster X4 ARM64 ASIO B5`

B5 uses a separate CLSID so the proven B4D registration remains installed during validation.

Implemented narrow contract:

- output channels: 2
- input channels: 2 at 48/96 kHz
- inputs reported as 0 at 192 kHz because X4 Capture Pin 4 does not advertise 192 kHz
- ASIO sample type: `Int24LSB`
- output sample rates: 48 / 96 / 192 kHz
- input sample rates: 48 / 96 kHz
- ASIO buffers: 96..4800 frames, 48-frame granularity, preferred 240
- 512 frames accepted as an additional compatibility exception for B4D-era saved host settings
- Internal Clock
- ASIO 2.x time-info retained

Broad multichannel output remains deferred.

## WaveRT transport

New B5 WaveRT source is configurable for:

- Render Pin 1
- Capture Pin 4
- 24-bit packed stereo PCM
- 48/96/192 kHz render
- 48/96 kHz capture
- host-selected buffer frame count
- NotificationCount=2

The mapped WaveRT cyclic buffer request is derived from:

`frames * 2 channels * 3 bytes * 2 notifications`

B5 currently requires the returned WaveRT buffer geometry to match the requested geometry exactly. Unsupported/rounded hardware geometry therefore fails safely rather than silently changing the host contract.

Render continues the proven write-ahead rule:

`writePacket = PacketCount + 1`

Capture uses the WaveRT capture read-packet path and derives the cyclic slot from the returned packet number.

## Full duplex

- Capture RUN starts before Render RUN.
- Render notification is the full-duplex ASIO callback master.
- One completed capture packet is copied to planar ASIO input buffers before the host callback.
- The host callback fills planar output buffers.
- Output is then packed/interleaved into the next render packet.
- Input-only operation uses capture notifications as the callback master.

## Safety preserved

The historical render collision remains an immutable gate:

- `init()` still checks Render Pin 1 local + global ownership before any later pin creation is permitted.
- B5 WaveRT render prepare re-checks Render Pin 1 immediately before `KsCreatePin`.
- B5 capture prepare independently checks Capture Pin 4 local + global ownership immediately before its own `KsCreatePin`.
- BUSY/indeterminate has no override.
- joined-worker teardown remains mandatory; if worker join fails, hardware teardown is withheld.

## Dual-target maintainability

The functional B5 implementation is shared by Classic ARM64 and ARM64EC:

- `driver_b5.cpp`
- `wavert_engine_b5.cpp`

ARM64EC uses thin ABI adapters. A separate Classic ARM64 CMake entry builds the same shared functional source directly.

---

# Bundled validation

New manual workflow on `main`:

`.github/workflows/build-asio-b5-productization.yml`

Workflow name:

`Build ASIO B5 Productization`

It builds:

- ARM64EC/ARM64X B5 DLL
- Classic ARM64 B5 DLL from the same functional source
- B5 registration verifier
- B5 capability probe
- KS idle/range probe
- one-shot B5 product validation executable

The packaged script:

`install_and_validate_b5.cmd`

performs registration + idle gate + public-contract capture + a single bundled silent matrix:

- 48 kHz / 240 / output x3
- 48 kHz / 240 / full duplex x2
- 96 kHz / 240 / full duplex x2
- 192 kHz / 240 / output x2
- 48 kHz / 96 / output
- 48 kHz / 4800 / output
- 48 kHz / 512 / compatibility output

Output:

`B5_PRODUCT_VALIDATION_REPORT.txt`

## Current validation status

Implementation is complete, but the new B5 productization source has **not yet received Actions compile PASS or hardware runtime PASS**.

Do not claim the new 24-bit/rate/buffer/input transport is proven until both are obtained.
