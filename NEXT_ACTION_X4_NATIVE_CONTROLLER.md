# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Stage A0 — PASSED

Native ARM64 `X4ApoArm64.dll` has passed:

- Release ARM64 build;
- PE machine `0xAA64` verification;
- export/CLSID/AVRT binary inspection;
- offline ARM64 COM class-factory/object/interface probe for SFX/MFX/EFX;
- final `DllCanUnloadNow == S_OK`.

## usbaudio2 attachment discovery — PASSED

Read-only runtime evidence proves the X4 audio function is:

`USB\VID_041E&PID_3278&MI_03`

with:

- class `MEDIA`;
- service `usbaudio2`;
- KSCATEGORY_AUDIO `msft_wave`;
- KSCATEGORY_AUDIO `msft_topo`;
- KSCATEGORY_TOPOLOGY `msft_topo`.

The current bare `usbaudio2` interface/device keys contain no `FX` or `EP` subtree.

Runtime `msft_topo` pin categories include Speaker, SPDIF, Microphone, Line and Digital Audio Interface.

## MMDevice endpoint association discovery — PASSED

Canonical record:

`DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`

Six active X4 endpoints were found:

- Render Speakers
- Render SPDIF
- Capture Microphone
- Capture UnknownDigitalPassthrough
- Capture SPDIF
- Capture LineLevel

Every endpoint currently reports:

`PKEY_AudioEndpoint_Association = GUID_NULL`

No separate Headphones MMDevice exists in the bare `usbaudio2` state.

Therefore:

- do not treat Creative `FX\1` as KS pin 1;
- do not invent a Headphone endpoint;
- keep Headphone out of the first live package gate.

## Current gate — Stage A1 Speaker-only package OFFLINE validation

Package directory:

`packaging/x4-apo-arm64-stage-a1-speaker`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

Manual workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow is `workflow_dispatch` only and is exposed from `main` while checking out this controller branch.

### Stage A1 scope

Only the directly proven render Speaker path is targeted.

The extension:

- matches `USB\VID_041E&PID_3278&MI_03`;
- creates component identity `VEN_NPKR&CID_X4APO`;
- reuses the proven KSCATEGORY_AUDIO reference string `msft_topo`;
- adds only `FX\0`;
- sets `PKEY_FX_Association = KSNODETYPE_SPEAKER`;
- binds the native Stage A0 SFX/MFX/EFX CLSIDs;
- advertises only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT`.

The APO component INF targets:

`SWC\VEN_NPKR&CID_X4APO`

and registers the three native ARM64 COM/APO classes with the Windows 11 `AudioProcessingObject` class model.

### Required offline gate

The workflow must:

1. rebuild `X4ApoArm64.dll` Release ARM64;
2. verify machine `0xAA64`;
3. run WDK `InfVerif` on `X4ApoArm64.inf`;
4. run WDK `InfVerif` on `X4ApoSpeakerExtension.inf`;
5. upload the artifact only if all checks pass.

Do **not** install the Stage A1 artifact yet.

If `InfVerif` fails, fix only the exact INF diagnostics and rerun this gate.

## After Stage A1 InfVerif PASS

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
- no live package install before the Stage A1 InfVerif gate passes
- no unrelated changes
