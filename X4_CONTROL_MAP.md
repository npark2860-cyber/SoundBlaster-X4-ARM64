# X4 CONTROL MAP — Static Analysis Baseline

Updated: 2026-09-05 KST

## Scope

This document records the read-only/static control map recovered from supplied Creative Windows binaries for Sound Blaster X4 / SB1815. It is not a claim that every listed operation is already hardware-validated on X4.

## Source binaries

- `Creative.Platform.Devices.dll` SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.Platform.Mixer.dll` SHA-256 `33f6ac6c84e093c766e8b483660d49518a8a0c14da144bd7a6a4f8bf0a79ae45`
- `Creative.Platform.CoreAudio.dll` SHA-256 `189ee6750a7e70f24421f1e2100fa88847878456f11233e28ed2fa0a6d1d4823`
- `CTCDC.dll` SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- `CTIntrfu.dll` SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`
- `MalLgcy.dll` SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`
- `CTAudEp.dll` SHA-256 `76adb6b105757849eb61c69842db7a4f46ae01251c0b1791fe7d248a31b469fa`
- `CTUSBAPO64.dll` SHA-256 `fa23a53861087df19487497c54067128f61a266cce2eae000f1a40b8752a17d3`
- `CTUSBfilt64.sys` SHA-256 `bc0140f821b4d2f83405a6e89b135f98b0df690b6fdefbab47c47d7ae8856105`

Architecture notes:

- MalLgcy / CTAudEp supplied binaries are x86 PE32 legacy wrappers/implementations for the normal mixer path.
- `CTUSBAPO64.dll` and `CTUSBfilt64.sys` are x86-64 native binaries.
- Normal mixer behavior is recoverable through public Windows COM without depending on MalLgcy/CTAudEp at ARM64 runtime.
- Creative effects require a separate Audio System Effects/APO processing path.

## Backend classes

Every controller feature must be classified into one of these layers:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative filter/APO processing
4. Creative App/profile orchestration

Several features can have more than one product-generation/path representation. A matching name alone does not prove which backend X4 actively uses.

## Confirmed CDCRawCommand values

| Command | Value |
|---|---:|
| MaxPayloadSize | `0x03` |
| DeviceInformationV2 | `0x09` |
| GetMalcolmSubFeatureSupport | `0x10` |
| GetMalcolmParameter | `0x11` |
| SetMalcolmParameter | `0x12` |
| GetAudioControlInformation | `0x21` |
| GetAudioLevelRanges | `0x22` |
| AudioLevel | `0x23` |
| AudioMute | `0x24` |
| HardwareButton | `0x26` |
| SpeakerConfiguration | `0x29` |
| SpeakerOutputTargetSelectionControl | `0x2C` |
| MicPriorityConfig | `0x2D` |
| FeatureControl | `0x39` |
| LEDControl | `0x3A` |
| SubwooferSetup | `0x40` |
| GraphicEqualizerControl | `0x44` |
| SpeakerChannelConfiguration | `0x4B` |
| SuperXFi | `0x6F` |
| NetworkStandby | `0x80` |
| LEDSegmentBrightness | `0x81` |
| CustomButtonControl | `0xA5` |
| SoundModeControl | `0xA7` |
| ModuleProfileControl | `0xA8` |
| Passthrough | `0xA9` |

## Device communication operations

`DevCommOperation`:

- Set = `0`
- Get = `1`
- Support = `2`
- SupportV2 = `5`
- GetAddParam = `6`
- SetV2 = `7`
- GetV2 = `8`

## Malcolm modules

| Module | ID |
|---|---:|
| MasterControl | `0x80` |
| VoiceInputManager | `0x95` |
| PlaybackManager | `0x96` |
| DolbyDecoder | `0x97` |

### PlaybackManager (`0x96`)

| Parameter | ID |
|---|---:|
| Surround Enable | 0 |
| Surround Immersion | 1 |
| Dialog Plus Enable | 2 |
| Dialog Plus Strength | 3 |
| Smart Volume Enable | 4 |
| Smart Volume Strength | 5 |
| Smart Volume Mode | 6 |
| Crystalizer Enable | 7 |
| Crystalizer Level | 8 |
| Graphic EQ Enable | 9 |
| Graphic EQ Preamp | 10 |
| Graphic EQ Bands 0..9 | 11..20 |
| Bass Crossover | 23 |
| Bass Enable | 24 |
| Bass Strength | 25 |

Current X4 runtime Malcolm feature mask exposes GraphicEQ only in the recovered table. Do not use the no-response state of other parameters as proof that their Windows APO implementations do not exist.

### VoiceInputManager (`0x95`)

Recovered parameter model:

| Group | IDs |
|---|---:|
| AEC | 0..3 |
| Noise Reduction | 4..5 |
| Voice Focus | 6..9 |
| Voice FX | 10..18 |
| Mic EQ Enable | 19 |
| Mic EQ Gains | 20..27 |
| Mic EQ Center Frequencies | 28..35 |
| Mic EQ Bandwidths | 36..43 |
| Mic Smart Volume | 44..45 |

Runtime params `0..45` returned no response in the tested CTCDC session. This only rejects that raw route for the tested X4 session; it does not reject the Windows APO implementation described below.

## Mixer / AudioControl

Recovered `AudioControlType` values:

- Speaker = 1
- Headphone = 2
- Mic input = 3
- Line input = 4
- What U Hear recording = 5
- USB input = 6
- Bluetooth input = 7
- Room calibration = 8
- SPDIF input = 9
- Aux input = 10
- Smart-device input = 11
- External mic input = 12
- Subwoofer = 13
- SPDIF output = 14
- Mic monitoring = 15
- Line monitoring = 16
- SPDIF monitoring = 17
- Chat audio = 18
- Game audio = 19
- Headset = 20
- Automatic Gain Control = 63

### Runtime-confirmed SB1815 list

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

`0x22` returns ranges for indices 0..9. `0x24` Mute GET works for indices 0..10 in the tested runtime.

## AudioLevel (`0x23`) — official static model

Full trace:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

GET request `RawCmdAudioLevelGet`, `Pack=1`:

- Operation byte
- AudioControlIndex byte

Exact GET frame:

`5A 23 02 01 <index>`

GET response `RawResAudioLevelGet`, `Pack=1`:

- AudioControlIndex byte
- CurValue UInt16

Managed payload size = 3 bytes.

The runtime trailing fourth `0x03` is outside this managed structure and is ignored by the official managed decode. Its firmware semantic meaning remains unresolved.

Creative Platform creates `0x23` keys only for `CDCGameVoice.GameIndex` / `VoiceIndex`, selected from type 19 / 18 descriptors. The runtime X4 descriptor list has neither type.

Therefore generic per-index `0x23` reads are not the normal Windows Speaker/Input/Monitoring mixer backend. Do not repeat generic `0x23` probing and do not issue `0x23` SET.

The CDC raw UInt16 engineering-unit conversion remains unresolved; `/256` is not yet confirmed.

## Normal Windows Mixer backend — native path recovered

Full traces:

- `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
- `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`

Recovered chain:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll`
-> `CTAudEp.dll`
-> Microsoft Core Audio / DeviceTopology

Direct ARM64 map:

| Control | Public Windows backend |
|---|---|
| Endpoint master/channel | `IAudioEndpointVolume` |
| Endpoint volume range | `IAudioEndpointVolume::GetVolumeRange` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

CTAudEp's topology dB/scalar base mapping is:

`scalar = 10 ^ ((levelDB - maxDB) / 20)`

with a low-end 6 dB interpolation branch for ranges below 40 dB.

This is a float DeviceTopology conversion and is unrelated to proving CDC UInt16 format.

## Creative APO / CrystalVoice backend — static-confirmed

Full trace:

`DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`

Recovered control plane:

`Creative Platform feature model`
-> `ApoDeviceRepoKeyFactory`
-> `PropStoreRepository`
-> `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
-> `IPropertyStore::GetValue/SetValue`
-> Windows Audio System Effects property notification
-> `CTUSBAPO64.dll`

`Creative.Platform.CoreAudio.dll` supplies the managed COM interop surface including `IAudioSystemEffectsPropertyStore` and normal Core Audio/DeviceTopology interfaces.

`ApoDeviceRepoKeyFactory` references approximately 158 Creative `CTPKEY_*` values. `PropStoreRepository` directly performs user FX property-store reads/writes and registers change notification.

### Selected playback PROPERTYKEYs

| Feature | GUID | PID |
|---|---|---:|
| CMSS3D / Surround Enable | `5b4777a4-8ad4-4d34-893a-df34da0e56ca` | 0 |
| CMSS3D Immersion | `a5a78ea4-c156-4db7-85aa-81cff1c3f192` | 0 |
| Crystalizer Enable | `3cd83c04-868f-4f08-8d75-b4625ffe3b31` | 0 |
| Crystalizer Level | `0f03f0bb-72c7-4ec1-8422-7b8d7410694a` | 0 |
| SVM Enable | `9ad782d7-f46e-465c-8df5-3cda75424987` | 0 |
| SVM Strength | `80b0c7bb-0989-434e-af5b-fb9020f471b3` | 0 |
| Bass Redirection | `d3dcf273-cf72-40c5-a1ab-a7785a849ea8` | 0 |
| Bass Crossover | `836d3bc0-7c99-4e38-990f-68775abc8335` | 0 |
| XBass Enable | `f67cf426-f8cb-4a40-bdac-580802e3e193` | 0 |
| Graphic EQ Enable | `9a9d0cb2-4dc9-494c-8210-9848ae1aa629` | 0 |
| APO Direct Mode | `f3eaf467-52bd-4853-baa0-82d23a8759f5` | 0 |

Graphic EQ `Gain0..9` use GUID `2b88c76d-d07c-4e97-8922-1bac9f6d5935`, PIDs 0..9.

### Selected CrystalVoice PROPERTYKEYs

| Feature | GUID | PID |
|---|---|---:|
| Mic SVM Enable | `400d2ef9-cec3-4c2f-ab54-4f9b47f7d615` | 0 |
| Mic SVM Strength | `22821d29-df1d-4907-a721-4b3937542e87` | 0 |
| AEC Enable | `35f00393-1adf-43ce-84cb-7a926ac012b6` | 0 |
| Noise Reduction Enable | `40d0d021-20bd-4d15-a93c-1dbe8922c642` | 1 |
| Noise Reduction Strength | `6a72f5dd-6c09-4147-82c5-14c64b0e4e0f` | 0 |
| MicBeam Plus Enable | `40d0d021-20bd-4d15-a93c-1dbe8922c642` | 0 |
| MicBeam Wedge Angle | `72e09675-2af9-485c-89f1-898e532bf06e` | 0 |
| MicBeam Source Angle | `a0d4f6a1-9775-48a2-8d4d-c0441436bf60` | 0 |
| MicBeam Gain | `8d6ddb63-253d-424e-be3b-7391722c4227` | 0 |
| TD Noise Reduction family | `e370f545-381e-4961-9a94-7f97aafa77d7` | 0..5 |

The same selected GUIDs are physically present in `CTUSBAPO64.dll`, confirming the Platform repository and native APO belong to the same control plane.

### DSP modules in `CTUSBAPO64.dll`

Recovered concrete module classes include:

- Crystalizer / THX Crystalizer
- Bass Management
- SVM / THX SVM
- Noise Reduction / TD Noise Reduction
- AEC / AEC reference
- MicBeam / MicBeamPlus
- VoiceFX
- Mic Signal Condition
- GraphicEQ

The APO initializes via the modern System Effects 3 path and implements endpoint/system-effects property notifications.

### APO registration CLSIDs

| FX registration | CLSID |
|---|---|
| GFX | `{CA854A19-6601-407B-8AFB-CB5C2801AFE6}` |
| LFX | `{DA3AD2CF-79F9-41B7-BE44-753ADEEC2EDD}` |
| SFX | `{71DAB6A1-39F3-423E-90A8-032729851157}` |
| MFX | `{C624D7B2-8333-448E-85C8-51EEFC2025ED}` |
| EFX | `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}` |
| OSFX | `{BD813F37-2483-4ED1-90A8-6C4587A6AACB}` |
| OMFX | `{05800E59-C53F-487A-91A7-C3FB4B91B9E6}` |
| AEC MFX | `{9A626D17-A2FD-40DD-876B-0F9792DE4B4F}` |

Binary presence does not prove these CLSIDs are registered on the current ARM64 endpoint. Product/INF/endpoint registration must still be recovered.

## CTUSBfilt64 classification

`CTUSBfilt64.sys` is a supporting x64 WDM audio filter/forwarding component.

Static evidence includes normal AddDevice/IRP filter-stack logic and `DrmForwardContentToDeviceObject` use.

No high-level CrystalVoice/Acoustic effect property repository or DSP modules were identified there. The effect algorithms/property consumer evidence are in `CTUSBAPO64.dll`.

## Current implementation implications

### Safe to implement directly on ARM64

- normal endpoint/channel mixer via Core Audio;
- monitoring / Mic Boost / Mic AGC via DeviceTopology;
- controller-side Audio System Effects property-store access, once X4-specific endpoint keys/registration are identified.

### Not solved by controller writes alone

Crystalizer, SVM, Noise Reduction, AEC, MicBeam, VoiceFX and related DSP require an actual compatible APO processing layer.

The supplied `CTUSBAPO64.dll` is x86-64, not native ARM64. Static work has not yet proven whether Windows ARM64 can host this exact APO in the audio engine. Do not assume either direction.

## Sound Mode control (`0xA7`)

Recovered operations include active/support/name/UUID/module/toggle/parameter queries. Module IDs:

- Equalizer = 0
- Acoustic Engine = 1
- Super X-Fi = 2
- LED Control = 3
- Dolby Digital = 4

The direct generic `0xA7` command returned `NotSupported` in the tested X4 CTCDC session. This does not invalidate the separate Windows APO property path.

## Runtime-confirmed state-changing item

Direct Mode firmware remains the only currently hardware-confirmed state-changing controller command:

- ON `5A 39 03 00 05 01`
- OFF `5A 39 03 00 05 00`

No new state-changing command is authorized by this static map.
