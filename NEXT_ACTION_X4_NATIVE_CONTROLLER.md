# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Closed gates

### Stage A0 — PASSED

Native ARM64 `X4ApoArm64.dll` has passed:

- Release ARM64 build;
- PE machine `0xAA64` verification;
- export/CLSID/AVRT binary inspection;
- offline ARM64 COM class-factory/object/interface probe for SFX/MFX/EFX;
- final `DllCanUnloadNow == S_OK`.

### `usbaudio2` attachment discovery — PASSED

Read-only runtime evidence proves the X4 audio function is:

`USB\VID_041E&PID_3278&MI_03`

with:

- class `MEDIA`;
- service `usbaudio2`;
- KSCATEGORY_AUDIO `msft_wave`;
- KSCATEGORY_AUDIO `msft_topo`;
- KSCATEGORY_TOPOLOGY `msft_topo`.

The current bare `usbaudio2` interface/device keys contain no inspected `FX` or `EP` subtree.

Runtime `msft_topo` pin categories include Speaker, SPDIF, Microphone, Line and Digital Audio Interface.

### MMDevice endpoint association discovery — PASSED

Canonical record:

`DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`

Six active X4 endpoints were found:

- Render Speakers
- Render SPDIF
- Capture Microphone
- Capture UnknownDigitalPassthrough
- Capture SPDIF
- Capture LineLevel

All currently report:

`PKEY_AudioEndpoint_Association = GUID_NULL`

No separate Headphones MMDevice exists in the bare `usbaudio2` state.

Therefore:

- do not treat Creative `FX\1` as KS pin 1;
- do not invent a Headphone endpoint;
- keep Headphone out of the first package/runtime gate.

## Current gate — Stage A1 Speaker-only package OFFLINE validation

Canonical record:

`DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_SPEAKER_PACKAGE_OFFLINE_GATE.md`

Package directory:

`packaging/x4-apo-arm64-stage-a1-speaker`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

Manual workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow is `workflow_dispatch` only. The workflow file is exposed on `main` for GitHub Actions UI discovery, but the actual build explicitly checks out this controller branch.

### Stage A1 scope

Only the directly proven render Speaker path is targeted.

The extension:

- matches `USB\VID_041E&PID_3278&MI_03`;
- keeps Microsoft `usbaudio2` as base function driver;
- reuses `KSCATEGORY_AUDIO` reference string `msft_topo`;
- adds only `FX\0`;
- sets `PKEY_FX_Association = KSNODETYPE_SPEAKER`;
- binds Stage A0 native ARM64 SFX/MFX/EFX CLSIDs;
- advertises only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT`.

APO component identity:

`SWC\VEN_NPKR&CID_X4APO`

### Workflow state at handoff

The initial Stage A1 workflow failed before compilation because bare `msbuild` was not available on PATH.

Current workflow now:

1. finds Visual Studio with `vswhere.exe`;
2. resolves `MSBuild\Current\Bin\MSBuild.exe`;
3. invokes MSBuild by absolute path.

The user subsequently reported that the updated path produced a DLL.

This closes only the previous MSBuild-path failure. It does **not** yet prove the whole Stage A1 offline gate.

Do not mark Stage A1 PASS until the exact workflow evidence confirms both InfVerif steps and the final upload step.

## Immediate action in the next tab

First inspect/obtain the result of the latest **fresh** workflow execution of:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Do not confuse a historical `Re-run jobs` attempt with the current workflow definition.

Required step results:

1. `Locate MSBuild` -> PASS
2. `Build Stage A0 APO Release ARM64` -> PASS
3. `Stage package and verify ARM64 PE` -> PASS, machine `0xAA64`
4. `Locate WDK InfVerif` -> PASS
5. `InfVerif APO component INF` -> PASS
6. `InfVerif Speaker extension INF` -> PASS
7. `Upload offline Stage A1 package artifact` -> PASS

Artifact name when complete:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Expected artifact contents:

- `X4ApoArm64.dll`
- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

If InfVerif fails, fix only the exact INF diagnostic and rerun this offline gate. Do not expand scope.

## After Stage A1 offline PASS

Only then prepare an explicit signed/test-install package and rollback procedure for the Speaker-only pass-through runtime gate.

First live goal:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

Success criteria:

- existing Speaker endpoint remains functional;
- no AudioDG crash;
- no audio loss;
- native ARM64 APO loads in the live graph;
- pass-through remains transparent;
- rollback restores the original bare Microsoft `usbaudio2` state.

## Fixed exclusions

- no Headphone in A1
- no Microphone in A1
- no SPDIF/DDL
- no Creative DSP
- no Creative FX property writes
- no CTCDC writes
- no CTUSBWrap/DGFX
- no Creative UpperFilter
- no B5 ASIO changes

## Safety

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live package install before Stage A1 offline validation is conclusively PASS
- no unrelated changes
