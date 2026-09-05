# DEBUG HISTORY — 2026-09-05 X4 ARM64 APO Stage A0 Build Fix 1

Branch:

`exp/windows-arm64-x4-native-controller`

## First ARM64 build result

Command:

`msbuild src/x4-apo-arm64/X4ApoArm64.vcxproj /m /restore /p:Configuration=Release /p:Platform=ARM64 /v:minimal`

The first compile reached `X4Apo.cpp` / `X4ApoDll.cpp` and failed in ATL with C3246.

Representative error:

`ATL::CComContainedObject<contained>: cannot inherit from 'CX4EfxApo' as it has been declared as 'final'`

The same class pattern existed for SFX, MFX and EFX.

## Root cause

The Stage A0 classes used ATL `CComCoClass` / `OBJECT_ENTRY_AUTO` but were declared `final`.

ATL constructs COM object wrappers such as `CComObject<T>` / aggregation helpers that derive from `T`. A `final` APO class therefore cannot be used as the ATL implementation type.

Microsoft SYSVAD APO classes are likewise non-final.

This is a COM/ATL object-factory compile issue only. It does not change the recovered Creative CLSIDs, APO interface map, DSP behavior, CTCDC behavior or endpoint bindings.

## Fix

Removed only `final` from:

- `CX4SfxApo`
- `CX4MfxApo`
- `CX4EfxApo`

No interface, CLSID, registration property, real-time processing, property-store or hardware-control code was changed.

Fix commit:

`7463d6ef23c1a8344ac208659a3e21c22ae63097`

## Next gate

Re-run the exact same Release/ARM64 MSBuild command.

Do not make speculative additional changes before the next compiler/linker result.

Stage A0 remains pass-through/read-only and is not installable/bound to the live X4 endpoint yet.
