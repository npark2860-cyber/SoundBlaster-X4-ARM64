# NEXT ACTION — Native ARM64 / ARM64EC ASIO

Updated: 2026-09-04 KST

## Validated baseline

B4D remains the proven fallback:

`exp/windows-arm64-asio-com-stage-b4d-reaper-registration@a95a95d014bcc1c3a521be41325841ae96dc8a61`

Known-good B4D runtime:

- REAPER ARM64EC real playback
- 48 kHz
- stereo output
- signed 16-bit PCM
- 512 ASIO frames
- Render Pin 1
- local + global BUSY gates
- joined worker stop
- ASIO 2.x time-info

Do not alter or re-prove B4D unless B5 exposes a concrete regression.

---

# Current B5 source

`exp/windows-arm64-asio-b5-capability-productization@1d6c3a6f3229b0d4d7b18009073fc878621bedae`

B5 first-release contract remains:

- 2 output channels, Int24LSB
- 2 input channels at 48/96 kHz, Int24LSB
- output 48/96/192 kHz
- 192 kHz reports zero input channels
- buffer 96..4800 / granularity 48 / preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- strict local/global BUSY gates before every pin creation
- joined worker before hardware teardown

---

# Latest runtime evidence

Product report generated 2026-09-04 11:29:00 advanced B5 substantially.

### PASS — 48 kHz / 240 output-only

Three cycles:

- callbacks=140 / 139 / 142
- clean `ASE_OK` stop
- workerJoined=YES
- no render packet discontinuity
- no callback index error
- no copy error

### PASS — 48 kHz / 240 full duplex

Two cycles:

- cycle 1: callbacks=139, renderNotif=140, captureNotif=139, outFrames=33360, inFrames=33360
- cycle 2: callbacks=138, renderNotif=139, captureNotif=138, outFrames=33120, inFrames=33120
- both clean `ASE_OK` stop

This proves the duplex lifecycle/copy path survives at 48 kHz. `inputNonzeroSamples=0` does not yet prove real external input signal content.

### FAIL — 96 kHz / 240 full duplex

The first cycle ran 259 callbacks, then strict stop diagnostics reported:

- worker=0
- idx=20
- outCopy=0
- inCopy=0
- rPkt=20
- rPos=0
- cPkt=0

Render PACKETCOUNT skipped absolute packets while capture stayed sequential. Because callback buffer parity is derived from the render packet, each +2 render jump also repeated the same host buffer index.

This is a realtime scheduling failure at the 2.5 ms period, not BUSY, `KsCreatePin`, KS state, copy, capture-packet or position-regression failure.

The report stopped at this case; 192 kHz/min/max/512 cases remain pending.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_RUNTIME_48K_PASS_96K_SCHEDULING_FIX.md`

---

# High-rate scheduling fix now implemented

ARM64EC and Classic ARM64 B5 adapters now run the shared worker using Windows MMCSS:

- `Pro Audio` task
- `AVRT_PRIORITY_CRITICAL`
- `THREAD_PRIORITY_HIGHEST` fallback only if MMCSS task registration fails
- proper MMCSS revert at worker exit
- `avrt.lib` linked only to B5 targets

The B5 static-CRT trace stream now uses a 2 MiB full buffer initialized before driver logging and flushes only after the worker exits. This preserves detailed packet diagnostics while removing trace file I/O from the realtime loop.

No diagnostic was relaxed. The next report still fails if it sees any render/capture packet discontinuity, callback-index repetition or copy error.

BUSY safety remains immutable.

---

# Immediate action

The DLL changed, so **do not reuse the previous validation ZIP**.

Run manual workflow:

`Build ASIO B5 Productization`

The workflow must check out:

`1d6c3a6f3229b0d4d7b18009073fc878621bedae`

If the build fails, fix the exact compiler/linker issue on this same B5 branch; do not request a hardware micro-test.

After Actions PASS:

1. download the new `SoundBlaster-X4-ASIO-B5-Productization.zip`;
2. close other X4 playback and move Windows default output away if necessary;
3. run the new `install_and_validate_b5.cmd` once;
4. return `B5_PRODUCT_VALIDATION_REPORT.txt`.

The full strict matrix must now reach and pass:

- 48k/240 output x3
- 48k/240 full duplex x2
- 96k/240 full duplex x2
- 192k/240 output x2
- 48k/96 output x1
- 48k/4800 output x1
- 48k/512 compatibility output x1

Only after the full matrix passes should the final REAPER test cover actual audible 24-bit output plus real stereo input together.

## Immutable safety

Never bypass BUSY.

Historical collision class remains:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery
