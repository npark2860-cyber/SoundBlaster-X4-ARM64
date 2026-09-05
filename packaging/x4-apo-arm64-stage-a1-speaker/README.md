# X4 ARM64 APO Stage A1 — Speaker-only offline package gate

This directory contains the first package candidate after the Stage A0 native ARM64 DLL, offline COM, `usbaudio2` attachment and MMDevice runtime gates passed.

## Scope

Stage A1 intentionally targets **Speaker only**.

Included:

- X4 audio hardware ID `USB\VID_041E&PID_3278&MI_03`
- existing Microsoft `usbaudio2` function driver remains the base path
- existing KSCATEGORY_AUDIO topology reference string `msft_topo`
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`
- native ARM64 Stage A0 SFX/MFX/EFX CLSIDs
- default signal-processing mode only
- Windows 11 componentized APO registration

Excluded:

- Headphone
- Microphone
- SPDIF / DDL
- Line In
- What-U-Hear
- Creative DSP algorithms
- Creative FX setters
- CTCDC writes
- Creative UpperFilter / CTUSBfilt
- CTUSBWrap / CTUSBDGFX

## Files

- `X4ApoArm64.inf` — AudioProcessingObject-class software-component driver for `SWC\VEN_NPKR&CID_X4APO`
- `X4ApoSpeakerExtension.inf` — X4 MI_03 extension; creates the APO software component and adds Speaker-only FX metadata to the existing `KSCATEGORY_AUDIO\msft_topo` interface

`X4ApoArm64.dll` is staged into the build artifact by GitHub Actions from `src/x4-apo-arm64`.

## Why Speaker only

Read-only runtime evidence on the ARM64 test machine proved:

- Render Speakers MMDevice exists and is active;
- its FormFactor is `Speakers`;
- X4 `msft_topo` exposes `KSNODETYPE_SPEAKER`;
- bare `usbaudio2` has no inspected FX metadata;
- all current MMDevice `PKEY_AudioEndpoint_Association` values are GUID_NULL;
- no separate Headphones MMDevice exists.

Therefore the first live graph-loading test must not guess the Creative Headphone association. Speaker is the one exact common runtime/static attachment point.

## Offline workflow

Manual workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow file is exposed on `main` only so GitHub Actions can list the manual workflow. The workflow itself explicitly checks out:

`exp/windows-arm64-x4-native-controller`

Current workflow resolves `MSBuild.exe` with `vswhere.exe` and invokes the absolute path rather than relying on `msbuild` being present on PATH.

The workflow must:

1. build `X4ApoArm64.dll` Release ARM64;
2. verify PE machine `0xAA64`;
3. locate WDK `InfVerif.exe`;
4. run `InfVerif` against `X4ApoArm64.inf`;
5. run `InfVerif` against `X4ApoSpeakerExtension.inf`;
6. upload `SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE` only if all prior checks pass.

## Status at 2026-09-05 handoff

The earlier workflow failure `msbuild is not recognized` has been addressed by explicit `vswhere`/absolute-path MSBuild resolution.

The user subsequently reported that a DLL was produced by the updated workflow path. This proves progress past the previous MSBuild-path failure only.

**Stage A1 offline validation is not yet recorded as PASS.** Exact evidence for both InfVerif steps and the final artifact-upload step has not yet been supplied in the handoff state.

Do not infer InfVerif success merely from DLL creation.

## Next gate

**Do not install these files yet.**

First confirm one fresh workflow execution has passed:

- `Locate MSBuild`
- ARM64 build
- PE `0xAA64` check
- `InfVerif APO component INF`
- `InfVerif Speaker extension INF`
- final artifact upload

Only after the complete offline INF gate passes should an explicit signed/test-install and rollback procedure be prepared.
