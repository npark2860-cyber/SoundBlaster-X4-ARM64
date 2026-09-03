# DEBUG HISTORY — ASIO COM Stage B1 runtime success

Date: 2026-09-04 KST

## Scope

Stage B1 integrated the already hardware-proven Render Pin 1 instance-capacity preflight into the native ARM64 ASIO COM object's `init()` path only.

It remained registry-free and did not call `KsCreatePin`, allocate WaveRT, change KS state, or perform audio I/O.

Branch:

`exp/windows-arm64-asio-com-stage-b1-preflight`

Validated implementation HEAD:

`9a27ea1e4092d264d6472c40183cdb61e7ad9e3c`

## Active Windows X4 playback — BUSY path PASS

Observed runtime result:

```text
init=0
preflightState=BUSY
errorMessage=Stage B1 preflight BUSY: C 0/1 G 1/1; KsCreatePin SKIPPED
start=SKIPPED because init did not report FREE
STAGE B1 COM PREFLIGHT RESULT: PASS (BUSY SAFELY BLOCKED)
```

This proves the COM-integrated preflight blocks the known collision condition before any KS pin creation.

## Idle — FREE path PASS

Observed runtime result:

```text
init=1
preflightState=FREE
errorMessage=Stage B1 preflight FREE: C 0/1 G 0/1; streaming not connected
start=-997 (Stage B1 expected ASE_InvalidMode=-997; streaming still disconnected)
STAGE B1 COM PREFLIGHT RESULT: PASS (FREE)
```

This proves the same COM path remains available when the single global Render Pin 1 instance is not consumed.

## Result

Stage B1 is closed as PASS for both ownership states:

- idle: FREE / `init=1`
- active Windows X4 playback: BUSY / `init=0` / no `KsCreatePin`

The next stage may connect the fixed, already-proven 48 kHz / stereo / 16-bit WaveRT render lifecycle behind the COM object, while rechecking instance capacity immediately before the real `KsCreatePin`.
