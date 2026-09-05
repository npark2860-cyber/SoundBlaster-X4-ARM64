# X4 CONTROL MAP — Static Analysis Baseline

Updated: 2026-09-05 KST

## Scope

This document records the read-only/static control map recovered from the supplied Creative Windows binaries for Sound Blaster X4 / SB1815. It is an analysis baseline, not a claim that every listed operation has already been hardware-validated on X4.

Source binary hashes:

- `Creative.Platform.Devices.dll` SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.Platform.Mixer.dll` SHA-256 `33f6ac6c84e093c766e8b483660d49518a8a0c14da144bd7a6a4f8bf0a79ae45`
- `CTCDC.dll` SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- `CTIntrfu.dll` SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

The Devices/CTCDC/CTIntrfu hashes match the previously recorded controller baseline exactly.

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

## Device communication operation

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
| Surround Enable | `0` |
| Surround Immersion | `1` |
| Dialog Plus Enable | `2` |
| Dialog Plus Strength | `3` |
| Smart Volume Enable | `4` |
| Smart Volume Strength | `5` |
| Smart Volume Mode | `6` |
| Crystalizer Enable | `7` |
| Crystalizer Level | `8` |
| Graphic EQ Enable | `9` |
| Graphic EQ Preamp | `10` |
| Graphic EQ Bands 0..9 | `11..20` |
| Bass Crossover | `23` |
| Bass Enable | `24` |
| Bass Strength | `25` |

### VoiceInputManager (`0x95`)

| Parameter group | IDs |
|---|---:|
| AEC Enable / delays | `0..3` |
| Noise Reduction | `4..5` |
| Voice Focus | `6..9` |
| Voice FX | `10..18` |
| Mic EQ Enable | `19` |
| Mic EQ Gains 0..7 | `20..27` |
| Mic EQ Center Frequencies 0..7 | `28..35` |
| Mic EQ Bandwidths 0..7 | `36..43` |
| Mic Smart Volume Enable / Strength | `44..45` |

## Graphic Equalizer control (`0x44`)

Recovered `GraphicEqualizerOperation` values:

- Get state = `0x02`
- Get total bands = `0x03`
- Get band level range = `0x04`
- Get one band level = `0x05`
- Get all band levels = `0x06`
- Set one band level = `0x07`
- Set all band levels = `0x08`
- Set state = `0x09`
- Get support bitmask = `0x0A`
- Get total prestored presets = `0x0B`
- Get prestored preset name = `0x0C`
- Set active index = `0x0D`
- Get active index = `0x0E`

Support bits:

- EQ band setting = `0x01`
- Prestored EQ preset selection = `0x02`

## Mixer / AudioControl

Recovered `AudioControlType` values:

- Speaker = `1`
- Headphone = `2`
- Mic input = `3`
- Line input = `4`
- What U Hear recording = `5`
- USB input = `6`
- Bluetooth input = `7`
- Room calibration = `8`
- SPDIF input = `9`
- Aux input = `10`
- Smart-device input = `11`
- External mic input = `12`
- Subwoofer = `13`
- SPDIF output = `14`
- Mic monitoring = `15`
- Line monitoring = `16`
- SPDIF monitoring = `17`
- Chat audio = `18`
- Game audio = `19`
- Headset = `20`
- Automatic Gain Control = `63`

### Runtime-confirmed SB1815 AudioControl list

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

`0x22` returns range entries for indices `0..9` and `0x24` Mute GET works for indices `0..10` in the tested runtime.

## AudioLevel (`0x23`) — official Windows static model

Full trace:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

### GET request

`RawCmdAudioLevelGet`, `Pack=1`:

- `Operation : byte`
- `AudioControlIndex : byte`

GET frame:

`5A 23 02 01 <index>`

### GET response

`RawResAudioLevelGet`, `Pack=1`:

- `AudioControlIndex : byte`
- `CurValue : UInt16`

Official managed payload size: **3 bytes**.

The extra runtime trailing `0x03` seen on raw index 0/1 is not a field of this structure and is ignored by the managed parser. Its firmware semantic meaning remains unresolved.

### Official call-path scope

Creative Platform creates `0x23` keys only for `CDCGameVoice.GameIndex` and `VoiceIndex`.

Those are selected by descriptor type:

- `GameAudioLevel = 19`
- `ChatAudioLevel = 18`

The runtime SB1815 list above contains neither type. Therefore do not use generic per-index `0x23` reads as the normal Windows Speaker/Input/Monitoring mixer backend, and do not interpret raw index 2..9 `GeneralFailure` as proof of missing volume capability.

No `0x23` SET is authorized.

## Windows Mixer backend

`Creative.Platform.Mixer.dll` routes ordinary mixer controls through `ICTMalLgcyLibrary` / native `MalLgcy.dll` using Windows endpoint/topology/KS objects.

| Control | Managed path | Native mode |
|---|---|---|
| Endpoint master volume | `MixerLine` | `fScalar=true` |
| Endpoint channel volume | `MixerLine` | `fScalar=true` |
| Monitoring level | `MonitorLine` | `fScalar=true` |
| Mic Boost | KS node volume | `fScalar=false` / dB path |
| Mic AGC | KS node auto-gain | boolean/control path |

`MixerLine` and `MonitorLine` convert normalized scalar floats to/from managed 0..100.

KS/monitoring range signatures explicitly name their range outputs `MinLevelDB` / `MaxLevelDB`.

The exact CDC raw `UInt16` fixed-point-to-dB conversion for `0x22/0x23` is not implemented in the recovered Devices path and remains to be located before hard-coding a conversion.

## Sound Mode control (`0xA7`)

Relevant recovered operations:

- Set active = `0`
- Get active = `1`
- Get support = `2`
- Get name data = `6`
- Get name-data support = `7`
- Get short-name data = `9`
- Get short-name support = `10`
- Get UUID = `12`
- Get UUID support = `13`
- Get module included state = `17`
- Get module parameters = `19`
- Get toggle mask = `22`
- Get parameter customization query = `24`
- Get module parameter support = `25`

Module IDs:

- Equalizer = `0`
- Acoustic Engine = `1`
- Super X-Fi = `2`
- LED Control = `3`
- Dolby Digital = `4`

Output types:

- Speaker = `0`
- Headphone = `1`
- Line Out = `2`
- Active = `7`

Acoustic Engine Sound Mode parameters map to Surround, Crystalizer, Bass, Smart Volume, Dialog Plus, and Bass Crossover.

## Runtime-confirmed state-changing item

Direct Mode remains the currently hardware-confirmed state-changing Windows control:

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

## Current controller-analysis branch

Branch:

`exp/windows-arm64-x4-native-controller`

Probe path:

`src/x4-control-readonly-probe`

No new runtime probe is required for the recovered `0x23` backend split. Continue static recovery before any new state-changing command.
