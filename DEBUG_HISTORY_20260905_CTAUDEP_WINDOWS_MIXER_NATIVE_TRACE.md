# DEBUG HISTORY — 2026-09-05 CTAudEp Windows Mixer Native Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace continues the recovered Windows mixer chain:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll`
-> `CTAudEp.dll`
-> Windows Core Audio / DeviceTopology

The supplied `CTAudEp.dll` was analyzed statically only. No hardware runtime probe was added, no state-changing command was sent, and no B5 ASIO code was modified.

## Binary

Supplied:

`CTAudEp.dll`

SHA-256:

`76adb6b105757849eb61c69842db7a4f46ae01251c0b1791fe7d248a31b469fa`

Architecture:

- PE32
- x86 (`IMAGE_FILE_MACHINE_I386`)

Therefore this exact binary cannot be loaded in-process by the ARM64-native controller.

## 1. Result summary

The targeted Creative Windows mixer functions in `CTAudEp.dll` resolve to public Microsoft Core Audio / DeviceTopology COM interfaces.

Recovered implementation split:

1. endpoint master/channel volume -> `IAudioEndpointVolume`;
2. monitoring volume/mute -> `IDeviceTopology` / `IPart` -> `IAudioVolumeLevel` / `IAudioMute`;
3. Mic Boost -> `KSNODETYPE_VOLUME` part -> `IAudioVolumeLevel`;
4. Mic AGC -> `KSNODETYPE_AGC` part -> `IAudioAutoGainControl`.

The analyzed functions do not require a Creative-only kernel IOCTL to perform those controls.

`CTAudEp.dll` imports `DeviceIoControl` for other functionality, but the mixer functions traced here use the public COM topology path.

This means the ordinary X4 Windows mixer subset can be reconstructed directly for ARM64 without porting or loading the x86 `MalLgcy.dll` / `CTAudEp.dll` binaries.

## 2. Relevant GUIDs recovered from the binary

### Endpoint activation

`CLSID_MMDeviceEnumerator`

`{BCDE0395-E52F-467C-8E3D-C4579291692E}`

`IID_IMMDeviceEnumerator`

`{A95664D2-9614-4F35-A746-DE8DB63617E6}`

`IID_IAudioEndpointVolume`

`{5CDF2C82-841E-4546-9722-0CF74078229A}`

### DeviceTopology

`IID_IDeviceTopology`

`{2A07407E-6497-4A18-9787-32F79BD0D98F}`

`IID_IPart`

`{AE2DE0E4-5BCA-4F2D-AA46-5D13F8FDB3A9}`

`IID_IAudioVolumeLevel`

`{7FB7B48F-531D-44A2-BCB3-5AD5A134B3DC}`

`IID_IAudioMute`

`{DF45AEEA-B74A-4B6B-AFAD-2366B6AA012E}`

`IID_IAudioAutoGainControl`

`{85401FD4-6DE4-4B9D-9869-2D6753A82F3C}`

### KS node types

`KSNODETYPE_VOLUME`

`{3A5ACC00-C557-11D0-8A2B-00A0C9255AC1}`

`KSNODETYPE_AGC`

`{E88C9BA0-C557-11D0-8A2B-00A0C9255AC1}`

## 3. Endpoint master/channel volume

Relevant exports include:

- `CTGetMasterVolume`
- `CTSetMasterVolume`
- `CTGetChannelVolume`
- `CTSetChannelVolume`
- `CTGetVolumeRange`

The common activation sequence is:

1. `CoCreateInstance(CLSID_MMDeviceEnumerator, ..., CLSCTX_ALL, IID_IMMDeviceEnumerator, ...)`;
2. `IMMDeviceEnumerator::GetDevice(endpointId, ...)`;
3. `IMMDevice::Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, nullptr, ...)`.

### Master GET

`CTGetMasterVolume(endpointId, fScalar, out level)` selects:

- `fScalar != 0` -> `IAudioEndpointVolume::GetMasterVolumeLevelScalar`;
- `fScalar == 0` -> `IAudioEndpointVolume::GetMasterVolumeLevel`.

### Channel GET

`CTGetChannelVolume(endpointId, fScalar, channel, out level)` selects:

- scalar -> `GetChannelVolumeLevelScalar`;
- non-scalar -> `GetChannelVolumeLevel`.

The setters mirror the same split using the scalar or dB setter methods.

`CTGetVolumeRange` uses `IAudioEndpointVolume::GetVolumeRange`.

### ARM64 consequence

No Creative-specific endpoint-volume algorithm is required here. The ARM64 implementation can call `IAudioEndpointVolume` directly.

## 4. Monitoring control discovery

`CTOpenMonitoringControl` activates the endpoint's `IDeviceTopology` and follows the connected topology.

Recovered sequence:

1. get `IMMDevice` by endpoint ID;
2. `IMMDevice::Activate(IID_IDeviceTopology, ...)`;
3. `IDeviceTopology::GetConnector(0, ...)`;
4. `IConnector::GetConnectedTo(...)`;
5. QueryInterface the connected object for `IPart`;
6. traverse incoming parts with `IPart::EnumPartsIncoming`;
7. attempt activation of `IAudioVolumeLevel` and `IAudioMute` on relevant parts;
8. retain the discovered part/control objects in the monitoring-control collection.

This establishes that monitoring is topology-part based, not generic CDC `0x23` per-index control.

## 5. Monitoring level and mute

### Volume level

`CTGetVolumeLevelOfMonitoringControl(handle, channel, fScalar, out level)` uses:

`IAudioVolumeLevel::GetLevel(channel, out dB)`.

If `fScalar == 0`, the dB value is returned directly.

If `fScalar != 0`, it also obtains:

`IAudioVolumeLevel::GetLevelRange(channel, out minDB, out maxDB, out stepDB)`

and converts the dB value into the Creative scalar representation.

### Range

`CTGetVolumeLevelRangeOfMonitoringControl` calls `IAudioVolumeLevel::GetLevelRange` directly.

Thus the range values at this boundary are dB floats.

### Channel count

`CTGetVolumeChannelCountOfMonitoringControl` uses `IAudioVolumeLevel::GetChannelCount`.

### Mute

Monitoring mute uses `IAudioMute`:

- GET -> `IAudioMute::GetMute`;
- SET -> `IAudioMute::SetMute`.

### SET level

When scalar input is requested, CTAudEp converts scalar -> dB first, then calls `IAudioVolumeLevel::SetLevel` or `SetLevelUniform`.

The non-scalar path passes the dB value directly.

## 6. Mic Boost

`CTOpenKsNodeTypeVolumeOfAudioEndpoint` locates a topology part whose subtype is:

`KSNODETYPE_VOLUME`

and activates:

`IID_IAudioVolumeLevel`.

### GET

`CTGetLevelOfKsNodeTypeVolumeOfAudioEndpoint`:

1. calls `IAudioVolumeLevel::GetChannelCount`;
2. calls `GetLevel` for each channel;
3. retains the largest channel dB value;
4. if scalar mode was requested, obtains the range and converts that dB value to scalar.

### Range

`CTGetLevelRangeOfKsNodeTypeVolumeOfAudioEndpoint` calls `GetLevelRange` for channel 0.

### SET

`CTSetLevelOfKsNodeTypeVolumeOfAudioEndpoint`:

- converts scalar input to dB when `fScalar != 0`;
- then uses `IAudioVolumeLevel::SetLevelUniform`.

The managed Creative Mixer path previously recovered for Mic Boost calls this native layer with `fScalar=false`, so the normal Creative Platform Mic Boost value is dB at this boundary.

## 7. Mic AGC

`CTOpenKsNodeTypeAutoGainControlOfAudioEndpoint` locates:

`KSNODETYPE_AGC`

and activates:

`IID_IAudioAutoGainControl`.

Recovered state operations:

- GET -> `IAudioAutoGainControl::GetEnabled`;
- SET -> `IAudioAutoGainControl::SetEnabled`.

Therefore Mic AGC is also a public DeviceTopology control path.

## 8. CTAudEp dB/scalar conversion

For `IAudioVolumeLevel`, Windows exposes dB levels rather than the endpoint-volume scalar methods available on `IAudioEndpointVolume`.

CTAudEp therefore contains private conversion helpers.

### Direct exponential mapping

The recovered base conversion is:

`scalar = 10 ^ ((levelDB - maxDB) / 20)`

and the inverse branch is based on:

`levelDB = maxDB + 20 * log10(scalar)`

with boundary validation/clamping.

### Low-range special handling

The binary contains a separate branch when the dB range width is below `40.0` dB.

That branch computes a reference scalar at `minDB + 6.0 dB` and applies a linear low-end segment before returning to the normal exponential curve above that point.

Constants recovered directly from the code include:

- `10.0`
- `20.0`
- `6.0`
- `40.0`

For dB -> scalar, the low-end segment is equivalent to:

- at/below minimum -> scalar `0`;
- between `minDB` and `minDB + 6` -> linearly interpolate from `0` to the exponential scalar corresponding to `minDB + 6`;
- above that threshold -> standard exponential mapping.

The scalar -> dB helper mirrors this branch structure and clamps the lower boundary before using `log10` on the exponential branch.

This conversion belongs to the Windows `IAudioVolumeLevel` topology path.

It is **not** evidence that CDC `0x22/0x23` raw `UInt16` values use the same representation, and it does not confirm a CDC `/256` conversion.

## 9. Creative-specific dependency assessment

For the mixer functions traced in this document, the important behavior is provided by Microsoft interfaces already present in Windows:

- `IAudioEndpointVolume`;
- `IDeviceTopology`;
- `IConnector`;
- `IPart`;
- `IAudioVolumeLevel`;
- `IAudioMute`;
- `IAudioAutoGainControl`.

No requirement to load x86 `MalLgcy.dll` or x86 `CTAudEp.dll` remains for implementing these controls on ARM64.

This result does **not** imply that all Creative audio features are public Core Audio controls. CrystalVoice, Acoustic Engine processing, encoder features and other APO/filter-backed functionality remain a separate backend problem.

## 10. ARM64 implementation map

The ordinary mixer subset can be implemented natively as follows:

| Function | ARM64 direct implementation |
|---|---|
| Endpoint master volume | `IMMDevice` + `IAudioEndpointVolume` |
| Endpoint channel volume | `IAudioEndpointVolume` channel APIs |
| Endpoint volume range | `IAudioEndpointVolume::GetVolumeRange` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

The final ARM64 controller does not need to reproduce the legacy DLL boundary itself; it needs to reproduce the observable control behavior.

## 11. Consequences and next action

Confirmed consequences:

- stop treating x86 MalLgcy/CTAudEp binaries as runtime dependencies for the ARM64 controller;
- ordinary Windows mixer control can be rebuilt directly with public Core Audio / DeviceTopology COM;
- do not confuse CTAudEp's float dB/scalar conversion with CDC raw `UInt16` AudioLevel conversion;
- no new mixer SET is required to establish this static architecture.

Next static priorities:

1. recover the official CDC Game/Voice raw `UInt16` engineering-unit conversion from its actual higher-layer consumer;
2. continue CrystalVoice / non-EQ Acoustic Engine backend tracing through Creative Platform/CoreAudio and Creative APO/filter/KS-property paths;
3. only after static design is complete, implement a narrowly scoped ARM64 Core Audio mixer layer rather than porting the x86 Creative wrappers.
