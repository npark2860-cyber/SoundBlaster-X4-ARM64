# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in both FREE and BUSY states

Do not intentionally reproduce the known green-screen collision.

## Frozen render baseline

Keep unchanged until later expansion:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- notification count = 2
- zero buffer once before RUN
- 20 notifications for registry-free smoke
- no writes during RUN
- `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`
- unregister notification event before closing handles

Known-good SDK baseline:

`exp/windows-arm64-asio-sdk-abi-baseline@a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

## Coexistence rule — hardware confirmed

Render Pin 1 has one global instance.

- idle: C `0/1`, G `0/1` -> FREE
- Windows X4 playback active: C `0/1`, G `1/1` -> BUSY

Product safety rule:

**If either instance query is indeterminate or saturated, do not call `KsCreatePin`.**

A second gate must run immediately before every real render `KsCreatePin`, not only at COM `init()`.

Do not add Creative `TakeExclusiveControl` arbitration yet.

## Stage B0 — PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b0@53a1854167447338ca45606b6de2181ae6d8148d`

Registry-free COM class-factory / vtable / unload smoke PASSed.

## Stage B1 — FREE + BUSY PASS

Validated source:

`exp/windows-arm64-asio-com-stage-b1-preflight@9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

Idle:

```text
init=1
preflightState=FREE
errorMessage=Stage B1 preflight FREE: C 0/1 G 0/1; streaming not connected
STAGE B1 COM PREFLIGHT RESULT: PASS (FREE)
```

Active Windows X4 playback:

```text
init=0
preflightState=BUSY
errorMessage=Stage B1 preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
start=SKIPPED because init did not report FREE
STAGE B1 COM PREFLIGHT RESULT: PASS (BUSY SAFELY BLOCKED)
```

See `DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B1_RUNTIME_SUCCESS.md`.

## Immediate next stage — B2 fixed WaveRT lifecycle behind COM

Create a new branch from the validated B1 source.

B2 is still a registry-free diagnostic stage, not DAW-ready ASIO.

Change only these runtime behaviors:

1. keep B1 `init()` preflight unchanged
2. add a fixed internal WaveRT render object
3. use `createBuffers()` only as a Stage-B2 diagnostic preparation call for the known-good fixed pin/buffer/event geometry
4. immediately before real `KsCreatePin`, query CINSTANCES and GLOBALCINSTANCES again and fail closed on BUSY/INDETERMINATE
5. `start()` performs the proven `ACQUIRE -> PAUSE -> RUN` sequence and observes exactly 20 notifications synchronously for the registry-free smoke only
6. `stop()` performs `RUN -> PAUSE -> ACQUIRE -> STOP`
7. `disposeBuffers()` unregisters the event and closes event/pin/filter cleanly
8. no callback thread, DAW callback, or buffer writes during RUN yet

The B2 smoke must be run idle first. Do not deliberately bypass the B1 BUSY gate.

## Still frozen

Do not add yet:

- DAW registration/testing
- real ASIO double-buffer callback delivery
- capture
- 24-bit transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

## Architecture

native ARM64 DAW
-> independent native ARM64 ASIO COM DLL
-> SetupAPI / `KsCreatePin` / WaveRT
-> Microsoft `usbaudio2.sys`
-> Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
