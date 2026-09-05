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
2. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
3. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
4. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
5. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
6. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
7. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
8. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
9. `DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`
10. `DEBUG_HISTORY_20260904_X4_READONLY_CAPABILITY_RUNTIME_SUCCESS.md`
11. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
12. `X4_CONTROL_MAP.md`
13. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
14. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

## Scope boundary

This branch is for X4 Windows ARM64 native controller / driver-path analysis and the isolated ARM64 APO restoration track.

Keep it separate from B5 ASIO. Do not modify B5 ASIO source, WaveRT engine, mux, failsafe or control-panel behavior from this branch.

## Four backend classes

Every feature must remain classified into one of:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not collapse these into one generic Creative command path.

## CTCDC — confirmed live firmware path

Control interface:

`USB\VID_041E&PID_3278&MI_01`

Known session:

- COM3 in the validated machine
- 115200 / 8N1
- event mask `0x05`
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`
- `5A 03 00` -> `5A 03 02 3B 00`
- max payload 59
- firmware `1.9.251008.0930`

Creative App must be fully closed for independent CTCDC runtime tests because an ownership/session conflict is reproducible.

### Hardware-confirmed state change

Direct Mode:

- ON `5A 39 03 00 05 01`
- OFF `5A 39 03 00 05 00`

No other new state-changing command is authorized automatically.

## Firmware capability result

PlaybackManager `0x96` responded only for Graphic EQ params `9..20` in the tested X4 session.

Feature support query `5A 10 00` returned mask `0x40`, recovered as GraphicEQ.

Raw `VoiceInputManager 0x95` params `0..45` returned no response. This is **not** global CrystalVoice unsupported proof because the official Windows stack uses a separate APO property-store path.

Do not repeat blind `0x95` probing.

## AudioControl / AudioLevel status

Runtime `0x21` descriptor set:

0 Speaker; 1 Headphone; 2 SPDIF Output; 3 Mic Monitoring; 4 Line Monitoring; 5 Mic Input; 6 Line Input; 7 What U Hear Recording; 8 SPDIF Monitoring; 9 SPDIF Input; 10 AGC.

`0x22` ranges work for indices 0..9. `0x24` Mute GET works for 0..10.

Official `0x23` GET frame:

`5A 23 02 01 <index>`

Official managed response is exactly:

- index byte
- `UInt16 CurValue`

packed size 3. The runtime trailing fourth `0x03` is outside the managed struct and ignored by the official parser; meaning unresolved.

Creative Platform creates `0x23` keys only for Game/Voice indices selected from `GameAudioLevel (19)` / `ChatAudioLevel (18)`. X4's runtime descriptor list has neither. Therefore generic index 0..9 `0x23` probing is not the normal Windows mixer model and must not be repeated.

CDC raw UInt16 engineering-unit conversion remains unresolved; `/256` is not confirmed.

## Normal mixer — ARM64 path recovered

Original Creative chain:

`Creative.Platform.Mixer.dll -> MalLgcy.dll -> CTAudEp.dll -> Core Audio / DeviceTopology`

`MalLgcy.dll` and `CTAudEp.dll` supplied binaries are x86, but the required behavior is standard Windows COM and can be reimplemented directly on ARM64:

| Function | ARM64 Windows backend |
|---|---|
| Endpoint master/channel | `IAudioEndpointVolume` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

Do not port/load the x86 MalLgcy/CTAudEp binaries for this subset.

## Creative effects control plane — recovered

Official Windows control path:

`Creative App / Platform -> ApoDeviceRepoKeyFactory -> PropStoreRepository -> IAudioSystemEffectsPropertyStore -> IPropertyStore -> CTUSBAPO64.dll`

This path covers Crystalizer, Surround, SVM, Noise Reduction, AEC, MicBeam, Mic SVM, VoiceFX and related processing.

`PropStoreRepository` schema:

- Boolean -> `VT_BOOL (11)`
- float -> `VT_R4 (4)`
- float[] -> `VT_VECTOR|VT_R4 (0x1004)`
- string -> `VT_LPWSTR (31)`

Product gate:

- APO definition property family `{F1056047-B091-4D85-A5C0-B13D4D8BAC57}`
- render PID 0 / capture PID 1
- Creative APO HW identifier `100` -> `SB1815`

Key/range details are canonical in `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`.

Confirmed examples:

- Crystalizer Level: float 0..1, step 0.01, default 0.65
- Noise Reduction Strength: 0..1, 0.01, default 0.5
- Mic Smart Volume: 0..1, 0.01, default 0.74
- Surround Immersion: 0..1, 0.01, default 0.4
- Dialog Plus Strength: 0..1, 0.01, default 0.05
- SVM Mode: `VT_R4`, Normal 0.0 / Loud 1.0 / Night 2.0
- XBass Strength: APO backend 0..100 step 1 default 50; HID-only backend 0..1 step 0.01 default 0.5

## SB1815 official INF binding — recovered

Canonical trace:

`DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`

Supplied INF:

- SHA-256 `adc7b2128b9d90625efab36c6fc499d8d8f4328e368265f03222cf6720b98b0b`
- DriverVer `09/26/2024,3.06.03.00`
- X4 audio HWID `USB\VID_041E&PID_3278&MI_03`

The package is x86/amd64-only. There are no `ntarm64` sections.

The amd64 X4 install uses Microsoft `usbaudio2.inf` plus Creative additions:

- `CTUSBfilt64.sys` UpperFilter
- `CTUSBAPO64.dll`
- `CTUSBWrap64.dll`
- `CTUSBDGFX64.dll`
- 32-bit compatibility components

### Exact Windows 11 FX graph

Creative primary CLSIDs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

Bindings:

- Speaker: Creative SFX/MFX/EFX
- Headphone: same Creative SFX/MFX/EFX
- Microphone: same Creative SFX/MFX/EFX
- Line In: Microsoft effects, not the Creative set
- What U Hear: Creative SFX/MFX, no EFX in the SB1815 Win11 section
- SPDIF Out: Creative SFX + separate chainer MFX/EFX; EFX chain includes Creative EFX + DGFX and DDL selection

SPDIF/DDL therefore remains a separate later subproject.

### Windows 11 FX property contexts

General Creative context:

`{852311BC-1AFB-454E-92CA-C35252CACAAF}`

Headphone context:

`{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

INF creates `Default`, `Volatile` and `User` stores, directly corroborating the recovered `IAudioSystemEffectsPropertyStore` path.

## ARM64 APO conclusion — resolved

The supplied official package has no ARM64 install target, and `CTUSBAPO64.dll` is plain x86-64 PE32+.

Custom APOs are in-process COM DLLs in the Windows audio engine. A native Arm64 process cannot directly load a plain x64 DLL; Arm64X helps only when an actual Arm64 view/implementation exists.

Therefore the supplied x64 `CTUSBAPO64.dll` is **not** a viable direct in-process payload for native ARM64 AudioDG.

Correct architecture:

1. retain Microsoft USB Audio 2.0 as base driver;
2. provide ARM64-native APO component/extension packaging;
3. reproduce SB1815 Speaker/Headphone/Microphone SFX/MFX/EFX bindings;
4. preserve the recovered Creative FX property schema;
5. port/reimplement required DSP later;
6. keep SPDIF/DDL separate because its official graph depends on chainer/DGFX components.

## ARM64 APO Stage A0 — implementation created, build pending

Canonical implementation history:

`DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`

Source:

`src/x4-apo-arm64`

Manual workflow:

`Build X4 APO ARM64 Stage A0`

Workflow file:

`.github/workflows/build-x4-apo-arm64-stage-a0.yml`

Current Stage A0 classes use the official X4 CLSIDs:

- `CX4SfxApo` -> SFX
- `CX4MfxApo` -> MFX
- `CX4EfxApo` -> EFX

All three currently share one transparent `CBaseAudioProcessingObject` implementation.

Stage A0 behavior is deliberately constrained:

- native ARM64 target only;
- float32 pass-through processing only;
- no DSP algorithms;
- no Creative property-store writes;
- no controllable effects advertised;
- no effect-state setters;
- no CTCDC access;
- no registry/endpoint installation path;
- no SPDIF/DDL;
- no B5 changes.

`APOProcess` contains no allocation, COM, property access, logging or blocking work. Both real-time code and APO vtables use the SYSVAD `AVRT_CODE` / `AVRT_VTABLES` placement pattern.

`Initialize` accepts `APOInitSystemEffects3/2/1`; the SystemEffects3 path only caches processing mode and endpoint identity. It intentionally does not open the Creative FX property store because Speaker and Headphone share the same APO CLSIDs but use different context GUIDs.

### Critical validation status

**Stage A0 has not been compiled yet.**

A manual `workflow_dispatch` build workflow exists and verifies ARM64 PE machine `0xAA64`, but the currently connected GitHub action set cannot start a new manual workflow run. Do not report Stage A0 as build-successful until an actual run exists.

No installable APO INF/extension package exists yet. Do not install/bind Stage A0 to the live X4 endpoint before the DLL builds cleanly and packaging is separately reviewed.

## Immediate next actions

1. Run `Build X4 APO ARM64 Stage A0` manually and capture compiler/linker output.
2. Fix only real build errors; keep Stage A0 pass-through/no-state-change.
3. Verify produced DLL PE machine is `0xAA64`.
4. Only after clean build, create a **non-installing review `.inx` template** for Windows 11 componentized APO packaging.
5. Do not bind/install on the live endpoint until the component/extension package is reviewed.
6. After graph-loading validation, add endpoint-context selection and read-only FX property access before any DSP or setters.
7. Keep SPDIF/DDL/CTUSBWrap/DGFX outside the first APO milestone.
8. Continue CDC Game/Voice UInt16 conversion only when a concrete App/UI consumer is found.

## Runtime safety

- Creative App closed for independent CTCDC tests.
- One variable at a time.
- Stage A0 must remain pass-through/read-only until graph loading is validated.
- Every new state-changing hardware operation requires separate physical confirmation.
- No blind `0x95` probing.
- No generic `0x23` probing.
- No unrelated changes.
- No B5 ASIO changes from this branch.
