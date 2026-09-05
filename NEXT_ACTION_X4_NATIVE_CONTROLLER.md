# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
2. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
3. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`

## `AudioLevel (0x23)` — resolved call-path split

Static-confirmed facts:

- GET frame: `5A 23 02 01 <index>`;
- official `RawResAudioLevelGet` payload is `AudioControlIndex : byte` + `CurValue : UInt16`, packed size 3;
- runtime trailing `0x03` is outside that managed response structure and is ignored by the official managed decode;
- Creative Platform creates `0x23` GET/SET keys only for discovered `CDCGameVoice.GameIndex` / `VoiceIndex`;
- those indices come from `GameAudioLevel (19)` / `ChatAudioLevel (18)` descriptors;
- runtime X4 `0x21` descriptors contain neither type;
- general Speaker/Input/Monitoring control is not generic per-index `0x23` control.

Therefore:

- do not repeat generic `0x23` probing over indices `0..9`;
- do not interpret index `2..9` `GeneralFailure` as missing volume support;
- do not issue `0x23` SET.

## Windows Mixer native path — static recovery complete

Recovered chain:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll`
-> `CTAudEp.dll`
-> Microsoft Core Audio / DeviceTopology

Supplied native binaries:

- `MalLgcy.dll` SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`, x86 PE32;
- `CTAudEp.dll` SHA-256 `76adb6b105757849eb61c69842db7a4f46ae01251c0b1791fe7d248a31b469fa`, x86 PE32.

### Direct ARM64 replacement map

| Control | Native Windows implementation |
|---|---|
| Endpoint master volume | `IMMDevice` + `IAudioEndpointVolume` |
| Endpoint channel volume | `IAudioEndpointVolume` channel APIs |
| Endpoint volume range | `IAudioEndpointVolume::GetVolumeRange` |
| Monitoring volume | `IDeviceTopology` / `IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology` / `IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

`CTAudEp` uses standard endpoint scalar methods for `IAudioEndpointVolume`.

For topology `IAudioVolumeLevel`, CTAudEp performs its own float dB/scalar conversion because that interface exposes dB values. The recovered base mapping is:

`scalar = 10 ^ ((levelDB - maxDB) / 20)`

with a low-end 6 dB interpolation branch for ranges below 40 dB.

This conversion is only the Windows topology float conversion. It does **not** prove the representation of CDC `0x22/0x23 UInt16` values.

### Architecture consequence

Do not port or load the supplied x86 MalLgcy/CTAudEp DLLs into the ARM64-native controller.

For this ordinary mixer subset, reproduce the observable behavior directly through public Windows COM interfaces.

No Creative-only kernel IOCTL requirement was found in the traced endpoint/monitoring/Mic Boost/AGC functions.

## Immediate static priority — CDC Game/Voice engineering units

The remaining `AudioLevel` question is now narrowly scoped to the CDC Game/Voice raw `UInt16` representation.

Exact known state:

- `RawResAudioLevelGet.GetValue()` returns raw `UInt16` unchanged;
- `AudioControlLevelRange` carries raw `UInt16` Min/Max/Step unchanged;
- `CDCGameVoiceFeature` passes those values through without dB conversion in `Creative.Platform.Devices.dll`;
- neither MalLgcy nor CTAudEp consumes this CDC representation;
- CTAudEp's float dB/scalar helper belongs to DeviceTopology and must not be reused as proof for CDC fixed-point format.

Observed X4 raw values remain numerically compatible with signed Q8.8, but `/256` is still **not confirmed**.

### Next targets for this point

Trace the actual higher-layer consumer of `CDCGameVoice` values:

1. Creative App/UI assemblies that display or set Game/Voice balance/level;
2. other Creative Platform assemblies referencing `CDCGameVoiceFeature`, `GameAudioLevel`, `ChatAudioLevel`, `AudioControlLevelRange`, or the raw `UInt16` values;
3. only follow native code where a concrete reference from that consumer exists.

Do not spend more time looking for the CDC conversion in MalLgcy/CTAudEp.

## Secondary priority — CrystalVoice / non-EQ Acoustic Engine

Continue backend classification using the four-layer rule:

1. firmware / CTCDC;
2. Windows Core Audio endpoint/property;
3. Creative filter/APO/driver processing;
4. Creative App/profile orchestration.

The next important static targets are:

- `Creative.Platform.CoreAudio.dll`;
- other Creative Platform assemblies referenced by the App;
- `CTUSBAPO64.dll`;
- `CTUSBfilt64.sys`;
- recovered Creative KS/property GUID paths.

Current raw `VoiceInputManager (0x95)` no-response does **not** prove CrystalVoice is unsupported. Do not repeat blind `0x95` probing without new backend evidence.

## Implementation gate

The ordinary Windows mixer path is now sufficiently understood to design an ARM64 implementation, but keep implementation separate from unresolved feature work.

If/when implementation begins, first implement the smallest read-only Core Audio layer and validate endpoint/topology discovery before adding state-changing setters.

Do not use this as permission to issue new hardware writes automatically.

## Runtime safety rules

- Creative App must be fully closed for independent CTCDC runtime tests.
- Keep read-only tests read-only unless a specific state-changing command has exact evidence.
- Every new state-changing hardware command requires physical X4 confirmation.
- `WriteFile` success alone is not physical validation.
- Do not weaken or alter B5 ASIO runtime behavior.
- Do not change unrelated paths.

## Known live firmware routes

- Direct Mode: `0x39`, hardware-confirmed ON/OFF.
- Graphic EQ state: `0x11`, module `0x96`, params `9..20`, hardware-read confirmed.
- Mixer AudioControl descriptor/range/mute: `0x21/0x22/0x24`, hardware-read confirmed.
- `0x23`: official Creative Platform use is Game/Voice-indexed; do not treat it as generic Windows mixer level control.

## Known rejected / unsupported routes in current session

- direct GraphicEqualizerControl `0x44`: ACK `0x81 NotSupported`;
- direct SoundModeControl `0xA7`: ACK `0x81 NotSupported`;
- raw VoiceInputManager `0x95` params `0..45`: no response;
- naked COM Direct Mode without CTCDC session setup: rejected previously;
- HID / BLE / UAC Extension Unit / vendor-interface guesses: do not repeat without new evidence.
