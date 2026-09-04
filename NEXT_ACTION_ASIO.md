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

# B5 productization state

Current B5 branch:

`exp/windows-arm64-asio-b5-capability-productization@7223257c0e86ea9c0a64b90f61968d00496011ab`

Implemented contract:

- 2 outputs, `Int24LSB`
- 2 inputs at 48/96 kHz, `Int24LSB`
- output at 48/96/192 kHz
- 192 kHz reports zero inputs
- buffers 96..4800, granularity 48, preferred 240
- 512 compatibility exception
- Internal Clock
- ASIO 2.x time-info
- Render Pin 1 + Capture Pin 4 WaveRT
- full-duplex capture-before-render start ordering
- joined worker lifecycle
- render/capture local+global BUSY gates before `KsCreatePin`
- shared functional B5 source for Classic ARM64 and ARM64EC

Validation identity remains side-by-side:

`Sound Blaster X4 ARM64 ASIO B5`

---

# Latest runtime report

`B5_PRODUCT_VALIDATION_REPORT.txt` generated 2026-09-04 11:24:32 proved:

- B5 registration PASS
- registry verify PASS
- initial KS property-only gate `C 0/1 G 0/1`, BUSY=NO
- B5 COM creation PASS
- B5 public ASIO capability PASS
- 2 in / 2 out
- Int24LSB type 17
- buffers 96..4800 / preferred 240 / granularity 48
- 48/96/192 kHz only
- Internal Clock
- latency 240/240
- time-info supported

The actual lifecycle matrix then stopped on its first `init()` because Render Pin 1 had changed to:

- `C 0/1`
- `G 1/1`
- BUSY

and B5 correctly skipped `KsCreatePin`.

This is a safety block, not a transport crash.

The capability probe does not call `createBuffers()`, `start()`, or `KsCreatePin`; therefore the report does not identify the capability probe as the owner. The supported interpretation is an external pin-ownership race between separate validation processes.

See:

`DEBUG_HISTORY_20260904_ASIO_B5_PRODUCT_VALIDATION_RUNTIME_BUSY_RACE.md`

---

# Script correction already committed

`install_and_validate_b5.cmd` now runs in this order:

1. register/verify;
2. immutable property-only idle gate;
3. **product lifecycle matrix immediately**;
4. post-matrix capability report.

Matrix exit code 10 is now classified as BUSY_BLOCKED rather than generic FAIL, and one property-only KS snapshot is recorded after the block.

No ownership gate was weakened.

---

# Immediate action — no rebuild required for this attempt

Use the already-downloaded validation package.

Do not run the old `install_and_validate_b5.cmd` again for this attempt, because that old copy performs the capability process before the matrix.

With REAPER/media players/Creative App and other X4 playback closed, and preferably with Windows default playback moved away from X4, run the existing:

`x4-asio-stage-b5-product-validation.exe`

**once directly** from that package.

Redirect its output to a text file and return the file.

The executable itself still enforces the Render Pin 1 `init()` BUSY gate before any `KsCreatePin`, so this does not weaken safety.

If the direct matrix immediately reports `G 1/1` again, do not repeatedly retry. Implement ownership diagnostics next.

If the matrix passes, the next and final B5 first-release validation is one REAPER test covering audible output + stereo input together.

## Immutable safety

Never bypass BUSY.

Historical collision class remains:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = 5
- stale/destroyed `WDFUSBPIPE` path in `usbaudio2` recovery
