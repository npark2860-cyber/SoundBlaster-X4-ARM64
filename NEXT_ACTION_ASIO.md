# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Current status

The following layers are now independently hardware/runtime confirmed on the Sound Blaster X4:

1. native ARM64 KS/WaveRT one-stream baseline
2. active-playback collision mechanism
3. Creative-equivalent pin-instance coexistence gate
4. native ARM64 ASIO COM Stage B0 ABI shell
5. Stage B1 COM-integrated BUSY preflight path

Do not intentionally reproduce the known green-screen collision.

## Fixed render baseline

Known-good render shape remains frozen:

- native Windows ARM64
- X4 `msft_wave`
- Render Pin 1
- 48 kHz
- stereo
- 16-bit PCM / `WAVE_FORMAT_EXTENSIBLE`
- 4096-byte WaveRT notification buffer
- notification count = 2
- 20/20 notifications
- packet discontinuities = 0
- presentation-position regressions = 0
- clean unregister / STOP / close

SDK baseline branch:

`exp/windows-arm64-asio-sdk-abi-baseline`

Baseline HEAD:

`a02be3c7ffb4dc66c7eb903712a8b4301efe8ea7`

## Coexistence gate — hardware confirmed

Creative static analysis and ARM64 hardware tests agree on the useful first product rule:

- Render Pin 1 has `PossibleCount=1`
- idle: `GLOBALCINSTANCES CurrentCount=0` → FREE
- normal Windows X4 playback active: `GLOBALCINSTANCES CurrentCount=1` → BUSY
- while BUSY, do not call `KsCreatePin`

The ARM64 gate experiment proved that BUSY can be returned cleanly without triggering the known `usbaudio2` / WDF stale-pipe crash.

Validated gate branch HEAD:

`362d58372b58640ac666dd59f17e532b092c05d3`

Do not add Creative-style `TakeExclusiveControl` arbitration yet.

## Stage B0 COM shell — runtime PASS

Branch:

`exp/windows-arm64-asio-com-stage-b0`

Validated HEAD:

`53a1854167447338ca45606b6de2181ae6d8148d`

Runtime smoke result:

```text
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
init=1
driverName=Sound Blaster X4 ARM64
driverVersion=100
getChannels=0 inputs=0 outputs=2
getBufferSize=0 min=512 max=512 preferred=512 granularity=0
getSampleRate=0 rate=48000.0
start=-997
DllCanUnloadNow hr=0x00000000
STAGE B0 COM SMOKE RESULT: PASS
```

See:

`DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B0_RUNTIME_SUCCESS.md`

## Stage B1 — COM-integrated read-only coexistence preflight

Branch:

`exp/windows-arm64-asio-com-stage-b1-preflight`

Implementation HEAD:

`9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

Stage B1 changes only the COM `init()` ownership preflight path.

On `init()` it:

1. discovers X4 `msft_wave`
2. opens the KS filter
3. queries Render Pin 1 `KSPROPERTY_PIN_CINSTANCES`
4. queries Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES`
5. immediately closes the filter
6. returns `ASIOTrue` only when both queries succeed and neither count is saturated
7. returns `ASIOFalse` on BUSY or INDETERMINATE

Stage B1 still does **not**:

- call `KsCreatePin`
- allocate a WaveRT buffer
- register notification events
- change KS state
- write audio
- connect `createBuffers()` / `start()` to the render engine

`start()` remains `ASE_InvalidMode` when preflight is FREE.

## Stage B1 BUSY path — hardware PASS

With normal Windows playback actively using X4, the registry-free Stage B1 smoke produced:

```text
init=0
preflightState=BUSY
errorMessage=Stage B1 preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
driverName=Sound Blaster X4 ARM64
driverVersion=101
getChannels=0 inputs=0 outputs=2
getBufferSize=0 min=512 max=512 preferred=512 granularity=0
getSampleRate=0 rate=48000.0
start=SKIPPED because init did not report FREE
DllCanUnloadNow hr=0x00000000
STAGE B1 COM PREFLIGHT RESULT: PASS (BUSY SAFELY BLOCKED)
```

Hardware-established consequences:

- `GLOBALCINSTANCES` was `1/1`
- COM `init()` failed cleanly before any render pin creation
- `KsCreatePin` was skipped
- the smoke harness did not call `start()`
- COM lifetime/unload completed cleanly
- no green-screen / WDF crash occurred

See:

`DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B1_BUSY_RUNTIME_SUCCESS.md`

## Immediate next action — Stage B1 FREE path only

Do **not** change code yet.

Stop all X4 playback, then run the exact same already-built executable once:

```bat
x4-asio-stage-b1-smoke.exe
```

Required FREE-path semantics:

```text
init=1
preflightState=FREE
errorMessage=Stage B1 preflight FREE: C 0/1 G 0/1; ...
start=-997
DllCanUnloadNow hr=0x00000000
STAGE B1 COM PREFLIGHT RESULT: PASS (FREE)
```

The wording after `FREE:` may vary slightly. The important requirements are:

- local/global count both `0/1`
- `init()` succeeds
- no `KsCreatePin`
- no WaveRT
- `start()` remains disabled with `ASE_InvalidMode`
- clean COM unload

## After both Stage B1 branches pass — Stage B2

Only then connect the fixed one-stream WaveRT render engine behind the ASIO object.

B2 must change one runtime variable at a time:

1. keep 48 kHz / stereo / 16-bit / Render Pin 1
2. keep 4096-byte buffer / notification count 2
3. re-query the coexistence gate immediately before the real `KsCreatePin`
4. connect only the minimal one-stream engine lifecycle behind the COM object
5. preserve the proven WaveRT state order and cleanup
6. return clean BUSY without pin creation whenever capacity is exhausted
7. keep the first B2 test registry-free; no DAW yet

## Scope still frozen

Do not add or test yet:

- capture
- 24-bit ASIO transport
- multichannel
- sample-rate expansion
- dynamic buffer-size expansion
- repeated reopen stress
- Creative runtime dependencies
- custom kernel driver

## Architectural rule

Final architecture remains:

native ARM64 DAW
→ independent native ARM64 ASIO COM DLL
→ SetupAPI / `KsCreatePin` / WaveRT
→ Microsoft `usbaudio2.sys`
→ Sound Blaster X4

Creative binaries remain reference-only and must not be loaded or redistributed as runtime dependencies.
