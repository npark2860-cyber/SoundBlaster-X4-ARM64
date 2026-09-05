# DEBUG HISTORY — 2026-09-05 X4 APO Stage A1 Speaker Package Offline Gate

Branch:

`exp/windows-arm64-x4-native-controller`

## Purpose

Record the first Speaker-only componentized APO package offline gate and its final completion result.

No live APO package installation was performed by Stage A1.

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

Therefore Stage A1 was deliberately reduced to Speaker only.

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

Workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Workflow file:

`.github/workflows/build-x4-apo-arm64-stage-a1-speaker-package.yml`

`workflow_dispatch` only.

The workflow is exposed on `main` for Actions UI discovery and explicitly checks out:

`exp/windows-arm64-x4-native-controller`

## Failure history

### Initial failure — MSBuild command not found

Observed error:

`The term 'msbuild' is not recognized as a name of a cmdlet, function, script file, or executable program.`

This was a workflow PATH/environment failure, not an APO source compile failure.

The final fix changed the workflow to:

1. locate Visual Studio with `vswhere.exe`;
2. resolve `MSBuild\Current\Bin\MSBuild.exe`;
3. invoke MSBuild by absolute path.

Baseline containing that fix:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

## Final fresh workflow result — PASS

Fresh run ID:

`33958338454`

Run attempt:

`1`

Head SHA:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

Conclusion:

`success`

The same fresh execution proved all required gates:

1. `Locate MSBuild` PASS
2. `Build Stage A0 APO Release ARM64` PASS
3. `Stage package and verify ARM64 PE` PASS
4. `Locate WDK InfVerif` PASS
5. `InfVerif APO component INF` PASS
6. `InfVerif Speaker extension INF` PASS
7. `Upload offline Stage A1 package artifact` PASS

Workflow log details:

- ARM64 PE machine check accepted `0xAA64`;
- rebuilt DLL SHA-256 `5007E95F32983A4572D406671154B6417612D6FEC6F71C10012942A9AA5501A5`;
- `X4ApoArm64.inf` -> `INF is VALID`;
- `X4ApoSpeakerExtension.inf` -> `INF is VALID`;
- exactly four files were uploaded.

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Artifact ZIP SHA-256:

`e719629a81690477e1f672b9b9d4e366aeb71e5b465934920d276c037ce6f6b0`

## Gate result

Stage A1 Speaker-only OFFLINE validation is conclusively **PASS**.

Do not reopen the earlier MSBuild-path or InfVerif uncertainty unless new evidence contradicts run `33958338454`.

## Next gate

The next gate is the separately prepared signed/test-install + rollback Stage A2 package:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Canonical transition record:

`DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_OFFLINE_PASS_STAGE_A2_SIGNED_LIVE_PREP.md`

First live target remains:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG load -> transparent audio`

No DSP, property setters, Headphone, Mic or SPDIF work is authorized before that live Speaker pass-through gate succeeds.
