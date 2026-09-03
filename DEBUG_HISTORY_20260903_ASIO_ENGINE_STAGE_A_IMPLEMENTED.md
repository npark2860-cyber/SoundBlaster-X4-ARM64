# DEBUG HISTORY — Native ARM64 ASIO Engine Stage A implemented

Date: 2026-09-03 KST

Branch: `exp/windows-arm64-asio-engine-stage-a`

Base main HEAD: `fb4b6f223556f1c62b3d0311c50f2dd7ab214dc6`

## Scope lock

Stage A implements only the previously hardware-proven render path:

- Sound Blaster X4 `msft_wave` filter discovery
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM
- 4096-byte WaveRT cyclic buffer
- notification count = 2
- two 512-frame logical host buffers
- packet-count continuity validation
- presentation-position monotonicity validation
- clean notification unregister / STOP / close

Not implemented in this stage:

- Capture Pin 4
- 24-bit
- 96/192 kHz
- multichannel
- dynamic buffer sizes
- sample-rate switching
- ASIO COM registration / `IAsio`
- Creative DLL loading

## Runtime model

The engine opens the same hardware-confirmed path used by the successful active WaveRT probe.

For each DMA notification:

1. query `KSPROPERTY_RTAUDIO_PACKETCOUNT`
2. query `KSPROPERTY_RTAUDIO_PRESENTATION_POSITION`
3. derive the completed logical half-buffer index from `(packetCount - 1) & 1`
4. expose that 512-frame half-buffer to the Stage A callback
5. current callback fills the half-buffer with silence
6. validate packet continuity, buffer 0/1 alternation, and monotonic sample position

The test harness performs three full open -> run -> stop -> close lifecycles, with 64 callbacks per run.

PASS requires:

- 192 callbacks total
- zero packet discontinuities
- zero buffer alternation errors
- zero presentation-position regressions
- all three resource lifecycles complete

## Independence

The implementation has no Creative runtime dependency.

It imports only Windows system components:

- `KERNEL32.dll`
- `SETUPAPI.dll`
- `KSUSER.dll`

Creative `CtU2As64.dll`, `CTCDC.dll`, `CTIntrfu.dll`, and Creative application assemblies are not linked or loaded.

## Cross-build verification

A freestanding Windows ARM64 PE was cross-linked from the Stage A source for runtime testing.

Verified PE machine:

- `IMAGE_FILE_MACHINE_ARM64`
- machine value `0xAA64`

The generated test executable is intentionally not committed as source-of-truth code. Source remains under `src/asio-engine-stage-a/`.

## Next evidence required

Run the ARM64 Stage A executable on the physical X4 and upload:

`x4-asio-engine-stage-a.txt`

Do not extend to `IAsio` until this native implementation passes the Stage A invariants on hardware.
