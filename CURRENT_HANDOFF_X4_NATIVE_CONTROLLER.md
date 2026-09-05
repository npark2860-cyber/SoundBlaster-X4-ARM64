# CURRENT HANDOFF — Sound Blaster X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Verified code/workflow baseline immediately before this handoff refresh:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

Always verify the actual branch HEAD before continuing because the handoff-document commits themselves advance HEAD.

Keep this branch separate from B5 ASIO. Do not modify B5 ASIO source, WaveRT engine, mux, failsafe, runtime control-panel behavior, or unrelated paths from this workstream.

## Read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_SPEAKER_PACKAGE_OFFLINE_GATE.md`
3. `DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`
4. `DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`
5. `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
7. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
8. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
9. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
10. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
11. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
12. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
13. `packaging/x4-apo-arm64-stage-a1-speaker/README.md`
14. `packaging/x4-apo-arm64-review/README.md`

## Architecture boundary

Keep four backend classes distinct:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not collapse these into one generic Creative command path.

## Fixed firmware / mixer facts

- CTCDC: `USB\VID_041E&PID_3278&MI_01`
- validated firmware: `1.9.251008.0930`
- Direct Mode hardware-confirmed:
  - ON `5A 39 03 00 05 01`
  - OFF `5A 39 03 00 05 00`
- raw PlaybackManager `0x96` exposed Graphic EQ only in the tested session
- raw VoiceInputManager `0x95` no-response is not global CrystalVoice unsupported proof
- generic raw `0x23` probing must not be repeated
- CDC UInt16 engineering-unit conversion remains unresolved; `/256` is not confirmed

Normal ARM64 Windows mixer path remains standard Core Audio / DeviceTopology:

- endpoint master/channel -> `IAudioEndpointVolume`
- monitoring volume/mute -> `IDeviceTopology/IPart + IAudioVolumeLevel/IAudioMute`
- Mic Boost -> `KSNODETYPE_VOLUME + IAudioVolumeLevel`
- Mic AGC -> `KSNODETYPE_AGC + IAudioAutoGainControl`

Do not port/load supplied x86 MalLgcy/CTAudEp for this subset.

## Creative APO control plane — recovered

Official Windows path:

`Creative Platform -> IAudioSystemEffectsPropertyStore -> IPropertyStore -> Creative APO`

Primary property types:

- bool -> `VT_BOOL`
- float -> `VT_R4`
- float vector -> `VT_VECTOR|VT_R4`
- string -> `VT_LPWSTR`

Product gate:

- APO definition family `{F1056047-B091-4D85-A5C0-B13D4D8BAC57}`
- render PID 0 / capture PID 1
- APO HW identifier `100` -> `SB1815`

Detailed keys/ranges remain canonical in `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`.

## Official SB1815 Windows 11 APO graph — recovered

X4 audio HWID:

`USB\VID_041E&PID_3278&MI_03`

Primary Creative CLSIDs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

Official Win11 binding:

- Speaker -> SFX/MFX/EFX
- Headphone -> same SFX/MFX/EFX
- Microphone -> same SFX/MFX/EFX
- Line In -> Microsoft effects
- What U Hear -> Creative SFX/MFX
- SPDIF -> separate chainer/DGFX/DDL path

General FX context:

`{852311BC-1AFB-454E-92CA-C35252CACAAF}`

Headphone context:

`{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

## ARM64 APO Stage A0 — PASSED

Source:

`src/x4-apo-arm64`

Validated DLL:

- size `176640`
- SHA-256 `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / machine `0xAA64`
- exports `DllGetClassObject`, `DllCanUnloadNow`
- exact official SFX/MFX/EFX CLSIDs present
- AVRT sections `RT_CODE`, `RT_CONST`, `RT_DATA` present
- no Creative x64 DLL imports

Offline native ARM64 COM probe:

`RESULT: PASS`

For SFX/MFX/EFX independently:

- class factory -> `S_OK`
- APO object creation -> `S_OK`
- QI APO/RT/Configuration/SystemEffects/SystemEffects2/SystemEffects3/Notifications -> PASS
- final `DllCanUnloadNow` -> `S_OK`

This proves isolated native ARM64 APO COM structure, not yet live AudioDG graph loading.

## ARM64 `usbaudio2` attachment runtime — PASSED

Canonical record:

`DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`

Live devnode:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

Confirmed:

- Class `MEDIA`
- Service `usbaudio2`
- Friendly `Sound Blaster X4`
- KSCATEGORY_AUDIO `msft_wave`
- KSCATEGORY_AUDIO `msft_topo`
- KSCATEGORY_TOPOLOGY `msft_topo`

`msft_topo` runtime categories include:

- `KSNODETYPE_SPEAKER`
- `KSNODETYPE_SPDIF_INTERFACE`
- `KSNODETYPE_MICROPHONE`
- `KSNODETYPE_LINE_CONNECTOR`
- `KSNODETYPE_DIGITAL_AUDIO_INTERFACE`

Read-only inspection found no `FX` or `EP` subtrees in the examined device/driver/audio-interface/topology-interface registry locations. Therefore the missing live Creative graph is an attachment/metadata problem, not an ARM64 APO DLL-load proof failure.

## MMDevice endpoint association runtime — PASSED

Canonical record:

`DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`

Six active X4 MMDevice endpoints were found:

| Flow | FormFactor | PKEY_AudioEndpoint_Association |
|---|---|---|
| Render | Speakers | GUID_NULL |
| Render | SPDIF | GUID_NULL |
| Capture | Microphone | GUID_NULL |
| Capture | UnknownDigitalPassthrough | GUID_NULL |
| Capture | SPDIF | GUID_NULL |
| Capture | LineLevel | GUID_NULL |

Every endpoint was active (`0x00000001`).

No distinct Headphones MMDevice exists in the current bare Microsoft `usbaudio2` state.

Consequences:

- do not equate Creative `FX\1` with KS pin 1;
- do not invent a Headphone endpoint;
- keep Headphone out of the first live gate;
- Speaker is the one exact common runtime/static attachment point for Stage A1.

## Current gate — Stage A1 Speaker-only package OFFLINE validation

Package directory:

`packaging/x4-apo-arm64-stage-a1-speaker`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

Workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Workflow file exists on `main` only for Actions UI discovery, but the workflow itself explicitly checks out:

`exp/windows-arm64-x4-native-controller`

### Stage A1 scope

Only Speaker is targeted.

The extension candidate:

- matches `USB\VID_041E&PID_3278&MI_03`;
- retains Microsoft `usbaudio2` as base function driver;
- reuses the proven KSCATEGORY_AUDIO topology reference string `msft_topo`;
- adds only Speaker `FX\0` metadata;
- uses `PKEY_FX_Association = KSNODETYPE_SPEAKER`;
- binds Stage A0 native ARM64 SFX/MFX/EFX CLSIDs;
- advertises only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT`;
- excludes Headphone/Mic/SPDIF/DDL/DSP/CTCDC/Creative filter replacements.

APO software-component identity:

`SWC\VEN_NPKR&CID_X4APO`

### Current workflow state

The first Stage A1 workflow attempt failed before compiling because bare `msbuild` was not on PATH.

A later workflow revision now resolves `MSBuild.exe` explicitly with `vswhere.exe` and executes the absolute path. Current workflow source reflects that fix.

The user subsequently reported that a DLL was produced from the updated Stage A1 workflow path. This is evidence that the build progressed past the previous `msbuild not recognized` failure.

However, **do not mark Stage A1 offline validation PASS yet**. No exact evidence has yet been supplied that both of these completed successfully in the same run:

- `InfVerif` on `X4ApoArm64.inf`
- `InfVerif` on `X4ApoSpeakerExtension.inf`

The workflow uploads the final offline artifact only after those checks. Before any installation work, confirm the current run reached the final upload step or otherwise obtain the exact InfVerif result.

## Immediate next action

1. Verify the latest fresh `Build X4 APO ARM64 Stage A1 Speaker Package` execution, not an old `Re-run jobs` attempt.
2. Confirm in that execution:
   - `Locate MSBuild` PASS
   - ARM64 APO build PASS
   - PE machine `0xAA64` PASS
   - `InfVerif APO component INF` PASS
   - `InfVerif Speaker extension INF` PASS
   - final offline artifact upload PASS
3. If InfVerif fails, fix only the exact diagnostic and rerun one gate.
4. If the full offline gate passes, then prepare a separate explicit signed/test-install + rollback package for Speaker-only pass-through.
5. First live runtime goal remains:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

No DSP/setters until that live gate passes.

## Safety / fixed exclusions

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live package install until Stage A1 offline validation is conclusively PASS
- no Headphone in A1
- no Microphone in A1
- no SPDIF/DDL
- no Creative DSP
- no Creative FX property writes
- no CTCDC writes
- no CTUSBWrap/DGFX
- no Creative UpperFilter replacement
- no blind `0x95`
- no generic `0x23`
- no unrelated changes
- no B5 ASIO changes
