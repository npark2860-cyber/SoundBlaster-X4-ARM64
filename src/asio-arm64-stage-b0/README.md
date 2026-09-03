# Sound Blaster X4 native ARM64 ASIO — Stage B1 preflight

Stage B1 adds the already hardware-proven KS pin-instance coexistence preflight to the Stage B0 native ARM64 ASIO COM shell.

## Safety scope

On `IASIO::init()`, Stage B1 only:

1. discovers the X4 `msft_wave` filter
2. opens the filter
3. queries Render Pin 1 `KSPROPERTY_PIN_CINSTANCES`
4. queries Render Pin 1 `KSPROPERTY_PIN_GLOBALCINSTANCES`
5. closes the filter immediately

Stage B1 does **not**:

- call `KsCreatePin`
- allocate a WaveRT buffer
- register a notification event
- change KS state
- write audio data
- invoke the streaming engine
- register itself unless `DllRegisterServer` is explicitly invoked

The supplied smoke executable remains registry-free and loads the DLL directly.

## Independent CLSID

`{0AA6D99C-4AF6-45EF-9CCA-10AC9239B7D4}`

Creative's CLSID is deliberately not reused.

## FREE behavior

With no other X4 render stream active, the hardware-proven expectation is:

- `CINSTANCES CurrentCount=0 / PossibleCount=1`
- `GLOBALCINSTANCES CurrentCount=0 / PossibleCount=1`
- `init=1`
- error text begins `Stage B1 preflight FREE:`
- `start()` still returns `ASE_InvalidMode (-997)` because streaming is not connected yet

Expected final line:

```text
STAGE B1 COM PREFLIGHT RESULT: PASS (FREE)
```

## BUSY behavior

With normal Windows playback actively using the X4, the previously proven expectation is:

- `GLOBALCINSTANCES CurrentCount=1 / PossibleCount=1`
- `init=0`
- error text begins `Stage B1 preflight BUSY:`
- message includes `KsCreatePin SKIPPED`
- the smoke harness does not call `start()`

Expected final line:

```text
STAGE B1 COM PREFLIGHT RESULT: PASS (BUSY SAFELY BLOCKED)
```

A query failure is treated fail-closed as `INDETERMINATE` and returns `init=0`.

## Files

Run from a directory containing:

```text
x4-asio-arm64.dll
x4-asio-stage-b1-smoke.exe
```

Then:

```bat
x4-asio-stage-b1-smoke.exe
```

## Test order

1. Run once with X4 playback idle.
2. Only after idle reports `PASS (FREE)`, run once while normal Windows X4 playback is active.

Both tests remain safe because Stage B1 never calls `KsCreatePin`.
