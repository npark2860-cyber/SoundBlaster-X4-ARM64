# CURRENT HANDOFF — Sound Blaster X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Always verify actual branch HEAD before work. Keep this branch separate from B5 ASIO; do not modify B5 ASIO source, WaveRT engine, mux, failsafe or control-panel behavior here.

## Read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`
3. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
4. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
5. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
6. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
7. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
8. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
9. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
10. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
11. `X4_CONTROL_MAP.md`
12. `packaging/x4-apo-arm64-review/README.md`

## Architecture boundary

Keep the four backend classes distinct:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not collapse them into one generic Creative command path.

## Firmware / mixer facts that remain fixed

- X4 CTCDC interface: `USB\VID_041E&PID_3278&MI_01`
- validated firmware: `1.9.251008.0930`
- Direct Mode hardware-confirmed:
  - ON `5A 39 03 00 05 01`
  - OFF `5A 39 03 00 05 00`
- raw PlaybackManager `0x96` exposed Graphic EQ only in the tested session
- raw VoiceInputManager `0x95` no-response is **not** global CrystalVoice unsupported proof
- generic raw `0x23` probing must not be repeated; official `0x23` is the Game/Voice path
- CDC UInt16 engineering-unit conversion remains unresolved; `/256` is not confirmed

Normal Windows mixer ARM64 implementation remains standard Core Audio/DeviceTopology:

- endpoint master/channel -> `IAudioEndpointVolume`
- monitoring volume/mute -> `IDeviceTopology/IPart + IAudioVolumeLevel/IAudioMute`
- Mic Boost -> `KSNODETYPE_VOLUME + IAudioVolumeLevel`
- Mic AGC -> `KSNODETYPE_AGC + IAudioAutoGainControl`

Do not port/load supplied x86 MalLgcy/CTAudEp for this subset.

## Creative APO control plane — recovered

Official Windows path:

`Creative Platform -> IAudioSystemEffectsPropertyStore -> IPropertyStore -> Creative APO`

Property schema:

- bool -> `VT_BOOL`
- float -> `VT_R4`
- float vector -> `VT_VECTOR|VT_R4`
- string -> `VT_LPWSTR`

Product gate:

- APO definition family `{F1056047-B091-4D85-A5C0-B13D4D8BAC57}`
- render PID 0 / capture PID 1
- APO HW identifier `100` -> `SB1815`

Detailed keys/ranges are canonical in `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`.

## SB1815 official Windows 11 APO binding — recovered

Supplied `ctusbaud.inf`:

- SHA-256 `adc7b2128b9d90625efab36c6fc499d8d8f4328e368265f03222cf6720b98b0b`
- X4 audio HWID `USB\VID_041E&PID_3278&MI_03`
- package has x86/amd64 targets only; no `ntarm64`

Primary Creative APO CLSIDs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

Windows 11 X4 bindings:

- Speaker -> Creative SFX/MFX/EFX
- Headphone -> same Creative SFX/MFX/EFX
- Microphone -> same Creative SFX/MFX/EFX
- Line In -> Microsoft effects
- What U Hear -> Creative SFX/MFX
- SPDIF Out -> separate chainer/DGFX/DDL path; keep out of first milestone

All official X4 Speaker/Headphone/Microphone SFX/MFX/EFX streaming-mode lists contain only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT`.

FX property contexts:

- general `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

## ARM64 hosting conclusion

The original `CTUSBAPO64.dll` is plain x86-64 and cannot be the direct in-process payload for native ARM64 AudioDG. The restoration path is an ARM64-native APO while retaining Microsoft USB Audio 2.0 as the base audio driver.

## ARM64 APO Stage A0 — binary gate PASSED

Source:

`src/x4-apo-arm64`

Stage A0 contains native ARM64 pass-through SFX/MFX/EFX classes using the official X4 CLSIDs. It has no Creative DSP, no FX writes, no CTCDC access, no endpoint installation and no SPDIF/DDL.

The first real build initially failed because ATL's object factory creates wrapper/aggregation classes derived from the COM class while SFX/MFX/EFX had been declared `final`. The fix removed only `final` from those three ATL COM classes.

Validated artifact:

- size: `176640` bytes
- SHA-256: `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / machine `0xAA64`
- exports: `DllCanUnloadNow`, `DllGetClassObject`
- `DllRegisterServer` intentionally absent
- exact SFX/MFX/EFX CLSIDs physically present
- linked AVRT sections present: `RT_CODE`, `RT_CONST`, `RT_DATA`
- no Creative x64 DLL imports

Therefore the Stage A0 compile/PE/export/AVRT binary gate passes.

## ARM64 APO Stage A0 — offline COM runtime gate PASSED

Canonical runtime record:

`DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`

Probe:

`src/x4-apo-com-probe-arm64/X4ApoComProbeArm64.exe`

Runtime result on the ARM64 test PC:

`RESULT: PASS`

For each SFX/MFX/EFX CLSID:

- `DllGetClassObject(IClassFactory)` -> `S_OK`
- `IClassFactory::CreateInstance(IAudioProcessingObject)` -> `S_OK`
- QI `IAudioProcessingObject` -> PASS
- QI `IAudioProcessingObjectRT` -> PASS
- QI `IAudioProcessingObjectConfiguration` -> PASS
- QI `IAudioSystemEffects` -> PASS
- QI `IAudioSystemEffects2` -> PASS
- QI `IAudioSystemEffects3` -> PASS
- QI `IAudioProcessingObjectNotifications` -> PASS

After all releases:

- `DllCanUnloadNow` -> `S_OK`

This proves isolated native ARM64 DLL load, class factory creation, APO object creation, interface exposure and clean COM lifetime handling.

It does **not** yet prove INF installation, real AudioDG graph initialization, `LockForProcess`, `APOProcess` inside AudioDG, endpoint binding, Creative property-store access or actual audio pass-through on Speaker/Headphone/Microphone.

## Current gate — Windows 11 package review, still NON-INSTALLING

Review directory:

`packaging/x4-apo-arm64-review`

Files:

- `README.md`
- `X4ApoComponent.inx.review`
- `X4ApoExtension.inx.review`

The component review models Microsoft's current Windows 11 `Class=AudioProcessingObject` package structure and registers the three official X4 APO CLSIDs to `X4ApoArm64.dll` plus `AudioEngine\AudioProcessingObjects` metadata.

The extension review matches the X4 audio HWID and models `AddComponent`, but intentionally uses unresolved placeholder component/catalog/ExtensionId data.

Most importantly, the recovered FX payload section for:

- `FX\0` Speaker
- `FX\1` Headphone
- `FX\3` Microphone

is **not referenced by the install section**. It records the official SFX/MFX/EFX/default-mode data but cannot activate endpoint FX binding as written.

The files remain `.inx.review`; do not rename to `.inf` or install them.

## Immediate next actions

1. Resolve the actual ARM64 Microsoft `usbaudio2` devnode/interface layout used by `USB\VID_041E&PID_3278&MI_03` without writing anything.
2. Determine the exact `PKEY_FX_Association` values/pin categories for Speaker, Headphone and Microphone on that ARM64 stack and compare them to the official SB1815 INF.
3. Decide the final software-component identity and ExtensionId only after the target attachment point is proven.
4. Convert the review package to a real pass-through-only test package only after those checks.
5. First live package test must exclude Creative DSP, property writes, CTCDC writes, SPDIF/DDL, CTUSBWrap and CTUSBDGFX.
6. After real AudioDG pass-through graph loading succeeds, add endpoint-context selection and read-only `IAudioSystemEffectsPropertyStore` access.
7. Add one DSP feature at a time only after the read-only control plane works.

## Safety

- one variable at a time
- no new hardware state changes automatically
- no blind `0x95` probing
- no generic `0x23` probing
- no live APO install while package attachment/association is unresolved
- no manual FX registry writes
- no `regsvr32` for Stage A0
- no unrelated changes
- no B5 ASIO changes from this branch
