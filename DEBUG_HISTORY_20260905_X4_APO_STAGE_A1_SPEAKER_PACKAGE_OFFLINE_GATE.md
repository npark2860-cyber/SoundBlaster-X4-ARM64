# DEBUG HISTORY — 2026-09-05 X4 APO Stage A1 Speaker Package Offline Gate

Branch:

`exp/windows-arm64-x4-native-controller`

## Purpose

Record the transition from the fully read-only Stage A0/attachment/MMDevice gates to the first package candidate intended only for **offline validation**.

No live APO package installation is authorized by this document.

## Evidence entering Stage A1

Previous gates established:

1. Native ARM64 `X4ApoArm64.dll` binary validation PASS.
2. Offline ARM64 COM object/interface probe PASS for official X4 SFX/MFX/EFX CLSIDs.
3. Live X4 audio function is `USB\VID_041E&PID_3278&MI_03` with Microsoft `usbaudio2`.
4. Existing KSCATEGORY_AUDIO interfaces include `msft_wave` and `msft_topo`.
5. `msft_topo` runtime pin categories include Speaker, SPDIF, Microphone, Line and Digital Audio Interface.
6. Bare `usbaudio2` registry/interface inspection contains no existing `FX` or `EP` subtree in the inspected locations.
7. Six active X4 MMDevice endpoints exist; all have `PKEY_AudioEndpoint_Association = GUID_NULL`.
8. The bare stack has an active Render Speakers endpoint but no separate Headphones MMDevice.

Therefore the first package candidate was deliberately reduced to Speaker only.

## Stage A1 package

Directory:

`packaging/x4-apo-arm64-stage-a1-speaker`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

The native DLL is rebuilt from:

`src/x4-apo-arm64`

### Speaker-only attachment model

The extension candidate:

- targets `USB\VID_041E&PID_3278&MI_03`;
- keeps Microsoft `usbaudio2` as the base function driver;
- reuses the proven KSCATEGORY_AUDIO reference string `msft_topo`;
- adds only `FX\0` metadata;
- uses `PKEY_FX_Association = KSNODETYPE_SPEAKER`;
- binds official X4 SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`;
- binds official X4 MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`;
- binds official X4 EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`;
- advertises only `AUDIO_SIGNALPROCESSINGMODE_DEFAULT`.

Software-component identity:

`SWC\VEN_NPKR&CID_X4APO`

Excluded from A1:

- Headphone
- Microphone
- SPDIF / DDL
- Line In
- What-U-Hear
- Creative DSP algorithms
- Creative FX property writes
- CTCDC writes
- CTUSBWrap / CTUSBDGFX
- Creative UpperFilter / CTUSBfilt replacement

## GitHub Actions workflow

Workflow name:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Workflow file:

`.github/workflows/build-x4-apo-arm64-stage-a1-speaker-package.yml`

`workflow_dispatch` only.

The file is also present on default branch `main` so GitHub Actions exposes the manual workflow in the UI. The actual build step explicitly checks out:

`exp/windows-arm64-x4-native-controller`

## Workflow failure history

### Failure 1 — MSBuild command not found

Observed error:

`The term 'msbuild' is not recognized as a name of a cmdlet, function, script file, or executable program.`

This was a workflow environment/path problem, not an APO source compile failure.

An intermediate revision added `microsoft/setup-msbuild@v2`.

A subsequent observed run still showed the old bare `msbuild` command. Because the exact workflow-run metadata was not retrieved, do not record the reason for that second observation as fact.

### Final workflow fix

The workflow was changed to avoid PATH/setup-action dependence:

1. locate Visual Studio with `vswhere.exe`;
2. derive `MSBuild\Current\Bin\MSBuild.exe`;
3. verify the file exists;
4. invoke that absolute path directly.

Code/workflow baseline containing this fix:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

## Latest runtime/build report at handoff

After the explicit-`vswhere` workflow fix, the user reported:

`DLL 나온`

This is sufficient to conclude only that the updated workflow progressed beyond the previous `msbuild not recognized` failure far enough to produce an APO DLL.

It is **not** sufficient evidence to conclude Stage A1 offline validation PASS.

The following remain unconfirmed in the supplied evidence:

- `InfVerif APO component INF` PASS;
- `InfVerif Speaker extension INF` PASS;
- final offline artifact upload PASS;
- exact SHA-256 of the newly rebuilt Stage A1 DLL in that run.

Do not infer those results from DLL production alone.

## Required completion gate

Stage A1 offline validation is PASS only when one fresh workflow execution proves all of the following:

1. `Locate MSBuild` PASS;
2. Release ARM64 APO build PASS;
3. PE machine check `0xAA64` PASS;
4. `InfVerif` on `X4ApoArm64.inf` PASS;
5. `InfVerif` on `X4ApoSpeakerExtension.inf` PASS;
6. `SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE` artifact upload PASS.

If InfVerif fails, fix only the exact diagnostic. Do not change the attachment scope or add other endpoints while resolving packaging syntax/validation errors.

## Next live gate after offline PASS

Only after the full offline gate passes, prepare a separate explicit test-install and rollback procedure for:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

The live test remains Speaker-only and pass-through-only.

## Safety

- no installation from this offline record
- no manual FX registry writes
- no `regsvr32`
- no Headphone/Mic/SPDIF expansion in A1
- no Creative DSP/setters
- no CTCDC writes
- no B5 ASIO changes
