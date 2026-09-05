# X4 ARM64 APO Stage A1 — Speaker-only offline package gate

This directory contains the first componentized APO package candidate after the Stage A0 native ARM64 DLL, offline COM, `usbaudio2` attachment and MMDevice runtime gates passed.

## Scope

Stage A1 targets **Speaker only**.

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

Therefore Stage A1 does not guess a Headphone association.

## Offline workflow

Manual workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow is exposed on `main` for Actions UI discovery and explicitly checks out:

`exp/windows-arm64-x4-native-controller`

## Final status — PASS

Fresh workflow run:

`33958338454`

Head SHA:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

The same fresh execution passed:

1. Release ARM64 build;
2. PE machine `0xAA64` verification;
3. WDK InfVerif discovery;
4. `InfVerif` on `X4ApoArm64.inf`;
5. `InfVerif` on `X4ApoSpeakerExtension.inf`;
6. final artifact upload.

Both INF logs report:

`INF is VALID`

Fresh rebuilt unsigned DLL SHA-256:

`5007E95F32983A4572D406671154B6417612D6FEC6F71C10012942A9AA5501A5`

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Artifact ZIP SHA-256:

`e719629a81690477e1f672b9b9d4e366aeb71e5b465934920d276c037ce6f6b0`

Stage A1 is therefore closed **PASS**.

## Next gate

Do not install the Stage A1 offline artifact directly.

The separate signed/test-install + rollback package is Stage A2:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Its workflow must pass before any live installation is attempted.
