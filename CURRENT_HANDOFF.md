# CURRENT HANDOFF — Sound Blaster X4 Windows ARM64

Updated: 2026-09-04 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Validated B4D source:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Validated Classic ARM64 B4C source:

`exp/windows-arm64-asio-com-stage-b4c-time-info@e23e9801a1dfefc421f02790e9b2dd10fc9442d8`

Current B5 productization source:

`exp/windows-arm64-asio-b5-capability-productization@d84ed0e7f8f5c4402b44d577140a4780b2aa0bf3`

At the start of a later chat, verify actual GitHub heads again. Do not reconstruct state from conversation memory.

## Read order

1. `CURRENT_HANDOFF.md`
2. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V3_96K_PASS_192K_GEOMETRY_PROBE.md`
3. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_RUNTIME_96K_PHASE_DECOUPLE_V3.md`
4. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_CGUID_SECOND_FAILURE_KS_HEADER_ISOLATION.md`
5. `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V2_ARM64EC_CGUID_COMPILE_FIX.md`
6. `DEBUG_HISTORY_20260904_ASIO_B5_96K_DUPLEX_EVENT_COALESCING_MUX_FIX.md`
7. `NEXT_ACTION_ASIO.md`
8. older B5/B4D runtime histories as needed

CTCDC remains deferred until the B5 first-release ASIO pass is closed.

---

# Proven fallback

B4D remains hardware/user proven in REAPER ARM64EC:

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

# Immutable safety

Never bypass BUSY.

B5 retains:

1. Render Pin 1 local/global preflight at ASIO `init()`;
2. Render Pin 1 local/global re-check before render `KsCreatePin`;
3. Capture Pin 4 local/global re-check before capture `KsCreatePin`;
4. mandatory joined worker before hardware teardown.

Historical collision class must never be intentionally reproduced:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` recovery path

---

# B5 first-release contract

- 2 outputs, Int24LSB
- 2 inputs at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz reports zero inputs
- ASIO buffer 96..4800 / step 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT

Important: the previous implementation assumed ASIO host buffer frames and WaveRT notification packet frames are always 1:1. The 192 kHz runtime now shows this assumption may not hold at every sample rate.

---

# Build status

The prior ARM64EC `cguid.h::__uuidof` compile problem is resolved enough for B5 productization to build and execute.

Current runtime/build marker:

`dual-event-mux-v3`

Main workflow refuses to package unless both ARM64EC and Classic ARM64 DLLs contain that marker.

---

# Latest runtime evidence — mux v3

Returned report generated `2026-09-04 12:37:34.26`.

Registration: PASS.

Property-only Render Pin 1 idle gate: FREE.

KS capability probe: PASS.

## 48 kHz / 240 output-only

Three cycles PASS:

- callbacks 139 / 139 / 140
- stop=ASE_OK
- workerJoined=YES

## 48 kHz / 240 full duplex

Two cycles PASS:

- callbacks 140 / 140
- renderNotif 140 / 140
- captureNotif 139 / 139
- stop=ASE_OK

Mux-v3 counters showed one capture phase miss per cycle and 139 capture packets consumed.

## 96 kHz / 240 full duplex

Two cycles PASS:

- callbacks 278 / 278
- renderNotif 278 / 278
- captureNotif 252 / 255
- outFrames 66720 / 66720
- inFrames 60480 / 61200
- stop=ASE_OK
- no strict packet/index/copy error

This proves mux-v3 removed mux-v2's false-positive exact-phase failure.

However capture remains behind render at 96 kHz:

- capturePhaseMisses 27 / 23

Treat this as a remaining capture cadence/latency quality issue, not as a closed 1:1 duplex timing result.

---

# New blocker — 192 kHz WaveRT buffer geometry

The first 192 kHz / 240 output cycle failed during `createBuffers()` before worker creation or KSSTATE_RUN:

`B5 RENDER BUFFER_WITH_NOTIFICATION FAILED Win32=87 requested=2880`

Current 1:1 geometry at 192 kHz is:

- ASIO host frames = 240
- stereo Int24 = 6 bytes/frame
- notification count = 2
- 1440 bytes per notification
- 2880-byte cyclic request
- 1.25 ms per notification

The exact same 2880-byte cyclic size succeeds at 48 and 96 kHz. Therefore simple byte divisibility is not sufficient to explain the 192 kHz rejection. The leading hypothesis is a sample-rate-dependent WaveRT/usbaudio2 service-period or minimum-duration constraint.

Do not patch product geometry by guesswork before measuring accepted sizes.

---

# 192 kHz geometry probe implemented

Current B5 branch includes a measurement-only tool:

`x4-asio-stage-b5-192k-geometry-probe.exe`

Runner:

`probe_b5_192k_geometry.cmd`

Safety behavior:

- X4 `msft_wave`, Render Pin 1 only
- 192 kHz / stereo / 24-bit PCM
- never enters KSSTATE_RUN
- checks Render Pin 1 local + global FREE before every `KsCreatePin`
- one fresh pin per candidate
- requests only `KSPROPERTY_RTAUDIO_BUFFER_WITH_NOTIFICATION`
- records requested geometry, Win32 result and ActualBufferSize
- closes the pin after each candidate
- refuses any next `KsCreatePin` unless local/global counts return FREE

Scan range:

- 48..960 frames per notification
- step 48 frames
- 0.25..5.0 ms per notification at 192 kHz
- NotificationCount=2

This brackets the failing 240-frame / 1.25 ms geometry and includes common period candidates such as 192, 384, 480 and 960 frames per notification.

The main productization workflow now builds/packages the probe and runner, but never executes them in Actions.

---

# B4D protection status

Validated B4D core remains untouched. Never weaken or bypass its safety model.

---

# Immediate next action

Run manual workflow:

`Build ASIO B5 Productization`

The build must include:

- `x4-asio-stage-b5-192k-geometry-probe.exe`
- `probe_b5_192k_geometry.cmd`

After Actions PASS:

1. download the new ZIP;
2. close REAPER, media players, Creative App playback and other X4 users;
3. do **not** rerun the full product matrix first;
4. run `probe_b5_192k_geometry.cmd` once;
5. return `B5_192K_GEOMETRY_REPORT.txt`.

Use that report to determine the first accepted 192 kHz WaveRT notification geometry. Only then decide whether B5 needs host-buffer/hardware-packet decoupling and what factor/latency it should use.

Do not repeatedly probe if the tool reports BUSY/INDETERMINATE or if the render pin does not return FREE after a candidate.
