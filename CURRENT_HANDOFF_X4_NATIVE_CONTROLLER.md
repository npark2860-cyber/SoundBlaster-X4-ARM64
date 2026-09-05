# CURRENT HANDOFF — Sound Blaster X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Current controller branch:

`exp/windows-arm64-x4-native-controller`

At the start of the next chat, verify the actual branch HEAD on GitHub before doing anything else.

Do not reconstruct state from conversation memory when repository documents can be checked.

## Read order for the next tab

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
3. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
4. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
5. `DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260904_X4_READONLY_CAPABILITY_RUNTIME_SUCCESS.md`
7. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
8. `X4_CONTROL_MAP.md`
9. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
10. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
11. older histories only when needed

## Scope boundary

This branch is for X4 Windows ARM64 native controller / driver-path analysis.

Keep it separate from B5 ASIO work.

Do not modify B5 ASIO source, WaveRT engine, mux, runtime failsafe, control-panel behavior, or unrelated paths from this controller branch.

The current Windows ARM64 machine uses the Microsoft USB Audio 2.0 path rather than the complete official Creative filter/APO stack. That fact must be considered when interpreting missing Creative features.

## CTCDC session — confirmed operating condition

X4 control interface:

`USB\VID_041E&PID_3278&MI_01`

Current tested port:

`COM3`

Validated session initialization:

- event mask `0x05`
- 115200 / 8N1
- zero COM timeouts
- `PurgeComm(0x0F)`
- `SETDTR`

Fast-path session gate:

- `5A 03 00` -> `5A 03 02 3B 00`
- maximum payload = 59
- firmware query -> `1.9.251008.0930`
- button query -> `5A 26 06 05 00 01 00 1E 00`

### Creative App conflict

A reproducible runtime condition was discovered:

- Creative App running: the independent CTCDC path can fail at the first readiness query.
- Creative App fully closed: the same independent path works again.

Treat this as an ownership/session conflict condition. Do not change protocol bytes to compensate.

Independent runtime tests must be performed with Creative App fully closed.

## Direct Mode — hardware-confirmed

Known state-changing frames:

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

Windows hardware state change has been physically confirmed previously.

Every new state-changing command still requires separate physical confirmation.

## Read-only capability-map result

The 83-query capability probe completed the entire query loop without losing the CTCDC session.

`83 / 83` means all queries were issued/completed by the probe loop, not that all received data replies.

### Malcolm PlaybackManager (`0x96`)

Only the Graphic EQ block responded:

- param 9: EQ enable = `1.0`
- param 10: preamp = `0.0`
- params 11..20: 10-band GEQ values

Observed band values:

`3, 2, 0, -2, 0, 1, 2, 3, 3, 3.5`

These match the supplied SB1815 Music EQ preset.

Surround, Dialog Plus, Smart Volume, Crystalizer and Bass parameters did not return data responses through this raw Malcolm path.

### VoiceInputManager (`0x95`)

Params `0..45` all returned no response.

Do **not** interpret this as proof that X4 lacks CrystalVoice globally.

The correct interpretation is that this raw VoiceInputManager route is not exposed in the tested firmware/session, while the ARM64 environment also lacks the complete official Creative driver/filter/APO stack.

### Direct generic commands

- GraphicEqualizerControl `0x44`: ACK `0x81 NotSupported`
- SoundModeControl `0xA7`: ACK `0x81 NotSupported`

Do not use those generic enum entries as X4 backends.

## Malcolm sub-feature support — runtime-confirmed

`5A 10 00`

Response:

`5A 10 08 40 00 00 00 00 00 00 00`

Parsed:

- FeatureMask = `0x00000040`
- UnavailableMask = `0x00000000`

Recovered mapping identifies `0x40` as GraphicEQ.

This is consistent with the runtime where only the PlaybackManager GEQ block responded. Do not broaden this into a claim that the entire product lacks other Creative software/driver-backed features.

## Mixer AudioControl — runtime-confirmed firmware discovery

### `0x21` descriptors

Runtime returned 11 controls:

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

Indices 0..9 advertise volume + mute. AGC advertises mute but no volume.

### `0x22` ranges

RangeCount = 10, covering indices `0..9`.

The exact CDC raw `UInt16` engineering-unit conversion is still not statically proven. Do not hard-code `/256` as confirmed yet.

### `0x24` Mute GET

All 11 indices returned valid current mute states.

Observed muted controls in the capture:

- index 3 / Mic Monitoring = `1`
- index 10 / Automatic Gain Control = `1`

All other indices were `0` in that capture.

## `0x23` AudioLevel — official static path recovered

Full trace:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

### Request

`RawCmdAudioLevelGet`, `Pack=1`:

- `Operation : byte`
- `AudioControlIndex : byte`

GET frame:

`5A 23 02 01 <index>`

### Managed response

`RawResAudioLevelGet`, `Pack=1`:

- `AudioControlIndex : byte`
- `CurValue : UInt16`

Managed payload size is exactly **3 bytes**.

The successful runtime responses for raw index 0/1 contained a fourth trailing `0x03`, but that byte is:

- not a response-structure member;
- not managed padding;
- ignored by the official managed decode.

Its firmware semantic meaning remains unresolved. Do not assign a channel-mask or similar meaning without direct evidence.

### Official scope of `0x23`

Creative Platform creates `RawCmdAudioLevelGet/Set` keys only for:

- `CDCGameVoice.GameIndex`
- `CDCGameVoice.VoiceIndex`

Those indices are selected from descriptor types:

- `GameAudioLevel = 19`
- `ChatAudioLevel = 18`

Incoming level callbacks are likewise matched only against those stored Game/Voice indices.

The runtime X4 descriptor list contains neither type 18 nor type 19.

Therefore:

- generic per-index `0x23` reads are not the official Windows Speaker/Input/Monitoring mixer model;
- raw index 2..9 `GeneralFailure` is not proof of missing volume capability;
- do not repeat generic `0x23` probing;
- do not issue `0x23` SET.

## General Windows Mixer path — native implementation recovered

Full traces:

- `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`
- `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`

Recovered chain:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll!CSCT*`
-> `CTAudEp.dll!CT*`
-> Microsoft Core Audio / DeviceTopology

Supplied binaries:

- `MalLgcy.dll` SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`, x86 PE32
- `CTAudEp.dll` SHA-256 `76adb6b105757849eb61c69842db7a4f46ae01251c0b1791fe7d248a31b469fa`, x86 PE32

### MalLgcy role

The relevant `CSCT*` functions are thin wrappers that pass the original arguments unchanged to matching `CTAudEp.dll!CT*` functions.

No scalar/dB/CDC fixed-point conversion occurs in MalLgcy.

### CTAudEp endpoint path

Endpoint master/channel volume is standard `IAudioEndpointVolume`:

1. `CoCreateInstance(CLSID_MMDeviceEnumerator, ..., IID_IMMDeviceEnumerator)`;
2. `IMMDeviceEnumerator::GetDevice(endpointId)`;
3. `IMMDevice::Activate(IID_IAudioEndpointVolume, ...)`;
4. use scalar or dB endpoint methods according to `fScalar`.

ARM64 can implement this directly without Creative DLLs.

### CTAudEp monitoring path

Monitoring uses public DeviceTopology:

1. activate `IID_IDeviceTopology` on the endpoint;
2. get connector 0;
3. follow `IConnector::GetConnectedTo`;
4. QueryInterface for `IPart`;
5. traverse `IPart::EnumPartsIncoming`;
6. activate `IAudioVolumeLevel` and/or `IAudioMute` on relevant parts.

Monitoring volume/range/channel-count use `IAudioVolumeLevel`; mute uses `IAudioMute`.

### Mic Boost

CTAudEp locates `KSNODETYPE_VOLUME` and activates `IAudioVolumeLevel`.

The managed Creative Mic Boost path uses `fScalar=false`, so its normal native value is dB.

### Mic AGC

CTAudEp locates `KSNODETYPE_AGC` and activates `IAudioAutoGainControl`.

State maps directly to `GetEnabled` / `SetEnabled`.

### CTAudEp dB/scalar helper

For topology `IAudioVolumeLevel`, CTAudEp performs float dB/scalar conversion itself.

Base mapping:

`scalar = 10 ^ ((levelDB - maxDB) / 20)`

The code contains a low-end 6 dB interpolation branch when the range width is below 40 dB.

This conversion applies to the Windows DeviceTopology float path only. It does **not** prove the encoding of CDC `0x22/0x23 UInt16` values.

### ARM64 architecture conclusion

The ordinary Windows mixer subset does **not** require porting/loading the x86 MalLgcy/CTAudEp binaries.

Direct ARM64 mapping:

| Function | ARM64 Windows API |
|---|---|
| Endpoint master volume | `IAudioEndpointVolume` |
| Endpoint channel volume | `IAudioEndpointVolume` channel APIs |
| Endpoint range | `IAudioEndpointVolume::GetVolumeRange` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

`CTAudEp.dll` imports `DeviceIoControl` for other functionality, but no Creative-only kernel IOCTL requirement was found in the mixer functions traced above.

## Driver/APO architecture rule

Controller implementation must classify each feature into one of four backend classes:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative filter/APO/driver-side processing
4. Creative App/profile orchestration

Current confirmed examples:

- Direct Mode -> CTCDC firmware
- Graphic EQ -> CTCDC PlaybackManager GEQ block
- Mixer descriptor/range/mute discovery -> CTCDC `0x21/0x22/0x24`
- CDC `0x23` official Platform path -> Game/Voice feature indices, not generic Windows mixer
- endpoint/channel/monitoring volume -> public Core Audio / DeviceTopology
- Mic Boost / Mic AGC -> public DeviceTopology KS-node interfaces
- CrystalVoice -> backend not yet resolved; raw `0x95` no-response is not global unsupported proof
- Acoustic Engine non-EQ controls -> backend not yet resolved
- Dolby Digital Live / encoder -> driver/software path remains relevant

## Files / probe state

Probe directory:

`src/x4-control-readonly-probe`

Current diagnostics include:

- `x4-control-readonly-probe.exe`
- `x4-mixer-readonly-drilldown.exe`
- one-click CMD launchers

Manual workflow:

`Build X4 Read-Only Capability Probe ARM64`

No new runtime probe is required for the recovered AudioLevel/MalLgcy/CTAudEp architecture.

## Next engineering action

Do **not** create another broad runtime probe.

### Priority 1 — CDC Game/Voice raw engineering unit

Recover the exact official consumer-side conversion for CDC raw `UInt16` level/range values.

Known exclusions:

- no conversion in `RawResAudioLevelGet`;
- no conversion in `AudioControlLevelRange`;
- no conversion in `CDCGameVoiceFeature` inside `Creative.Platform.Devices.dll`;
- no CDC raw conversion in MalLgcy;
- CTAudEp's float dB/scalar helper is a separate DeviceTopology path.

Observed values remain numerically compatible with signed Q8.8, but `/256` is not confirmed.

Trace Creative App/UI and other Platform assemblies that actually consume `CDCGameVoice`, `GameAudioLevel`, `ChatAudioLevel`, or those raw range/current values.

### Priority 2 — CrystalVoice / Acoustic Engine backend

Continue static tracing through:

- `Creative.Platform.CoreAudio.dll`
- other App/Platform assemblies
- `CTUSBAPO64.dll`
- `CTUSBfilt64.sys`
- Creative KS/property GUID paths already identified

Do not repeat blind raw `0x95` probing.

### Implementation gate

The ordinary mixer path is now understood well enough for a future native ARM64 Core Audio implementation.

When implementation is started, begin with narrowly scoped read-only endpoint/topology discovery and keep setters gated behind separate validation. Do not interpret this handoff as authorization to issue new state-changing commands automatically.

## Safety / workflow rules

- GitHub is source of truth.
- Verify branch HEAD before starting.
- Creative App closed for independent CTCDC runtime.
- One variable at a time in runtime tests.
- Read-only until a state-changing command has exact evidence.
- Every new state-changing hardware command requires physical X4 confirmation.
- `WriteFile` success alone is not hardware validation.
- No unrelated modifications.
- No ASIO modifications from this controller branch.
