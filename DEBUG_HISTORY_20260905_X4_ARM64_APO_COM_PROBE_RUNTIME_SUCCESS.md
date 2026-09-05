# DEBUG HISTORY — 2026-09-05 X4 ARM64 APO Offline COM Probe Runtime Success

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This runtime test validates the native ARM64 Stage A0 APO DLL as an isolated COM/APO module without registering the DLL, touching the X4 endpoint, changing registry state, calling APO Initialize, or starting AudioDG graph processing.

Probe:

`src/x4-apo-com-probe-arm64/X4ApoComProbeArm64.exe`

APO DLL:

`X4ApoArm64.dll`

Runner:

`RUN-COM-PROBE.cmd`

## Runtime result

Overall result:

`RESULT: PASS`

### SFX

- `DllGetClassObject(IClassFactory)` -> `0x00000000` PASS
- `IClassFactory::CreateInstance(IAudioProcessingObject)` -> `0x00000000` PASS
- `QI IAudioProcessingObject` -> PASS
- `QI IAudioProcessingObjectRT` -> PASS
- `QI IAudioProcessingObjectConfiguration` -> PASS
- `QI IAudioSystemEffects` -> PASS
- `QI IAudioSystemEffects2` -> PASS
- `QI IAudioSystemEffects3` -> PASS
- `QI IAudioProcessingObjectNotifications` -> PASS

### MFX

The same class-factory, CreateInstance and seven QueryInterface checks all returned `0x00000000` PASS.

### EFX

The same class-factory, CreateInstance and seven QueryInterface checks all returned `0x00000000` PASS.

### COM lifetime cleanup

After all class factories and APO objects were released:

- `DllCanUnloadNow` -> `0x00000000` / `S_OK` PASS

This confirms that the test left no outstanding ATL COM object/module lock in the isolated probe process.

## What this proves

This runtime result proves that on the tested Windows ARM64 machine:

1. the ARM64 process can load `X4ApoArm64.dll`;
2. the DLL exports a callable `DllGetClassObject`;
3. all three official SB1815 Creative APO CLSIDs resolve to working ATL class factories;
4. each class factory creates an APO object successfully without aggregation;
5. each object exposes the required Stage A0 interface set;
6. object/factory lifetime accounting returns the DLL to unloadable state.

Combined with the prior binary inspection, the Stage A0 DLL has passed both:

- ARM64 PE/static binary validation;
- isolated ARM64 COM/APO object runtime validation.

## What this does NOT prove

This test deliberately does not prove:

- Windows APO package installation;
- COM registration through an INF package;
- endpoint SFX/MFX/EFX binding;
- `APOInitSystemEffects3` behavior inside a real AudioDG graph;
- `LockForProcess` / `UnlockForProcess` runtime behavior;
- `APOProcess` execution inside AudioDG;
- actual speaker/headphone/microphone audio pass-through;
- Creative FX context/property-store access;
- Creative Platform discovery;
- any Creative DSP algorithm.

Those remain later gates and must not be inferred from this isolated COM result.

## Stage A0 gate status

The Stage A0 native binary/class architecture gate is now **PASS**.

The next allowed step is a non-installing review of Windows 11 componentized APO packaging. Live endpoint binding remains blocked until that package is reviewed separately.

## Safety

The successful probe performed no:

- registry write;
- endpoint property write;
- APO registration;
- X4 CTCDC command;
- driver installation;
- AudioDG restart;
- B5 ASIO modification.
