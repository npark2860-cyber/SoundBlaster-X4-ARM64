# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Closed gates

### Stage A0 — PASS

Native ARM64 pass-through APO binary and offline COM gates are closed.

### usbaudio2 attachment — PASS

X4 audio HWID:

`USB\VID_041E&PID_3278&MI_03`

Base service:

`usbaudio2`

Runtime topology interface:

`KSCATEGORY_AUDIO\msft_topo`

### MMDevice association — PASS

Six active X4 endpoints were observed; all report `PKEY_AudioEndpoint_Association = GUID_NULL`.

No separate Headphones MMDevice exists in the bare Microsoft stack.

### Stage A1 Speaker-only OFFLINE package — PASS

Fresh workflow run:

`33958338454`

All required steps passed in one run, including both InfVerif checks and final artifact upload.

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Canonical record:

`DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_OFFLINE_PASS_STAGE_A2_SIGNED_LIVE_PREP.md`

## Current gate — Stage A2 signed Speaker-only live-test package

Package directory:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Workflow:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

The workflow is `workflow_dispatch` only and explicitly checks out the controller branch.

Stage A2 retains the Stage A1 attachment model unchanged:

- `USB\VID_041E&PID_3278&MI_03`
- Microsoft `usbaudio2`
- `KSCATEGORY_AUDIO\msft_topo`
- Speaker `FX\0` only
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`
- native ARM64 SFX/MFX/EFX
- default processing mode only
- pass-through only

No Headphone, Mic, SPDIF/DDL, Creative DSP, Creative FX setter, CTCDC write, Creative filter replacement or B5 ASIO work is included.

## Immediate action

Run one fresh manual execution of:

`Build X4 APO ARM64 Stage A2 Speaker Live Test`

Required PASS steps:

1. `Locate MSBuild`
2. `Build Stage A0 APO Release ARM64`
3. `Stage A2 package and verify ARM64 PE`
4. `Parse Stage A2 PowerShell runner`
5. `Locate WDK validation and signing tools`
6. `InfVerif APO component INF`
7. `InfVerif Speaker extension INF`
8. `Create ephemeral test certificate, sign DLL, build and sign catalogs`
9. `Verify final Stage A2 artifact file set`
10. `Upload signed Stage A2 Speaker live-test artifact`

Expected artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A2-Speaker-LIVE-TEST`

Expected exact files:

- `X4ApoArm64.dll`
- `X4ApoArm64.inf`
- `X4ApoArm64.cat`
- `X4ApoSpeakerExtension.inf`
- `X4ApoSpeakerExtension.cat`
- `X4ApoStageA2Test.cer`
- `X4ApoStageA2.ps1`
- `README.md`

If any Stage A2 workflow step fails, fix only that exact build/signing/catalog diagnostic. Do not install and do not expand endpoint scope.

## After Stage A2 workflow PASS

Run from an elevated PowerShell in the extracted artifact directory:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Preflight`

Preflight must confirm:

- native ARM64 host
- exactly one X4 MI_03 present
- base service `usbaudio2`
- Secure Boot disabled
- Windows TESTSIGNING enabled
- valid dedicated Stage A2 signer/certificate match
- no previous Stage A2 package already in Driver Store

Only after Preflight PASS:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Install`

Then:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Verify`

First live target:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG load -> transparent Speaker audio`

If the live gate fails or audio becomes unstable, use:

`powershell -ExecutionPolicy Bypass -File .\X4ApoStageA2.ps1 -Action Rollback`

Rollback removes only the recorded Stage A2 packages/certificate and verifies the X4 MI_03 base service returns/remains `usbaudio2`.

## Fixed safety boundary

- no manual FX registry writes
- no `regsvr32`
- no live install before Stage A2 signed-package workflow PASS
- no automatic Secure Boot/BCD changes
- no Headphone
- no Microphone
- no SPDIF/DDL
- no Creative DSP
- no Creative FX property writes
- no CTCDC writes
- no CTUSBWrap/DGFX
- no Creative UpperFilter
- no B5 ASIO changes
