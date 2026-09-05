# HANDOFF PROMPT — Sound Blaster X4 Native Controller / ARM64 APO

Use this file to continue the X4 Windows ARM64 native-controller/APO work without reconstructing prior conversation from memory.

## Repository / branch

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Pre-refresh verified implementation baseline:

`97f59a80ad540eb67537190f19afffdea61cd720`

Always fetch the actual current branch HEAD before changing anything because handoff-document commits advance HEAD.

GitHub is the source of truth.

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

## Fixed project boundaries

Keep these backend classes separate:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not modify B5 ASIO source, WaveRT engine, mux, failsafe, B5 control-panel behavior, or unrelated paths.

Do not repeat excluded BLE/HID/UAC Extension Unit/vendor-interface/random raw probing.

Do not generic-probe CTCDC `0x23` again.

Do not interpret raw VoiceInputManager `0x95` no-response as global CrystalVoice unsupported proof.

Do not hard-code CDC `/256`; engineering-unit conversion remains unresolved.

## Closed gates

### Stage A0 native ARM64 APO — PASS

Native ARM64 pass-through SFX/MFX/EFX binary and isolated COM gates are closed PASS.

### Microsoft usbaudio2 attachment — PASS

Live X4 audio HWID:

`USB\VID_041E&PID_3278&MI_03`

Base service:

`usbaudio2`

Existing topology interface:

`KSCATEGORY_AUDIO\msft_topo`

### MMDevice association — PASS

Six active X4 endpoints were observed; all have `PKEY_AudioEndpoint_Association = GUID_NULL`.

No distinct Headphones MMDevice exists in the bare Microsoft stack.

### Stage A1 Speaker-only OFFLINE package — PASS

Fresh workflow run:

`33958338454`

Head SHA:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

All required build, PE, InfVerif and upload steps succeeded.

Fresh rebuilt unsigned DLL SHA-256:

`5007E95F32983A4572D406671154B6417612D6FEC6F71C10012942A9AA5501A5`

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Artifact ZIP SHA-256:

`e719629a81690477e1f672b9b9d4e366aeb71e5b465934920d276c037ce6f6b0`

Do not re-open Stage A1 unless new evidence invalidates this exact run.

## Current work — Stage A2 signed Speaker-only live-test package

Package directory:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Workflow:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

The workflow is `workflow_dispatch` only. It is exposed on `main` for Actions UI discovery and explicitly checks out the controller branch.

Stage A2 keeps the Stage A1 technical scope unchanged:

- target `USB\VID_041E&PID_3278&MI_03`
- retain Microsoft `usbaudio2`
- reuse `KSCATEGORY_AUDIO\msft_topo`
- Speaker `FX\0` only
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`
- exact native ARM64 SFX/MFX/EFX CLSIDs
- default processing mode only
- pass-through processing only
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

## Stage A2 package design

A fresh Stage A2 workflow is expected to:

1. build the native ARM64 pass-through DLL;
2. verify PE machine `0xAA64`;
3. parse-check `X4ApoStageA2.ps1`;
4. run InfVerif on both INFs;
5. create an ephemeral dedicated test code-signing certificate;
6. sign `X4ApoArm64.dll`;
7. run Inf2Cat for ARM64;
8. sign both catalogs;
9. verify the signatures;
10. upload exactly 8 files.

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

The private signing key must not be uploaded.

## IMPORTANT — Stage A2 is NOT yet PASS

The Stage A2 workflow has not yet been executed.

Do not install anything until one fresh Stage A2 run proves all build/signing/catalog/artifact steps PASS.

The first action in the next tab is to inspect the latest fresh Stage A2 workflow result if the user has run it. If no run exists yet, the required user action is to manually dispatch:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

Do not substitute an old run or a different workflow.

## After Stage A2 workflow PASS

Only then run the signed artifact on the ARM64 X4 test machine.

First command, elevated PowerShell:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Preflight`

Preflight must PASS before installation.

Only then:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Install`

Then:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Verify`

Rollback command:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Rollback`

The script does not automatically change Secure Boot or BCD TESTSIGNING state.

## First live success target

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG load -> transparent Speaker audio`

No DSP or property setters until this live pass-through gate succeeds.

## Safety

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live install before Stage A2 signed-package workflow PASS
- no automatic Secure Boot/BCD changes
- no new hardware state changes automatically
- no Headphone/Mic/SPDIF expansion
- no Creative DSP/setters
- no CTCDC writes
- no B5 ASIO changes
