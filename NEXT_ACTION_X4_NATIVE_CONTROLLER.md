# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Stage A0 status — binary + offline COM gates PASSED

Validated `X4ApoArm64.dll`:

- size `176640` bytes
- SHA-256 `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / machine `0xAA64`
- exports `DllCanUnloadNow`, `DllGetClassObject`
- no `DllRegisterServer`
- official X4 SFX/MFX/EFX CLSIDs present
- AVRT sections `RT_CODE`, `RT_CONST`, `RT_DATA` present
- no Creative x64 DLL imports

Offline native ARM64 COM probe result:

`RESULT: PASS`

For SFX, MFX and EFX independently:

- `DllGetClassObject(IClassFactory)` -> `S_OK`
- `IClassFactory::CreateInstance(IAudioProcessingObject)` -> `S_OK`
- QI `IAudioProcessingObject` -> PASS
- QI `IAudioProcessingObjectRT` -> PASS
- QI `IAudioProcessingObjectConfiguration` -> PASS
- QI `IAudioSystemEffects` -> PASS
- QI `IAudioSystemEffects2` -> PASS
- QI `IAudioSystemEffects3` -> PASS
- QI `IAudioProcessingObjectNotifications` -> PASS

After releases:

- `DllCanUnloadNow` -> `S_OK`

Canonical traces:

- `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
- `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`

This closes the isolated native ARM64 binary/class-factory/interface gate.

## Current gate — package attachment review, still NON-INSTALLING

Review directory:

`packaging/x4-apo-arm64-review`

Files:

- `README.md`
- `X4ApoComponent.inx.review`
- `X4ApoExtension.inx.review`

The component review follows Microsoft's Windows 11 `Class=AudioProcessingObject` model and records:

- ARM64-only target;
- `X4ApoArm64.dll` DriverStore copy;
- COM registration for official X4 SFX/MFX/EFX CLSIDs;
- `AudioEngine\AudioProcessingObjects` registration;
- `IAudioProcessingObject` primary interface;
- Stage A0 1-in/1-out/default APO metadata.

The extension review records:

- X4 HWID `USB\VID_041E&PID_3278&MI_03`;
- future `AddComponent` association;
- official SB1815 Win11 FX payload for:
  - `FX\0` Speaker
  - `FX\1` Headphone
  - `FX\3` Microphone
- SFX/MFX/EFX all restricted to `AUDIO_SIGNALPROCESSINGMODE_DEFAULT` as in the official Creative INF.

The FX payload section is deliberately **not referenced by the install section**. Component ID, ExtensionId and catalogs remain placeholders. The files use `.inx.review` and must not be renamed or installed yet.

## Immediate priority 1 — read-only ARM64 usbaudio2 attachment discovery

Before producing a real INF, determine the actual live device/interface layout of the Microsoft `usbaudio2` stack for the X4 audio interface.

Need exact read-only evidence for:

1. devnode instance for `USB\VID_041E&PID_3278&MI_03`;
2. generated audio/topology interfaces and reference strings;
3. KS pin categories / endpoint associations corresponding to Speaker, Headphone and Microphone;
4. current `FX\*` / `EP\*` property presence, if any;
5. whether the X4 endpoint builder already preserves official slot numbering (`FX\0`, `FX\1`, `FX\3`) under the Microsoft class driver;
6. exact `PKEY_FX_Association` values needed for those paths.

This discovery must remain read-only.

## Immediate priority 2 — finalize pass-through test package only after attachment is proven

Once the target attachment point is exact:

1. choose the final software-component identity;
2. generate a real unique ExtensionId;
3. replace only the review placeholders;
4. wire the already-reviewed FX payload into the exact target install/interface section;
5. run INF verification/build/signing checks;
6. keep the package pass-through only.

The first live package must still contain:

- no Creative DSP algorithms;
- no Creative FX property writes;
- no CTCDC writes;
- no SPDIF/DDL;
- no CTUSBWrap/DGFX;
- no Creative UpperFilter replacement unless separate evidence later proves it is required.

## First live runtime gate after package review

The first installation test is only intended to prove:

`PnP package -> APO registration -> X4 endpoint FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

Success criteria:

- Speaker/Headphone/Microphone remain functional;
- no audio loss or AudioDG crash;
- native ARM64 APO actually loads in the graph;
- pass-through audio remains transparent;
- uninstall/rollback restores the original Microsoft `usbaudio2` state.

Do not enable Creative effect keys yet.

## Later gates

Only after real AudioDG pass-through succeeds:

1. exact general-vs-headphone context selection;
2. read-only `IAudioSystemEffectsPropertyStore` open;
3. property-change notification validation;
4. Creative Platform repository/discovery compatibility;
5. one DSP feature at a time;
6. only then effect setters.

## Fixed architecture constraints

- retain Microsoft USB Audio 2.0 as base driver
- original `CTUSBAPO64.dll` is x86-64 only and is not the native ARM64 AudioDG solution
- Speaker/Headphone/Microphone use the official Creative SFX/MFX/EFX identities
- general FX context `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone FX context `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`
- SPDIF/DDL/CTUSBWrap/DGFX remain a separate later track
- no B5 ASIO changes from this branch

## Safety

- one variable at a time
- Stage A0 remains pass-through/read-only
- no new hardware state changes automatically
- no manual FX registry writes
- no `regsvr32`
- no live APO install while attachment/association is unresolved
- no blind `0x95` probing
- no generic `0x23` probing
- no unrelated changes
