# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Hardware/runtime-confirmed layers

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. ASIO COM Stage B1 coexistence preflight in both FREE and BUSY states
6. ASIO COM Stage B2 BUSY refusal after fixed WaveRT engine integration

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

## Stage B2 fixed WaveRT behind COM — implemented

Branch:

`exp/windows-arm64-asio-com-stage-b2-wavert`

Current validated source HEAD:

`a6d3201260056a46ae8bce57271132871904d6ee`

B2 keeps the B1 `init()` preflight and adds:

- fixed internal WaveRT render engine
- second CINSTANCES/GLOBALCINSTANCES gate immediately before real `KsCreatePin`
- fixed 48k / stereo / 16-bit Render Pin 1 format
- fixed 4096-byte notification buffer
- notification count 2
- synchronous 20-notification diagnostic RUN
- proven WaveRT state ordering and cleanup

B2 remains registry-free and is not yet DAW-ready ASIO. It does not yet provide real ASIO host double-buffer callback delivery.

## Stage B2 BUSY path — hardware PASS

Observed runtime:

```text
init=0
initMessage=Stage B2 init preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
driverName=Sound Blaster X4 ARM64
driverVersion=102
createBuffers/start/stop=SKIPPED because init did not report FREE
DllCanUnloadNow hr=0x00000000
STAGE B2 COM/WAVERT RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

This confirms that integrating the fixed WaveRT engine did not regress the B1 coexistence refusal path.

See:

`DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B2_BUSY_RUNTIME_SUCCESS.md`

## Immediate next action — Stage B2 FREE lifecycle only

Do **not** change code yet.

Stop every X4 playback source and run the same already-built executable:

```bat
x4-asio-stage-b2-smoke.exe
```

Required initial state:

```text
init=1
```

The smoke may then proceed through:

1. diagnostic `createBuffers()` preparation
2. second instance gate immediately before real `KsCreatePin`
3. Render Pin 1 creation
4. 4096-byte WaveRT buffer acquisition
5. notification event registration
6. `ACQUIRE -> PAUSE -> RUN`
7. exactly 20 notification observations
8. `RUN -> PAUSE -> ACQUIRE -> STOP`
9. unregister event
10. clean pin/filter close and COM unload

Required final result:

```text
STAGE B2 COM/WAVERT RESULT: PASS (FREE LIFECYCLE)
```

If `init()` still reports BUSY, do not bypass it. Find/stop the application or system stream holding X4 and rerun later.

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
