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
2. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
3. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
4. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
5. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
6. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
7. `DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`
8. `DEBUG_HISTORY_20260904_X4_READONLY_CAPABILITY_RUNTIME_SUCCESS.md`
9. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
10. `X4_CONTROL_MAP.md`
11. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
12. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`

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

Do not collapse these layers into one generic Creative command path.

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

Full traces:

- `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
- `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`

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
-> Windows Audio System Effects notification
-> `CTUSBAPO64.dll` DSP modules

This is direct static evidence from the exact Platform and APO binaries.

### CoreAudio role

`Creative.Platform.CoreAudio.dll` is a managed COM interop layer containing normal Core Audio/DeviceTopology plus:

- `IAudioSystemEffectsPropertyStore`
- `IAudioSystemEffectsPropertyChangeNotificationClient`
- default/user/volatile FX property-store operations.

Recovered `IAudioSystemEffectsPropertyStore` GUID:

`302AE7F9-D7E0-43E4-971B-1F8293613D2A`

### Platform APO repository

Exact `Creative.Platform.Devices.dll` contains:

- `ApoDeviceRepoKeyFactory`
- `PropStoreRepository`
- `ApoDeviceRepositoryInitializer`
- `APODeviceFilter`

`PropStoreRepository` directly calls:

- `OpenUserPropertyStore`
- `IPropertyStore::GetValue`
- `IPropertyStore::SetValue`
- `RegisterPropertyChangeNotification`

## X4/SB1815 APO support predicate — recovered

Endpoint APO-definition PROPERTYKEY family:

`{f1056047-b091-4d85-a5c0-b13d4d8bac57}`

- Render PID `0`
- Capture PID `1`

`APODeviceFilter` selects the direction-specific endpoint property, obtains APO information and checks the Creative supported-model table.

Recovered official mapping:

- APO hardware identifier `100` -> `SB1815`

This is the static product gate. It does **not** prove that the current ARM64 Microsoft USB Audio 2.0 endpoint presently has the official Creative APO-definition property installed.

Relevant `EDeviceRepositoryType` values:

- Apo = 1
- CDC = 3
- HID = 5
- Mixer = 6

## APO property-store value schema — recovered

`PropStoreRepository` writer mapping:

| Managed value | PROPVARIANT |
|---|---|
| Boolean | `VT_BOOL (11)` |
| Single/float | `VT_R4 (4)` |
| float[] | `VT_VECTOR | VT_R4 (0x1004)` |
| String | `VT_LPWSTR (31)` |

This means Creative effect toggles and level parameters must be read/written using their actual property types, not arbitrary integer storage.

### Selected playback keys

| Feature | PROPERTYKEY |
|---|---|
| Surround Enable | `{5b4777a4-8ad4-4d34-893a-df34da0e56ca},0` |
| Surround Immersion | `{a5a78ea4-c156-4db7-85aa-81cff1c3f192},0` |
| Crystalizer Enable | `{3cd83c04-868f-4f08-8d75-b4625ffe3b31},0` |
| Crystalizer Level | `{0f03f0bb-72c7-4ec1-8422-7b8d7410694a},0` |
| SVM Enable | `{9ad782d7-f46e-465c-8df5-3cda75424987},0` |
| DynamX SVM Mode | `{e6ec3743-ddd2-4817-8466-b433761dcf9d},0` |
| Bass Redirection | `{d3dcf273-cf72-40c5-a1ab-a7785a849ea8},0` |
| Bass Crossover | `{836d3bc0-7c99-4e38-990f-68775abc8335},0` |
| XBass Enable | `{f67cf426-f8cb-4a40-bdac-580802e3e193},0` |
| XBass Strength | `{dd527e35-21a5-4ca6-ab90-8ad464fb55e3},0` |
| Graphic EQ Enable | `{9a9d0cb2-4dc9-494c-8210-9848ae1aa629},0` |
| APO Direct Mode | `{f3eaf467-52bd-4853-baa0-82d23a8759f5},0` |

Graphic EQ Gain0..9 use GUID `{2b88c76d-d07c-4e97-8922-1bac9f6d5935}`, PIDs 0..9.

### Selected CrystalVoice keys

| Feature | PROPERTYKEY |
|---|---|
| Mic SVM Enable | `{400d2ef9-cec3-4c2f-ab54-4f9b47f7d615},0` |
| Mic SVM Strength | `{22821d29-df1d-4907-a721-4b3937542e87},0` |
| AEC Enable | `{35f00393-1adf-43ce-84cb-7a926ac012b6},0` |
| Noise Reduction Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642},1` |
| Noise Reduction Strength | `{6a72f5dd-6c09-4147-82c5-14c64b0e4e0f},0` |
| MicBeam Plus Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642},0` |
| MicBeam Wedge Angle | `{72e09675-2af9-485c-89f1-898e532bf06e},0` |
| MicBeam Source Angle | `{a0d4f6a1-9775-48a2-8d4d-c0441436bf60},0` |
| MicBeam Gain | `{8d6ddb63-253d-424e-be3b-7391722c4227},0` |
| TD Noise Reduction family | `{e370f545-381e-4961-9a94-7f97aafa77d7},0..5` |

VoiceFX common GUID:

`{f7e70860-8eb1-4c6a-b2e1-1033b409ff5d}`

- Enable PID 99 / `VT_BOOL`
- parameters PID 0..7 / `VT_R4`

## Recovered feature ranges/defaults

| Parameter | Min | Max | Step | Default |
|---|---:|---:|---:|---:|
| Crystalizer Level | 0 | 1 | 0.01 | 0.65 |
| Crystalizer PreAttenuation | 0 | 1 | 0.01 | 1.0 |
| Noise Reduction Strength | 0 | 1 | 0.01 | 0.5 |
| Mic Smart Volume Strength | 0 | 1 | 0.01 | 0.74 |
| Surround Immersion | 0 | 1 | 0.01 | 0.4 |
| Dialog Plus Strength | 0 | 1 | 0.01 | 0.05 |
| Voice Focus / MicBeam wedge | 20 | 180 | 1 | 30 |
| Bass crossover | 10 | 1000 | 1 | 80 |

### SVM mode

Official representation is float / `VT_R4`:

- Normal = 0.0
- Loud = 1.0
- Night = 2.0

### XBass range selection — corrected and confirmed

The XBass feature enricher branches on repository availability:

- APO present -> Strength `0..100`, step `1`, default `50`
- HID-only -> Strength `0..1`, step `0.01`, default `0.5`

This is statically resolved control flow and corrects the earlier tentative expectation that APO would use the normalized range.

## Native APO DSP modules

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

## CTUSBfilt64 role

`CTUSBfilt64.sys` is a supporting x64 WDM audio filter/forwarder with normal AddDevice/IRP behavior and a DRM/content-forwarding path.

No high-level CrystalVoice/Acoustic property repository or DSP implementation was recovered there.

Do not call it useless; the narrow conclusion is that high-level DSP/property consumption is in the APO.

## ARM64 consequence for Creative effects

The controller/UI side can reproduce Windows Audio System Effects property-store reads/writes in native ARM64 code.

The schema is now sufficiently precise to build a targeted read-only FX property inspector later.

But writing properties alone does not reproduce the DSP.

The supplied `CTUSBAPO64.dll` is x86-64 and contains the effect implementation. Full Creative effect restoration requires:

1. correct endpoint FX/APO registration;
2. a compatible audio-engine APO processing layer;
3. required native dependencies/assets.

Static analysis has not yet established whether Windows ARM64 can host this exact x86-64 APO in the audio engine. Do not assume yes or no.

## Current next actions

Do **not** create another broad runtime probe.

### Priority 1 — APO registration / endpoint binding

Recover:

- Creative installation/INF or endpoint registration for SB1815;
- actual SFX/MFX/EFX/AEC positions used by X4;
- endpoint registration properties and CLSID bindings;
- native dependencies/model/config assets required by X4-relevant APO modules.

### Priority 2 — ARM64 APO hosting feasibility

Determine whether the exact x86-64 Creative APO can be registered/hosted by the Windows ARM64 audio engine or whether an ARM64 replacement processing layer is required.

Do not infer this from normal app emulation.

### Priority 3 — targeted read-only FX property validation

Only after registration expectations are understood, consider a narrow read-only inspector that checks:

- direction-specific APO-definition property;
- availability of `IAudioSystemEffectsPropertyStore`;
- exact known keys with exact recovered `PROPVARIANT` types.

No property SET is authorized by this handoff.

### Priority 4 — remaining CDC Game/Voice unit conversion

Continue only by finding a concrete App/UI consumer of `CDCGameVoice` raw UInt16 values.

Do not search MalLgcy/CTAudEp or the APO property path for that conversion again without direct evidence.

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
