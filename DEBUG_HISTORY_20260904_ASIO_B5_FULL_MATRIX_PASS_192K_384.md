# DEBUG HISTORY — ASIO B5 full product matrix PASS with measured 192 kHz / 384-frame contract

Date: 2026-09-04 KST

## Source

User-returned `B5_PRODUCT_VALIDATION_REPORT.txt`

Generated: `2026-09-04 13:21:47.16`

Current B5 source used for this validation:

`exp/windows-arm64-asio-b5-capability-productization@1ba2faabb922be0f002d698019c7be6e602ff3bc`

Runtime marker:

`dual-event-mux-v3`

---

# Result

`B5 PRODUCT VALIDATION RESULT: PASS code=0`

`B5 INSTALL + PRODUCT VALIDATION: PASS`

This is the first returned B5 product report that completes the entire intended first-release silent validation matrix successfully.

---

# Registration / capability

PASS:

- B5 registration
- registry verification
- property-only Render Pin 1 idle gate: `C 0/1 G 0/1 busy=NO`
- KS capability probe
- ASIO capability probe
- B5 side-by-side registry entry remains separate from Creative and B4D

Driver report at default 48 kHz:

- driverVersion 200
- 2 inputs / 2 outputs
- Int24LSB
- buffer 96..4800 / preferred 240 / granularity 48
- supported rates 48/96/192 kHz only
- one Internal Clock
- latency 240/240 before buffers at 48 kHz
- ASIO time-info support reported

---

# Lifecycle matrix

## 48 kHz / 240 / output-only x3

PASS all cycles.

Callbacks:

- 140
- 141
- 139

Each stopped with:

- `stop=0`
- `workerJoined=YES`
- no capture path

## 48 kHz / 240 / full duplex x2

PASS both cycles.

Cycle 1:

- callbacks 141
- renderNotif 141
- captureNotif 140
- capturePhaseMisses 1
- captureConsumed 140

Cycle 2:

- callbacks 141
- renderNotif 141
- captureNotif 142
- capturePhaseMisses 0
- captureConsumed 141

Both stopped cleanly with no strict packet/index/copy failure.

## 96 kHz / 240 / full duplex x2

PASS both cycles.

Cycle 1:

- callbacks 282
- renderNotif 282
- captureNotif 255
- capturePhaseMisses 27
- captureConsumed 255

Cycle 2:

- callbacks 280
- renderNotif 280
- captureNotif 254
- capturePhaseMisses 26
- captureConsumed 254

Both stopped cleanly with no strict packet/index/copy failure.

Important: 96 kHz capture cadence still trails render by roughly 26..27 callback periods over the validation window. This remains a latency/cadence quality observation, not a lifecycle failure. Do not weaken strict checks to hide it.

## 192 kHz / 384 / output-only x2

The measured rate-specific contract is now proven in RUN, not only in allocation probe.

Public contract:

- min 384
- max 4800
- preferred 384
- granularity 48

WaveRT geometry:

- 384 frames per notification
- 2304 bytes per packet
- 4608-byte cyclic buffer
- 2.0 ms notification period

Cycle 1:

- callbacks 350
- renderNotif 350
- latencyOut 384
- stop=0
- outFrames 134400

Cycle 2:

- callbacks 348
- renderNotif 348
- latencyOut 384
- stop=0
- outFrames 133632

This closes the previous `Win32=87 requested=2880` blocker for the first-release architecture.

## Boundary / compatibility cases

48 kHz / 96 output:

- PASS
- callbacks 250
- latency 96

48 kHz / 4800 output:

- PASS
- callbacks 9
- latency 4800

48 kHz / 512 compatibility output:

- PASS
- callbacks 65
- latency 512

---

# Interpretation

The first-release B5 silent product matrix is now closed.

Proven in the product harness:

- side-by-side COM/ASIO registration
- immutable local/global ownership gates
- Int24LSB public contract
- 48/96/192 output rates
- 48/96 input exposure
- rate-specific 192 kHz buffer contract
- full-duplex lifecycle at 48/96
- joined-worker shutdown
- strict packet/index/copy/position checks remain enabled
- buffer boundary and 512 compatibility cases

Not yet proven:

1. audible real 24-bit program output through B5 in REAPER;
2. real non-zero stereo input through B5 while output is active;
3. subjective/longer-run stability under actual DAW load;
4. whether 96 kHz capture phase misses produce an audible/recording-quality problem in a real host.

`inputNonzeroSamples=0` in this silent validation is expected when no external signal is present and must not be used as evidence that capture audio content works.

---

# Next action

Do not change B5 code before real-host validation unless a concrete regression appears.

Run REAPER ARM64EC with `Sound Blaster X4 ARM64 ASIO B5` and validate:

- 48 kHz or 96 kHz
- 24-bit output audibly
- real stereo input signal at the same time
- create/start/stop/reopen lifecycle

Capture evidence should include whether both input channels receive non-zero real signal and whether playback/recording remains stable.

Only after that host-level pass should B5 first release be considered closed and CTCDC work resume.
