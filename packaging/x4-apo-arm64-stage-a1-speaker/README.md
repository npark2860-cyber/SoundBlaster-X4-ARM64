# X4 ARM64 APO Stage A1 — Speaker-only offline package gate

This directory contains the first package candidate after the Stage A0 native ARM64 DLL, offline COM, usbaudio2 attachment and MMDevice runtime gates passed.

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
- bare `usbaudio2` has no FX metadata;
- all current MMDevice `PKEY_AudioEndpoint_Association` values are GUID_NULL;
- no separate Headphones MMDevice exists.

Therefore the first live graph-loading test must not guess the Creative Headphone association. Speaker is the one exact common runtime/static attachment point.

## Current gate

**Do not install these files yet.**

Run the manual GitHub Actions workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow must first:

1. build `X4ApoArm64.dll` Release ARM64;
2. verify PE machine `0xAA64`;
3. run WDK `InfVerif` against both INFs;
4. stage an offline artifact.

Only after the offline INF gate passes should an explicit signed/test-install and rollback procedure be prepared.
