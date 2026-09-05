# CURRENT HANDOFF — Sound Blaster X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Always verify the actual branch HEAD before continuing. Do not reconstruct state from conversation memory when repository documents are available.

## Read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
3. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
4. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
5. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
6. `DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`
7. `DEBUG_HISTORY_20260904_X4_READONLY_CAPABILITY_RUNTIME_SUCCESS.md`
8. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
9. `X4_CONTROL_MAP.md`
10. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
11. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

## Scope boundary

This branch is for X4 Windows ARM64 native controller / driver-path analysis.

Keep it separate from B5 ASIO.

Do not modify B5 ASIO source, WaveRT engine, mux, failsafe or control-panel behavior from this branch.

The current ARM64 Windows machine uses the Microsoft USB Audio 2.0 path and does not have the complete configured Creative APO/filter stack. Missing features must be interpreted with that environment in mind.

## Four-layer architecture rule

Classify every feature explicitly:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative filter/APO DSP processing
4. Creative App/profile orchestration

Do not collapse these layers into one generic “Creative command” path.

## CTCDC session — confirmed

X4 control interface:

`USB\VID_041E&PID_3278&MI_01`

Validated session:

- tested COM port: `COM3`
- event mask `0x05`
- 115200 / 8N1
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

Fast gate:

- `5A 03 00` -> `5A 03 02 3B 00`
- maximum payload 59
- firmware `1.9.251008.0930`

Creative App must be fully closed for independent CTCDC runtime tests; otherwise a reproducible ownership/session conflict can block the independent path.

## Direct Mode — hardware-confirmed firmware path

- ON `5A 39 03 00 05 01`
- OFF `5A 39 03 00 05 00`

Physical Windows/X4 state change was confirmed previously.

Do not infer that the separate APO DirectMode property is equivalent to this firmware command without product-specific evidence.

## Malcolm runtime result

### PlaybackManager `0x96`

Only the Graphic EQ block responded through the tested raw route:

- param 9 enable = 1.0
- param 10 preamp = 0.0
- params 11..20 band values = `3, 2, 0, -2, 0, 1, 2, 3, 3, 3.5`

These matched the supplied Music EQ preset.

### Feature mask

`5A 10 00` -> feature mask `0x00000040`, unavailable mask 0.

Recovered mapping identifies `0x40` as GraphicEQ.

### VoiceInputManager `0x95`

Params 0..45 returned no response in the tested CTCDC session.

Critical correction from later APO static analysis:

**this does not prove CrystalVoice is unsupported.**

The official Windows Creative stack has a separate Audio System Effects property-store + APO implementation for AEC, Noise Reduction, MicBeam, SVM, VoiceFX and related processing.

Do not repeat blind raw `0x95` probing.

## AudioControl firmware discovery

Runtime `0x21` returned:

| Index | Type |
|---:|---|
| 0 | Speaker |
| 1 | Headphone |
| 2 | SPDIF Output |
| 3 | Mic Monitoring |
| 4 | Line Monitoring |
| 5 | Mic Input |
| 6 | Line Input |
| 7 | What U Hear Recording |
| 8 | SPDIF Monitoring |
| 9 | SPDIF Input |
| 10 | Automatic Gain Control |

`0x22` returned ranges for 0..9.

`0x24` Mute GET succeeded for 0..10.

The exact raw UInt16 engineering-unit conversion for `0x22/0x23` remains unresolved. Do not hard-code `/256` as confirmed.

## `0x23` AudioLevel — static path resolved

Full trace:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

Exact GET:

`5A 23 02 01 <index>`

Managed response `RawResAudioLevelGet`, Pack=1:

- AudioControlIndex byte
- CurValue UInt16

Managed payload size = 3 bytes.

The runtime fourth trailing `0x03` is not a managed struct member/padding and is ignored by the official managed decode. Its firmware meaning remains unresolved.

Creative Platform creates `0x23` command keys only for Game/Voice indices selected from `GameAudioLevel (19)` / `ChatAudioLevel (18)` descriptors.

The X4 runtime descriptor set contains neither type 18 nor 19.

Therefore:

- generic 0..9 raw `0x23` probing is not the official normal mixer model;
- idx2..9 `GeneralFailure` is not missing-volume proof;
- do not repeat generic `0x23` probing;
- do not issue `0x23` SET.

## Normal Windows mixer — ARM64 implementation path recovered

Full traces:

- `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
- `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`

Original chain:

`Creative.Platform.Mixer.dll`
-> x86 `MalLgcy.dll`
-> x86 `CTAudEp.dll`
-> Microsoft Core Audio / DeviceTopology

MalLgcy is a thin forwarder. CTAudEp's relevant mixer functions resolve to public Windows COM interfaces.

Direct ARM64 targets:

| Function | Public Windows API |
|---|---|
| Endpoint master/channel | `IAudioEndpointVolume` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

The supplied x86 MalLgcy/CTAudEp DLLs do not need to become ARM64 runtime dependencies for this mixer subset.

CTAudEp's topology dB/scalar helper is a separate float path and is not proof of the CDC UInt16 format.

## CrystalVoice / non-EQ Acoustic Engine — backend recovered

Full trace:

`DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`

Supplied binaries:

- `Creative.Platform.CoreAudio.dll`
  - SHA-256 `189ee6750a7e70f24421f1e2100fa88847878456f11233e28ed2fa0a6d1d4823`
- `CTUSBAPO64.dll`
  - SHA-256 `fa23a53861087df19487497c54067128f61a266cce2eae000f1a40b8752a17d3`
- `CTUSBfilt64.sys`
  - SHA-256 `bc0140f821b4d2f83405a6e89b135f98b0df690b6fdefbab47c47d7ae8856105`

### Recovered official control plane

`Creative App / Platform`
-> `ApoDeviceRepoKeyFactory`
-> `PropStoreRepository`
-> `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
-> `IPropertyStore::GetValue / SetValue`
-> Windows Audio System Effects notification path
-> `CTUSBAPO64.dll` DSP modules

This is direct static evidence from the exact Platform and APO binaries.

### CoreAudio role

`Creative.Platform.CoreAudio.dll` is a managed COM interop layer containing:

- normal Core Audio/DeviceTopology interfaces;
- `IAudioSystemEffectsPropertyStore`;
- `IAudioSystemEffectsPropertyChangeNotificationClient`;
- default/user/volatile FX property-store operations.

Recovered `IAudioSystemEffectsPropertyStore` GUID:

`302AE7F9-D7E0-43E4-971B-1F8293613D2A`

### Platform APO repository

Exact `Creative.Platform.Devices.dll` contains:

- `ApoDeviceRepoKeyFactory`
- `PropStoreRepository`
- `ApoDeviceRepositoryInitializer`

`ApoDeviceRepoKeyFactory` references approximately 158 Creative `CTPKEY_*` values.

`PropStoreRepository` directly calls:

- `OpenUserPropertyStore`
- `IPropertyStore::GetValue`
- `IPropertyStore::SetValue`
- `RegisterPropertyChangeNotification`

### Selected exact feature keys

- Crystalizer Enable `{3cd83c04-868f-4f08-8d75-b4625ffe3b31}, PID 0`
- Crystalizer Level `{0f03f0bb-72c7-4ec1-8422-7b8d7410694a}, PID 0`
- SVM Enable `{9ad782d7-f46e-465c-8df5-3cda75424987}, PID 0`
- AEC Enable `{35f00393-1adf-43ce-84cb-7a926ac012b6}, PID 0`
- Noise Reduction Enable `{40d0d021-20bd-4d15-a93c-1dbe8922c642}, PID 1`
- MicBeam Plus Enable `{40d0d021-20bd-4d15-a93c-1dbe8922c642}, PID 0`
- TD NR family `{e370f545-381e-4961-9a94-7f97aafa77d7}, PID 0..5`
- Graphic EQ Enable `{9a9d0cb2-4dc9-494c-8210-9848ae1aa629}, PID 0`
- APO DirectMode Enable `{f3eaf467-52bd-4853-baa0-82d23a8759f5}, PID 0`

The same selected GUIDs are present in the supplied `CTUSBAPO64.dll`, tying the official Platform property repository to the native APO.

### Native APO DSP modules

`CTUSBAPO64.dll` contains concrete modules for:

- Crystalizer / THX Crystalizer
- Bass Management
- SVM
- Noise Reduction / TD Noise Reduction
- AEC
- MicBeam / MicBeamPlus
- VoiceFX
- Mic Signal Conditioning
- Graphic EQ

The APO uses modern System Effects 3 initialization and endpoint/system-effects property notifications.

Therefore the actual effect algorithms are in the user-mode Creative APO processing layer, not in the tested CTCDC `0x95` route.

### CTUSBfilt64 role

`CTUSBfilt64.sys` is a small supporting x64 WDM audio filter/forwarder with normal AddDevice/IRP behavior and a DRM/content-forwarding path.

No high-level CrystalVoice/Acoustic property repository or effect DSP implementation was recovered there.

Do not call it useless; it has driver-stack behavior. The narrower conclusion is that the high-level DSP/property consumer is the APO.

## ARM64 consequence for Creative effects

The controller/UI side can reproduce Windows Audio System Effects property-store reads/writes in native ARM64 code once the exact X4 endpoint key family and registration are known.

But writing properties alone does not reproduce the DSP.

The supplied `CTUSBAPO64.dll` is x86-64 and contains the actual effect implementation. The current ARM64 environment does not have the fully configured Creative APO/filter stack.

Full CrystalVoice/Acoustic restoration therefore requires:

1. correct endpoint FX registration/property stores;
2. a compatible APO processing layer;
3. required native dependencies/assets.

Static analysis has **not** yet established whether Windows ARM64 can host this exact x86-64 APO inside the audio engine. Do not assume yes or no without dedicated evidence.

## Current next actions

Do **not** create another broad runtime probe.

### Priority 1 — X4-specific APO selection

Statically trace:

- `ApoDeviceRepositoryInitializer`;
- product/endpoint support predicates/enrichers;
- which legacy/current `CTPKEY_*` family SB1815 selects;
- exact `PROPVARIANT` types/ranges/defaults for X4-relevant keys;
- SFX/MFX/EFX/AEC endpoint registration/INF properties;
- APO native dependencies/model files used by the relevant X4 modules.

Only after this is narrowed should a targeted read-only endpoint FX property-store enumeration be considered.

### Priority 2 — ARM64 APO hosting/registration feasibility

Determine whether the exact x64 Creative APO can be registered/hosted on this Windows ARM64 audio path or whether an ARM64 replacement processing layer is required.

Do not infer from normal application x64 emulation; audio-engine APO hosting must be checked specifically.

### Priority 3 — remaining CDC Game/Voice unit conversion

Continue only by finding a concrete App/UI consumer of `CDCGameVoice` raw UInt16 values.

Do not search MalLgcy/CTAudEp or the APO property path for this conversion again unless a direct reference proves relevance.

## Runtime safety rules

- GitHub is source of truth.
- Creative App closed for independent CTCDC runtime.
- One variable at a time.
- Read-only until a state-changing command/path is exactly justified.
- Every new state-changing hardware command needs physical X4 confirmation.
- `WriteFile` success alone is not hardware validation.
- No blind raw `0x95` probing.
- No unrelated changes.
- No B5 ASIO changes from this branch.
