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
4. `DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`
5. `DEBUG_HISTORY_20260904_X4_READONLY_CAPABILITY_RUNTIME_SUCCESS.md`
6. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
7. `X4_CONTROL_MAP.md`
8. `DEBUG_HISTORY_20260903_WINDOWS_CTCDC_PATH.md`
9. `DEBUG_HISTORY_20260903_CTCDC_NATIVE_UNLOCK_TRACE.md`
10. older histories only when needed

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

Treat this as an ownership/session conflict condition.

Do not change protocol bytes to compensate.

Independent runtime tests must be performed with Creative App fully closed.

## Direct Mode — hardware-confirmed

Known state-changing frames:

- ON: `5A 39 03 00 05 01`
- OFF: `5A 39 03 00 05 00`

Windows hardware state change has been physically confirmed previously.

Every new state-changing command still requires separate physical confirmation.

## First read-only capability-map result

The 83-query capability probe completed the entire query loop without losing the CTCDC session.

Important: `83 / 83` means all queries were issued/completed by the probe loop, not that all received data replies.

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

The correct interpretation is that this raw VoiceInputManager route is not exposed in the tested firmware/session, while the current ARM64 environment also lacks the complete official Creative driver/filter/APO stack.

### Direct generic commands

- GraphicEqualizerControl `0x44`: ACK `0x81 NotSupported`
- SoundModeControl `0xA7`: ACK `0x81 NotSupported`

Do not use those generic enum entries as X4 backends.

## Malcolm sub-feature support — runtime-confirmed

The mixer drill-down added:

`5A 10 00`

Response:

`5A 10 08 40 00 00 00 00 00 00 00`

Parsed:

- FeatureMask = `0x00000040`
- UnavailableMask = `0x00000000`

The recovered mask table maps `0x40` to GraphicEQ.

This is consistent with the previous runtime where only the PlaybackManager GEQ block responded.

This mask result must not be broadened into a claim that the entire X4 product lacks other Creative software/driver-backed features.

## Mixer AudioControl — runtime-confirmed live route

### `0x21` information

Runtime returned 11 controls with indices `0..10`:

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

All 0..9 advertise volume + mute. AGC advertises mute but no volume.

### `0x22` level ranges

RangeCount = 10, covering indices `0..9`.

Raw range data is recorded in:

`DEBUG_HISTORY_20260905_X4_MIXER_DRILLDOWN_RUNTIME_SUCCESS.md`

The exact CDC raw `UInt16` fixed-point/engineering-unit conversion is still not statically proven. Do not hard-code `/256` as confirmed yet.

### `0x24` Mute GET — full success

All 11 indices returned valid current mute states.

Observed muted controls in the captured runtime:

- index 3 / Mic Monitoring = `1`
- index 10 / Automatic Gain Control = `1`

All other indices = `0` in that capture.

This strongly validates the `0..10` index map and `0x24` GET path.

## `0x23` AudioLevel — static path recovered

Full trace:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

### Exact request structure

Official packed `RawCmdAudioLevelGet` fields:

- `Operation : byte`
- `AudioControlIndex : byte`

For GET:

`5A 23 02 01 <index>`

### Exact managed response structure

Official packed `RawResAudioLevelGet` fields:

- `AudioControlIndex : byte`
- `CurValue : UInt16`

Managed payload size is exactly **3 bytes**.

The successful hardware responses for raw index 0/1 contained a fourth trailing `0x03`, but that byte is:

- not a `RawResAudioLevelGet` field;
- not managed padding (`Pack=1`);
- ignored by the official managed response decode.

Its firmware-level semantic meaning remains unresolved. Do not assign a channel-mask or other meaning without new evidence.

### Official call-path meaning

Critical correction: Creative Platform does **not** create generic `0x23` command keys for every `0x21` AudioControl index.

It creates `RawCmdAudioLevelGet/Set` keys only for:

- `CDCGameVoice.GameIndex`
- `CDCGameVoice.VoiceIndex`

Those indices are selected from descriptor types:

- `GameAudioLevel = 19`
- `ChatAudioLevel = 18`

Incoming `RawResAudioLevelGet` callbacks are also matched only against those stored Game/Voice indices.

The runtime X4 descriptor list contains neither type 18 nor type 19.

Therefore raw index 2..9 `GeneralFailure` must not be interpreted as missing volume support, and generic per-index `0x23` probing is not the official Windows mixer model.

Do not issue `0x23` SET.

## General Windows Mixer path — static-confirmed

Managed/native chain:

`Creative.Platform.Mixer.dll`
-> `ICTMalLgcyLibrary`
-> `MalLgcy.dll!CSCT*`
-> `CTAudEp.dll!CT*`

Full MalLgcy native trace:

`DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`

Supplied MalLgcy binary:

- SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`
- x86 / PE32

`MixerRepositoryInitializer` selects a Windows `IDeviceEndpoint.DeviceEndpointId`, constructs its `MixerLine`, and discovers monitoring lines, Mic Boost and Mic AGC from that endpoint/topology.

Recovered paths:

- endpoint master/channel volume -> `MixerLine` -> `MalLgcy` -> `CTAudEp`
- monitoring levels -> `MonitorLine` -> monitoring-control handles -> `MalLgcy` -> `CTAudEp`
- Mic Boost -> KS node type volume -> `MalLgcy` -> `CTAudEp`
- Mic AGC -> KS node auto-gain-control -> `MalLgcy` -> `CTAudEp`

The native API bool parameter is named `fScalar`.

- `MixerLine` / `MonitorLine` use `fScalar=true`, normalized float internally and 0..100 in the managed wrapper.
- Mic Boost uses `fScalar=false`; range output names are explicitly `MinLevelDB` / `MaxLevelDB`.

The MalLgcy native wrappers perform no conversion. They pass the original parameters directly into matching CTAudEp functions.

Therefore:

- there is no hidden raw-level or dB conversion inside MalLgcy;
- `CTAudEp.dll` is the next implementation layer for Windows endpoint/topology/KS behavior;
- the supplied x86 MalLgcy binary cannot be loaded directly into an ARM64-native process;
- the ARM64 controller should reproduce the required endpoint/topology/KS behavior directly once CTAudEp implementation details are recovered.

This still does **not** prove the fixed-point interpretation of CDC `0x22/0x23` raw `UInt16` values.

## Driver/APO architecture rule

Controller implementation must classify each feature into one of four backend classes:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative filter/APO/driver-side processing
4. Creative App/profile orchestration

Current confirmed examples:

- Direct Mode -> CTCDC firmware path
- Graphic EQ -> CTCDC PlaybackManager GEQ block
- Mixer descriptor/range/mute discovery -> CTCDC `0x21/0x22/0x24`
- CDC `0x23` official Platform path -> Game/Voice feature indices, not generic Windows mixer
- normal endpoint/channel/monitoring level -> Creative Platform Mixer -> MalLgcy -> CTAudEp endpoint/topology path
- Mic Boost / Mic AGC -> CTAudEp KS-node paths behind the legacy wrapper
- CrystalVoice -> backend not yet resolved; raw `0x95` route rejected as current X4 path
- Acoustic Engine non-EQ controls -> backend not yet resolved
- Dolby Digital Live / encoder -> do not assume firmware command; driver/software path remains relevant

The original purpose of the ARM64 driver effort includes recovering Creative functions that disappear when the official Creative filter/APO stack is unavailable.

## Files / probe state

Probe directory:

`src/x4-control-readonly-probe`

Current diagnostics include:

- `x4-control-readonly-probe.exe`
- `x4-mixer-readonly-drilldown.exe`
- one-click CMD launchers

Manual GitHub Actions workflow:

`Build X4 Read-Only Capability Probe ARM64`

No new runtime probe is required for the recovered `0x23` call-path split or MalLgcy forwarding path.

## Next engineering action

Do **not** create another broad runtime probe.

Primary Windows Mixer static target:

1. inspect `CTAudEp.dll`;
2. recover the exact Core Audio / DeviceTopology calls for endpoint master/channel and monitoring controls;
3. recover the exact KS node/property implementation for Mic Boost and Mic AGC;
4. record GUIDs/property IDs/node matching needed for a direct ARM64 implementation.

Remaining CDC AudioLevel static work:

1. recover the exact official conversion for CDC raw `UInt16` level/range values before treating them as dB;
2. search higher Creative Platform/App consumers of `CDCGameVoice` values; do not look for this conversion in MalLgcy again.

Then continue CrystalVoice / non-EQ Acoustic Engine backend tracing through:

- `Creative.Platform.Devices.dll`
- `Creative.Platform.Mixer.dll`
- `Creative.Platform.CoreAudio.dll`
- `CTAudEp.dll`
- `CTUSBAPO64.dll`
- `CTUSBfilt64.sys`
- Creative KS/property paths.

Do not repeat blind raw `0x95` probing.

## Safety / workflow rules

- GitHub is source of truth.
- Verify branch HEAD before starting.
- Creative App closed for independent CTCDC runtime.
- One variable at a time in runtime tests.
- Read-only until a state-changing command has exact evidence.
- Every new state-changing command requires physical X4 confirmation.
- `WriteFile` success alone is not hardware validation.
- No unrelated modifications.
- No ASIO modifications from this controller branch.
