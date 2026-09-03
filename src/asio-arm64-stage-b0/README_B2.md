# Sound Blaster X4 native ARM64 ASIO — Stage B2 fixed WaveRT smoke

Stage B2 connects the already-proven fixed WaveRT render lifecycle behind the independent native ARM64 ASIO COM object.

This is still a registry-free diagnostic stage. It is **not** DAW-ready ASIO.

## Frozen hardware path

- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- notification count = 2
- zero buffer once before RUN
- no writes during RUN
- 20 notifications
- state order: `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`

## Coexistence safety

Stage B2 keeps the B1 `init()` preflight and adds a second fail-closed instance-capacity query immediately before the real `KsCreatePin`.

If CINSTANCES or GLOBALCINSTANCES is indeterminate or saturated, `KsCreatePin` is skipped.

Do not bypass a BUSY result.

## Diagnostic `createBuffers()` contract

Real ASIO double-buffer delivery is deliberately not implemented yet.

For Stage B2 smoke only, the harness calls:

```text
createBuffers(nullptr, 0, 512, nullptr)
```

That prepares the fixed KS pin, WaveRT buffer, and notification event.

## `start()` scope

For Stage B2 smoke only, `start()` performs the proven RUN transition and synchronously observes exactly 20 notifications. It returns `ASE_OK` only if:

- 20/20 notifications arrive
- packet discontinuities = 0
- presentation-position regressions = 0

There is no callback thread and no DAW callback delivery in this stage.

## Test order

Run only the registry-free smoke executable with the DLL beside it:

```bat
x4-asio-stage-b2-smoke.exe
```

### First test: idle only

Stop all normal Windows X4 playback first.

Expected key path:

```text
init=1
B2 PRE-PIN GATE: C 0/1 G 0/1 busy=NO
createBuffers=0
B2 KSSTATE 1 -> OK
B2 KSSTATE 2 -> OK
B2 KSSTATE 3 -> OK
... 20 notifications ...
start=0
startMessage=Stage B2 RUN observed 20/20 notifications; packetDiscontinuities=0 positionRegressions=0
B2 KSSTATE 2 -> OK
B2 KSSTATE 1 -> OK
B2 KSSTATE 0 -> OK
stop=0
disposeBuffers=0
DllCanUnloadNow hr=0x00000000
STAGE B2 COM/WAVERT RESULT: PASS (FIXED WAVERT LIFECYCLE)
```

### BUSY behavior

If normal Windows playback is active, B1-style init should reject it before any pin creation:

```text
init=0
... BUSY ... KsCreatePin SKIPPED
STAGE B2 COM/WAVERT RESULT: PASS (BUSY SAFELY BLOCKED AT INIT)
```

Do not deliberately try to bypass the BUSY gate.

## Do not do yet

- do not register with `regsvr32`
- do not test in a DAW
- do not add callback threads
- do not write audio during RUN
- do not add capture, 24-bit, multichannel, sample-rate expansion, or dynamic buffer sizes
