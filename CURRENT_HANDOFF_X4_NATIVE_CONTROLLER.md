# CURRENT HANDOFF — Sound Blaster X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

## Source of truth

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Verified implementation baseline immediately before this handoff refresh:

`97f59a80ad540eb67537190f19afffdea61cd720`

Always fetch the actual branch HEAD before continuing because handoff-document updates advance HEAD.

Do not infer state from previous chat memory.

## Mandatory read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
3. `DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_OFFLINE_PASS_STAGE_A2_SIGNED_LIVE_PREP.md`
4. `DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_SPEAKER_PACKAGE_OFFLINE_GATE.md`
5. `DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`
7. `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`
8. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
9. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
10. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
11. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
12. `packaging/x4-apo-arm64-stage-a2-speaker-live/README.md`
13. `packaging/x4-apo-arm64-stage-a1-speaker/README.md`

## Fixed architecture boundary

Keep these backend classes separate:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not modify B5 ASIO source, WaveRT engine, mux, failsafe, B5 control-panel behavior, or unrelated paths from this workstream.

Do not repeat excluded BLE/HID/UAC Extension Unit/vendor-interface/random raw probing.

Do not generic-probe CTCDC `0x23` again.

Do not interpret raw VoiceInputManager `0x95` no-response as global CrystalVoice unsupported proof.

Do not hard-code CDC `/256`; engineering-unit conversion remains unresolved.

## Closed gates

### Stage A0 native ARM64 APO — PASS

`X4ApoArm64.dll` already passed native ARM64 binary and offline COM gates for the official X4 SFX/MFX/EFX CLSIDs.

### Microsoft usbaudio2 attachment — PASS

Live X4 audio function:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

Confirmed service:

`usbaudio2`

Confirmed `KSCATEGORY_AUDIO` interfaces:

- `msft_wave`
- `msft_topo`

The bare stack contains no inspected Creative `FX`/`EP` metadata.

### MMDevice endpoint association — PASS

Six active X4 endpoints were observed and all report:

`PKEY_AudioEndpoint_Association = GUID_NULL`

No separate Headphones MMDevice exists in the bare Microsoft `usbaudio2` state.

Speaker remains the first exact runtime/static common attachment point.

### Stage A1 Speaker-only OFFLINE package — PASS

Fresh workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Run ID:

`33958338454`

Head SHA:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

Result:

`success`

Confirmed in the same fresh run:

- `Locate MSBuild` PASS
- Release ARM64 APO build PASS
- PE machine `0xAA64` PASS
- `InfVerif X4ApoArm64.inf` -> `INF is VALID`
- `InfVerif X4ApoSpeakerExtension.inf` -> `INF is VALID`
- final artifact upload PASS

Fresh rebuilt unsigned DLL SHA-256:

`5007E95F32983A4572D406671154B6417612D6FEC6F71C10012942A9AA5501A5`

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Artifact ZIP SHA-256:

`e719629a81690477e1f672b9b9d4e366aeb71e5b465934920d276c037ce6f6b0`

Stage A1 is conclusively closed PASS.

## Current gate — Stage A2 signed Speaker-only live-test package

Directory:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `X4ApoStageA2.ps1`
- `README.md`

Workflow:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

Workflow file:

`.github/workflows/build-x4-apo-arm64-stage-a2-speaker-live.yml`

The workflow is `workflow_dispatch` only. It is exposed on `main` for Actions UI discovery and explicitly checks out:

`exp/windows-arm64-x4-native-controller`

### Stage A2 scope

Stage A2 changes only deployment/signing state. It does not expand endpoint or DSP scope.

Unchanged invariants:

- target HWID `USB\VID_041E&PID_3278&MI_03`
- keep Microsoft `usbaudio2`
- reuse `KSCATEGORY_AUDIO\msft_topo`
- Speaker `FX\0` only
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`
- exact native ARM64 SFX/MFX/EFX CLSIDs
- default processing mode only
- pass-through audio only
- APO component `SWC\VEN_NPKR&CID_X4APO`

Excluded:

- Headphone
- Microphone
- SPDIF / DDL
- Creative DSP
- Creative FX property writes
- CTCDC writes
- CTUSBWrap / CTUSBDGFX
- Creative UpperFilter / CTUSBfilt
- B5 ASIO changes

### Stage A2 workflow design

A fresh Stage A2 run must prove:

1. Release ARM64 build PASS
2. PE `0xAA64` PASS
3. `X4ApoStageA2.ps1` PowerShell parser PASS
4. both A2 `InfVerif` checks PASS
5. ephemeral dedicated test certificate generation PASS
6. `X4ApoArm64.dll` signing PASS
7. `Inf2Cat` ARM64 catalog generation PASS
8. both catalog signatures PASS
9. final exact 8-file artifact upload PASS

Expected artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A2-Speaker-LIVE-TEST`

Expected files:

- `X4ApoArm64.dll`
- `X4ApoArm64.inf`
- `X4ApoArm64.cat`
- `X4ApoSpeakerExtension.inf`
- `X4ApoSpeakerExtension.cat`
- `X4ApoStageA2Test.cer`
- `X4ApoStageA2.ps1`
- `README.md`

The signing private key is not uploaded.

## Live-test runner

`X4ApoStageA2.ps1` exposes only:

- `Preflight`
- `Install`
- `Verify`
- `Rollback`

It does not change Secure Boot or BCD test-signing state.

Install is gated on:

- ARM64 host
- exactly one present X4 MI_03
- base service still `usbaudio2`
- Secure Boot disabled
- Windows TESTSIGNING enabled
- package signer matches the supplied dedicated test certificate
- no prior Stage A2 package with the same INF original names already in Driver Store

Install sequence:

`test certificate -> APO component INF -> Speaker Extension INF -> PnP rescan -> record exact oem*.inf -> verify usbaudio2 invariant`

No manual FX registry write is performed.

Rollback sequence:

`remove exact Extension oem*.inf -> remove exact APO oem*.inf -> rescan/restart X4 MI_03 -> remove dedicated test certificate -> verify usbaudio2`

## Immediate next action

The Stage A2 workflow has not yet been executed.

Do not install anything yet.

Run one fresh manual workflow execution of:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

Then inspect the exact run. Only if all Stage A2 build/signing/catalog/artifact gates pass may the test machine run:

`X4ApoStageA2.ps1 -Action Preflight`

and then, if Preflight is PASS:

`X4ApoStageA2.ps1 -Action Install`

First live success target remains:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG load -> transparent Speaker audio`

No DSP or property setters until this live pass-through gate succeeds.
