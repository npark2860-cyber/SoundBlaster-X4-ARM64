# DEBUG HISTORY — 2026-09-05 X4 APO Stage A1 Offline PASS / Stage A2 Signed Live-Test Preparation

Branch:

`exp/windows-arm64-x4-native-controller`

## Stage A1 offline gate — PASS

Fresh workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

Run ID:

`33958338454`

Workflow head SHA:

`e0d5565e1ae0e2f77fd141148dd9f14019588117`

Run conclusion:

`success`

The single job completed all required steps successfully:

1. `Locate MSBuild` PASS
2. `Build Stage A0 APO Release ARM64` PASS
3. `Stage package and verify ARM64 PE` PASS
4. `Locate WDK InfVerif` PASS
5. `InfVerif APO component INF` PASS
6. `InfVerif Speaker extension INF` PASS
7. `Upload offline Stage A1 package artifact` PASS

The workflow log explicitly reports:

- ARM64 PE check accepted machine `0xAA64`;
- rebuilt DLL SHA-256 `5007E95F32983A4572D406671154B6417612D6FEC6F71C10012942A9AA5501A5`;
- `X4ApoArm64.inf` -> `INF is VALID`;
- `X4ApoSpeakerExtension.inf` -> `INF is VALID`;
- exactly 4 files uploaded.

Artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Artifact ID:

`9967105545`

Artifact ZIP SHA-256 digest:

`e719629a81690477e1f672b9b9d4e366aeb71e5b465934920d276c037ce6f6b0`

Therefore Stage A1 Speaker-only OFFLINE package validation is conclusively **PASS**.

## Stage A2 purpose

Stage A2 prepares the first explicit live Speaker-only pass-through package without changing the validated DSP implementation or expanding endpoint scope.

Directory:

`packaging/x4-apo-arm64-stage-a2-speaker-live`

Files committed:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `X4ApoStageA2.ps1`
- `README.md`

Workflow:

`.github/workflows/build-x4-apo-arm64-stage-a2-speaker-live.yml`

The workflow is `workflow_dispatch` only. A copy is exposed on `main` for GitHub Actions UI discovery; the workflow explicitly checks out the controller branch.

## Stage A2 invariants

Unchanged from Stage A1:

- target `USB\VID_041E&PID_3278&MI_03` only;
- Microsoft `usbaudio2` remains the base function driver;
- existing `KSCATEGORY_AUDIO\msft_topo` interface;
- Speaker `FX\0` only;
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`;
- official X4 SFX/MFX/EFX CLSIDs;
- default processing mode only;
- native ARM64 pass-through APO;
- no Creative DSP or property setters.

Still excluded:

- Headphone;
- Microphone;
- SPDIF / DDL;
- Creative UpperFilter / CTUSBfilt;
- CTUSBWrap / CTUSBDGFX;
- CTCDC writes;
- B5 ASIO changes.

## Signed test-package design

The Stage A2 workflow is intended to:

1. rebuild the same native ARM64 pass-through DLL;
2. verify PE machine `0xAA64`;
3. parse-check `X4ApoStageA2.ps1`;
4. run `InfVerif` on both A2 INFs;
5. generate an ephemeral self-signed code-signing certificate;
6. export only the public `.cer` file;
7. Authenticode-sign `X4ApoArm64.dll`;
8. run `Inf2Cat` for Windows 11 ARM64 (`10_GE_ARM64`);
9. sign both generated catalogs;
10. verify the DLL/catalog signatures;
11. upload exactly 8 files.

Expected artifact:

`SoundBlaster-X4-APO-ARM64-Stage-A2-Speaker-LIVE-TEST`

Expected contents:

- `X4ApoArm64.dll`
- `X4ApoArm64.inf`
- `X4ApoArm64.cat`
- `X4ApoSpeakerExtension.inf`
- `X4ApoSpeakerExtension.cat`
- `X4ApoStageA2Test.cer`
- `X4ApoStageA2.ps1`
- `README.md`

The private signing key is never uploaded.

## Stage A2 runner behavior

`X4ApoStageA2.ps1` supports four explicit actions:

- `Preflight`
- `Install`
- `Verify`
- `Rollback`

The script does not change Secure Boot or BCD test-signing state.

Installation is blocked unless:

- host architecture is ARM64;
- exactly one present X4 MI_03 audio device exists;
- its service remains `usbaudio2`;
- Secure Boot is disabled;
- Windows `TESTSIGNING` is enabled;
- the package DLL/catalog signer matches the supplied dedicated test certificate;
- no prior Stage A2 INF with the same original name is already in Driver Store.

Install order:

1. import only the dedicated test certificate;
2. stage APO component INF;
3. install Speaker extension INF;
4. PnP rescan;
5. record exact `oem*.inf` package names and certificate thumbprint;
6. verify X4 MI_03 still uses `usbaudio2`.

No FX registry value is written manually.

Rollback order:

1. remove exact extension `oem*.inf`;
2. rescan;
3. remove exact APO `oem*.inf`;
4. rescan/restart X4 MI_03 only;
5. remove only the dedicated Stage A2 test certificate;
6. verify X4 MI_03 is back on `usbaudio2`.

## Current gate

Stage A2 source/package preparation is complete, but the Stage A2 workflow has **not yet been run**.

Do not install anything until one fresh Stage A2 workflow execution proves:

1. ARM64 build PASS;
2. PowerShell parser PASS;
3. both InfVerif steps PASS;
4. DLL signing PASS;
5. Inf2Cat PASS;
6. both catalog signatures PASS;
7. final 8-file artifact upload PASS.

Only after that signed package gate passes should the test machine run `Preflight` and then `Install`.

## Live-test success target

First live goal remains:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG load -> transparent Speaker audio`

The validated pass-through DLL is intentionally not modified with APOProcess real-time telemetry in Stage A2. The Verify action can inspect component/FX metadata and, where Windows permits process-module inspection, whether `X4ApoArm64.dll` is loaded by `audiodg.exe`.

Do not add DSP/property setters until the live pass-through gate is closed.
