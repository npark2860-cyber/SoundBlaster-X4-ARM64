# NEXT ACTION — Native ARM64 ASIO

Updated: 2026-09-04 KST

## Current status

Native ARM64 ASIO feasibility and the first render coexistence safety gate are now hardware-confirmed on the Sound Blaster X4.

Known-good fixed render baseline:

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

## Generic ABI investigation — closed

The newest controlled active-playback dump matched the already-established kernel fingerprint:

- `WDF_VIOLATION 0x10D`
- Parameter 1 = `5`
- `usbaudio2` recovery path
- stale/destroyed WDF USB pipe usage

Therefore a generic native ARM64 ABI / Windows SDK layout defect is no longer the primary explanation.

Do not intentionally reproduce that green-screen condition again.

## Render coexistence root-cause differential — hardware confirmed

Static analysis of reference-only Creative `CtU2As64.DLL` recovered a concrete pre-`KsCreatePin` ownership policy:

1. query `KSPROPERTY_PIN_CINSTANCES`
2. query `KSPROPERTY_PIN_GLOBALCINSTANCES`
3. treat `CurrentCount >= PossibleCount` as busy
4. when configured, attempt Creative's `TakeExclusiveControl` endpoint arbitration
5. re-query instance availability
6. do not proceed through the observed `KsCreatePin` path while capacity remains exhausted

On the user's X4 Render Pin 1:

### Idle

- `CINSTANCES`: `0 / 1`
- `GLOBALCINSTANCES`: `0 / 1`
- result: FREE

### Normal Windows X4 playback active

- `CINSTANCES`: `0 / 1`
- `GLOBALCINSTANCES`: `1 / 1`
- result: BUSY

Therefore Windows shared playback consumes the single global Render Pin 1 instance even though the local instance count remains zero.

## Native ARM64 GLOBALCINSTANCES gate — hardware confirmed

Experiment branch:

`exp/windows-arm64-asio-global-instance-gate`

Validated HEAD:

`362d58372b58640ac666dd59f17e532b092c05d3`

The only runtime safety change is a fail-closed `KSPROPERTY_PIN_GLOBALCINSTANCES` query immediately before the real `KsCreatePin`.

### Idle result

```text
GLOBAL INSTANCE GATE: PinId=1 PossibleCount=1 CurrentCount=0 busy=NO
GLOBAL INSTANCE GATE: FREE -> calling real KsCreatePin
```

The unchanged fixed WaveRT lifecycle then completed 20/20 and PASSed.

### Active Windows playback result

```text
GLOBAL INSTANCE GATE: PinId=1 PossibleCount=1 CurrentCount=1 busy=YES
GLOBAL INSTANCE GATE: BUSY -> KsCreatePin SKIPPED
KsCreatePin failed status=0x000000AA
```

The process exited normally with no green screen.

`0xAA` is intentional `ERROR_BUSY` from the experiment wrapper.

This establishes the first hardware-confirmed safe coexistence behavior for the native ARM64 render path:

**when global render capacity is exhausted, return a clean busy/coexistence failure and do not instantiate the KS pin.**

Do not add Creative-style WASAPI exclusive arbitration yet. The fail-closed gate is sufficient for the first product path.

See:

- `DEBUG_HISTORY_20260904_ASIO_CREATIVE_EXCLUSIVE_PREFLIGHT_STATIC.md`
- `DEBUG_HISTORY_20260904_ASIO_GLOBAL_INSTANCE_GATE_ACTIVE_SUCCESS.md`

## Stage B0 — native ARM64 ASIO COM shell implemented

ASIO COM productization may now resume.

Branch:

`exp/windows-arm64-asio-com-stage-b0`

Implementation HEAD:

`d766bc70cbb3d9abfc9ac265ee577b5185354103`

Independent project CLSID:

`{0AA6D99C-4AF6-45EF-9CCA-10AC9239B7D4}`

Creative's CLSID is not reused.

Stage B0 provides:

- native ARM64 in-process COM DLL
- `IClassFactory`
- ASIO-compatible vtable derived from `IUnknown`
- `DllCanUnloadNow`
- `DllGetClassObject`
- `DllRegisterServer`
- `DllUnregisterServer`
- fixed metadata/capabilities only:
  - 0 inputs
  - 2 outputs
  - 48 kHz
  - 512-frame fixed buffer

Stage B0 deliberately does **not** connect the WaveRT engine:

- `start()` returns `ASE_InvalidMode`
- `createBuffers()` returns `ASE_InvalidMode`
- no X4 filter open
- no `KsCreatePin`
- no WaveRT buffer
- no hardware I/O

See:

`DEBUG_HISTORY_20260904_ASIO_COM_STAGE_B0_IMPLEMENTED.md`

## Immediate next action — Stage B0 registry-free COM smoke

Run the manual GitHub Actions workflow:

`Build ASIO COM Stage B0 ARM64`

It builds and packages:

- `x4-asio-arm64.dll`
- `x4-asio-stage-b0-smoke.exe`

First test only the registry-free smoke executable with both files in the same directory:

```bat
x4-asio-stage-b0-smoke.exe
```

Expected final line:

```text
STAGE B0 COM SMOKE RESULT: PASS
```

This test is safe with respect to the X4 because it performs no hardware I/O.

### Do not do yet

Until the registry-free COM smoke passes:

- do not call `DllRegisterServer`
- do not use `regsvr32`
- do not test in a real DAW
- do not connect WaveRT to the COM object

## After Stage B0 smoke passes — Stage B1

Integrate the already-proven fixed render engine behind the ASIO object one variable at a time.

First B1 scope:

1. keep 48 kHz / stereo / 16-bit / Render Pin 1 only
2. keep fixed 4096-byte WaveRT buffer / two 512-frame halves
3. query `GLOBALCINSTANCES` immediately before every render `KsCreatePin`
4. if busy, return a clean ASIO device-busy/coexistence failure without pin creation
5. connect only the minimum `init` / `createBuffers` / `start` / `stop` lifecycle necessary for one render stream
6. preserve known-good WaveRT state ordering and cleanup

Do not add Creative `TakeExclusiveControl` arbitration in this stage.

## Scope still frozen

Do not add or test yet:

- capture
- 24-bit ASIO transport
- multichannel ASIO buffers
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

Creative binaries remain reference-only and must not be loaded or redistributed as final runtime dependencies.
