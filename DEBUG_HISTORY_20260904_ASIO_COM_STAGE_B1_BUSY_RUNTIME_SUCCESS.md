# DEBUG HISTORY — ASIO COM Stage B1 BUSY preflight runtime success

Date: 2026-09-04 KST

## Scope

This test validates only the BUSY branch of the Stage B1 ASIO COM preflight path on the user's Windows ARM64 Sound Blaster X4 system.

Branch under test:

`exp/windows-arm64-asio-com-stage-b1-preflight`

Implementation HEAD:

`9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

The Stage B1 DLL is still registry-free in this smoke test and does not instantiate a KS render pin.

Safety properties retained:

- no `KsCreatePin`
- no WaveRT buffer allocation
- no KS state changes
- no audio writes
- no Creative runtime dependency

The only hardware access added over Stage B0 is read-only X4 `msft_wave` filter discovery/open plus `KSPROPERTY_PIN_CINSTANCES` / `KSPROPERTY_PIN_GLOBALCINSTANCES` GET queries from `init()`.

## Hardware runtime result — active Windows X4 playback

Observed output:

```text
Sound Blaster X4 ARM64 ASIO Stage B1 COM preflight smoke
SAFETY: registry-free; KS filter GET-only preflight; no KsCreatePin; no WaveRT; no KS state changes.
Loading C:\SB\x4-asio-arm64.dll
DllGetClassObject hr=0x00000000
IClassFactory::CreateInstance hr=0x00000000
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

## Hardware-established facts

With normal Windows X4 playback active:

- local render pin instances: `CurrentCount=0 / PossibleCount=1`
- global render pin instances: `CurrentCount=1 / PossibleCount=1`
- ASIO COM `init()` returned false (`0`)
- Stage B1 reported `BUSY`
- the error message explicitly reported `KsCreatePin SKIPPED`
- the smoke harness did not call `start()` after a failed `init()`
- COM object/factory lifetime completed cleanly and `DllCanUnloadNow` returned `S_OK`
- the process exited normally
- no green-screen / WDF bugcheck occurred

This proves the previously hardware-validated render-capacity safety gate also works when placed inside the native ARM64 ASIO COM object's real `init()` path.

## Interpretation

The safe ASIO ownership behavior is now validated at two levels:

1. standalone guarded `KsCreatePin` baseline
2. native ARM64 ASIO COM `init()` preflight

For the active-playback case, Stage B1 correctly refuses initialization before any render pin creation can occur.

This is the desired fail-closed behavior for the first independent ARM64 ASIO product path.

## Immediate next action

Do not change code yet.

Run the exact same Stage B1 smoke executable once with all X4 playback stopped.

Expected FREE-path indicators:

```text
init=1
preflightState=FREE
errorMessage=Stage B1 preflight FREE: C 0/1 G 0/1; streaming still disabled
start=-997
STAGE B1 COM PREFLIGHT RESULT: PASS (FREE)
```

The exact wording may differ slightly, but the required semantics are:

- local/global capacity is free
- `init()` succeeds
- no `KsCreatePin`
- `start()` remains disabled (`ASE_InvalidMode`)
- clean COM unload

Only after both BUSY and FREE branches pass should Stage B2 connect the already-proven fixed WaveRT render lifecycle behind the COM object.
