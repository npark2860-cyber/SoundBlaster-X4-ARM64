# X4 ARM64 APO Windows 11 packaging review

**REVIEW ONLY — DO NOT INSTALL**

This directory is deliberately non-installable. Files use the `.inx.review` suffix so they cannot be accidentally submitted to PnPUtil as INF files.

## Purpose

Stage A0 has already passed:

- native ARM64 PE validation;
- `LoadLibraryW` runtime load;
- `DllGetClassObject` for the official SB1815 SFX/MFX/EFX CLSIDs;
- `IClassFactory::CreateInstance`;
- QueryInterface for all Stage A0 APO/system-effects interfaces;
- clean `DllCanUnloadNow` after releases.

The next task is to review the Windows 11 deployment model before any endpoint/registry change.

## Files

- `X4ApoComponent.inx.review`
  - models a Windows 11 `Class=AudioProcessingObject` software-component package;
  - registers `X4ApoArm64.dll` for the three official Creative X4 CLSIDs;
  - models `AudioEngine\AudioProcessingObjects` metadata;
  - uses an intentionally unresolved software-component identity.

- `X4ApoExtension.inx.review`
  - models an Extension package matching `USB\VID_041E&PID_3278&MI_03`;
  - associates the future APO software component;
  - records the recovered SB1815 Speaker/Headphone/Microphone SFX/MFX/EFX payload;
  - intentionally does not include a valid ExtensionId/catalog or a live-ready component ID.

## Deliberately unresolved before conversion to real INF

1. final software-component HWID/ComponentID;
2. final ExtensionId;
3. catalog/signing identity;
4. whether the extension should target the base X4 devnode only or selected generated interfaces on the actual ARM64 `usbaudio2` stack;
5. exact treatment of the official Creative per-context `Default`, `Volatile`, and `User` property trees;
6. EffectNodeInfo/APO product-identity compatibility needed by Creative Platform;
7. whether any Speaker/Headphone/Microphone path actually needs semantics from the x64-only Creative UpperFilter.

## Safety boundary

Do not rename these files to `.inf`, run PnPUtil, write FX registry values manually, or register the DLL with regsvr32.

The first live package must remain pass-through only and must exclude SPDIF/DDL, CTUSBWrap, CTUSBDGFX and Creative DSP algorithms.
