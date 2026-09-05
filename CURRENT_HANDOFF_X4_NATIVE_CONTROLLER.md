# CURRENT HANDOFF — Sound Blaster X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Always verify actual branch HEAD before continuing. Keep this branch separate from B5 ASIO; do not modify B5 ASIO source, WaveRT engine, mux, failsafe or control-panel behavior here.

## Read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`
3. `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`
4. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
5. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
6. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
7. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
8. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
9. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
10. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
11. `packaging/x4-apo-arm64-review/README.md`

## Architecture boundary

Keep four backend classes distinct:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

## Fixed firmware / mixer facts

- CTCDC: `USB\VID_041E&PID_3278&MI_01`
- validated firmware `1.9.251008.0930`
- Direct Mode hardware-confirmed:
  - ON `5A 39 03 00 05 01`
  - OFF `5A 39 03 00 05 00`
- raw `0x96` exposed Graphic EQ only in tested session
- raw `0x95` no-response is not global CrystalVoice unsupported proof
- generic `0x23` probing must not be repeated
- CDC UInt16 engineering-unit conversion remains unresolved

Normal ARM64 Windows mixer path remains standard Core Audio / DeviceTopology:

- endpoint master/channel -> `IAudioEndpointVolume`
- monitoring volume/mute -> `IDeviceTopology/IPart + IAudioVolumeLevel/IAudioMute`
- Mic Boost -> `KSNODETYPE_VOLUME + IAudioVolumeLevel`
- Mic AGC -> `KSNODETYPE_AGC + IAudioAutoGainControl`

Do not port supplied x86 MalLgcy/CTAudEp for this subset.

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

## ARM64 APO Stage A0 — binary + COM gates PASSED

Source:

`src/x4-apo-arm64`

Validated DLL:

- size `176640`
- SHA-256 `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / `0xAA64`
- `DllGetClassObject`, `DllCanUnloadNow`
- exact official SFX/MFX/EFX CLSIDs present
- `RT_CODE`, `RT_CONST`, `RT_DATA` present
- no Creative x64 DLL imports

Offline native ARM64 COM probe result:

`RESULT: PASS`

For SFX/MFX/EFX independently:

- class factory -> `S_OK`
- APO object creation -> `S_OK`
- QI APO/RT/Configuration/SystemEffects/SystemEffects2/SystemEffects3/Notifications -> PASS
- final `DllCanUnloadNow` -> `S_OK`

This proves isolated native ARM64 APO COM structure but not live AudioDG graph loading.

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

This strongly matches the official SB1815 topology families and confirms that an X4 extension should continue to target `USB\VID_041E&PID_3278&MI_03` while retaining Microsoft `usbaudio2`.

### Current bare stack has no FX metadata

Read-only inspection found no `FX` or `EP` subtrees in:

- device hardware key
- driver/software key
- KSCATEGORY_AUDIO interface keys
- KSCATEGORY_TOPOLOGY interface key

Therefore the absence of live Creative effects is an attachment/endpoint-metadata issue, not an ARM64 APO DLL failure.

### Remaining blocking ambiguity — Headphone

No `KSNODETYPE_HEADPHONES` category appeared in the live KS topology dump.

The official Creative INF still has a separate `FX\1` Headphone binding.

Therefore:

- do not equate `FX\n` with KS pin number;
- do not assume `FX\1 = pin 1`;
- do not activate the review INF yet.

The Headphone endpoint association must be recovered from MMDevice endpoint properties / `PKEY_AudioEndpoint_Association` and compared against official `PKEY_FX_Association` semantics.

## Package review status — still NON-INSTALLING

Directory:

`packaging/x4-apo-arm64-review`

Files remain `.inx.review` and must not be installed.

The component review follows Windows 11 `Class=AudioProcessingObject` packaging. The extension review matches X4 MI_03 and records the recovered Speaker/Headphone/Microphone FX payload, but that payload is deliberately not wired into the install section yet.

## Immediate next action

1. Perform one more read-only MMDevice property dump for all X4 endpoints.
2. Identify at minimum:
   - `PKEY_AudioEndpoint_Association`
   - `PKEY_AudioEndpoint_FormFactor`
   - endpoint/device-interface identity
   - Speaker vs Headphone differentiation
   - Microphone association
3. Compare association values with official SB1815 `PKEY_FX_Association` data.
4. Only then finalize a pass-through-only live extension/APO package.
5. First live gate is only:

`PnP -> APO registration -> endpoint FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

No DSP or setters until that passes.

## Safety

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live APO install while Headphone association is unresolved
- no CTCDC writes in APO work
- no SPDIF/DDL in first milestone
- no blind `0x95`
- no generic `0x23`
- no unrelated changes
- no B5 ASIO changes
