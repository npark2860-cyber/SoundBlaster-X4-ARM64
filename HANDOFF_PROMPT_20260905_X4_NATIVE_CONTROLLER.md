# HANDOFF PROMPT — Sound Blaster X4 Native Controller / ARM64 APO

Use this file to start the next ChatGPT tab without reconstructing prior conversation from memory.

## Repository / branch

Repository:

`npark2860-cyber/SoundBlaster-X4-ARM64`

Controller branch:

`exp/windows-arm64-x4-native-controller`

Pre-handoff verified branch baseline:

`fcbdd4b3420fee4a0b21239ef33e996d22cecf1b`

Always fetch the actual current branch HEAD before changing anything because handoff-document commits may advance HEAD after this baseline.

GitHub is the source of truth. Do not infer missing state from previous chat memory.

## Mandatory read order

1. `CURRENT_HANDOFF_X4_NATIVE_CONTROLLER.md`
2. `NEXT_ACTION_X4_NATIVE_CONTROLLER.md`
3. `DEBUG_HISTORY_20260905_X4_APO_STAGE_A1_SPEAKER_PACKAGE_OFFLINE_GATE.md`
4. `DEBUG_HISTORY_20260905_X4_MMDEVICE_ENDPOINT_ASSOCIATION_RUNTIME_SUCCESS.md`
5. `DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`
6. `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`
7. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
8. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
9. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
10. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
11. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
12. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
13. `packaging/x4-apo-arm64-stage-a1-speaker/README.md`
14. `packaging/x4-apo-arm64-review/README.md`

## Fixed project boundaries

Keep these backend classes separate:

1. X4 firmware / CTCDC raw control
2. Windows Core Audio endpoint/property control
3. Creative APO/filter DSP processing
4. Creative App/profile orchestration

Do not modify B5 ASIO source, WaveRT engine, mux, failsafe, B5 control-panel behavior, or unrelated paths from this controller/APO workstream.

Do not repeat excluded BLE/HID/UAC Extension Unit/vendor-interface/random raw probing.

Do not generic-probe CTCDC `0x23` again.

Do not interpret raw VoiceInputManager `0x95` no-response as global CrystalVoice unsupported proof.

Do not hard-code CDC `/256`; engineering-unit conversion remains unresolved.

## Closed runtime gates

### Stage A0 native ARM64 APO — PASS

`X4ApoArm64.dll` native ARM64 binary and offline COM gates passed.

Validated earlier Stage A0 artifact:

- PE32+ ARM64 / machine `0xAA64`
- exact official X4 SFX/MFX/EFX CLSIDs
- exports `DllGetClassObject`, `DllCanUnloadNow`
- AVRT sections present
- offline class factory / APO object / interface QI probe PASS
- final `DllCanUnloadNow == S_OK`

This does not yet prove live AudioDG graph loading.

### Microsoft usbaudio2 attachment discovery — PASS

Live X4 audio function:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

Confirmed:

- Class `MEDIA`
- Service `usbaudio2`
- KSCATEGORY_AUDIO `msft_wave`
- KSCATEGORY_AUDIO `msft_topo`
- KSCATEGORY_TOPOLOGY `msft_topo`

Runtime `msft_topo` categories include Speaker, SPDIF, Microphone, Line and Digital Audio Interface.

No `FX` or `EP` subtree was found in the inspected bare-usbaudio2 device/driver/interface locations.

### MMDevice endpoint association discovery — PASS

Six active X4 MMDevice endpoints were observed:

- Render Speakers
- Render SPDIF
- Capture Microphone
- Capture UnknownDigitalPassthrough
- Capture SPDIF
- Capture LineLevel

All six report:

`PKEY_AudioEndpoint_Association = GUID_NULL`

There is no separate Headphones MMDevice in the current bare Microsoft `usbaudio2` state.

Therefore:

- do not equate Creative `FX\1` with KS pin 1;
- do not invent a Headphone endpoint;
- keep Headphone out of Stage A1;
- Speaker is the first exact runtime/static common attachment point.

## Current work — Stage A1 Speaker-only OFFLINE package gate

Package directory:

`packaging/x4-apo-arm64-stage-a1-speaker`

Files:

- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

Workflow:

`Build X4 APO ARM64 Stage A1 Speaker Package`

The workflow file is exposed on default branch `main` only so GitHub Actions lists it. The actual workflow explicitly checks out:

`exp/windows-arm64-x4-native-controller`

Stage A1 scope is Speaker only:

- target HWID `USB\VID_041E&PID_3278&MI_03`
- retain Microsoft `usbaudio2`
- reuse KSCATEGORY_AUDIO reference string `msft_topo`
- add Speaker `FX\0` only
- `PKEY_FX_Association = KSNODETYPE_SPEAKER`
- bind Stage A0 native ARM64 SFX/MFX/EFX
- default processing mode only
- APO component identity `SWC\VEN_NPKR&CID_X4APO`

Excluded from A1:

- Headphone
- Microphone
- SPDIF / DDL
- Line In
- What-U-Hear
- Creative DSP
- Creative FX property writes
- CTCDC writes
- CTUSBWrap / CTUSBDGFX
- Creative UpperFilter / CTUSBfilt replacement

## Stage A1 workflow failure / fix history

Initial workflow failure:

`msbuild is not recognized`

This was a GitHub Actions environment/PATH issue, not an APO source compile failure.

The current workflow no longer relies on PATH or setup-msbuild. It:

1. uses `vswhere.exe`;
2. resolves Visual Studio `MSBuild\Current\Bin\MSBuild.exe`;
3. invokes MSBuild by absolute path.

The user later reported that the fresh workflow produced the DLL, which proves the build progressed beyond the old MSBuild-path failure.

## IMPORTANT: Stage A1 is NOT yet recorded PASS

Do not infer the whole workflow succeeded merely because the DLL exists.

At handoff, exact evidence is still needed for the same fresh workflow execution:

1. `Locate MSBuild` PASS
2. `Build Stage A0 APO Release ARM64` PASS
3. `Stage package and verify ARM64 PE` PASS with machine `0xAA64`
4. `Locate WDK InfVerif` PASS
5. `InfVerif APO component INF` PASS
6. `InfVerif Speaker extension INF` PASS
7. `Upload offline Stage A1 package artifact` PASS

Expected artifact name:

`SoundBlaster-X4-APO-ARM64-Stage-A1-Speaker-OFFLINE`

Expected artifact contents:

- `X4ApoArm64.dll`
- `X4ApoArm64.inf`
- `X4ApoSpeakerExtension.inf`
- `README.md`

If InfVerif fails, fix only the exact diagnostic. Do not expand endpoint scope while fixing packaging validation.

## Next action

The first action in the next tab is to inspect the latest fresh Stage A1 workflow result and establish whether both InfVerif steps and final artifact upload passed.

If the full offline gate passes, only then prepare a separate explicit signed/test-install + rollback package for the first live Speaker-only pass-through test.

First live goal:

`PnP package -> APO software component -> X4 msft_topo Speaker FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

No DSP, property setters, Headphone, Mic or SPDIF work until this live Speaker pass-through gate succeeds.

## Safety

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live install before Stage A1 offline validation is conclusively PASS
- no new hardware state changes automatically
- no unrelated changes
- no B5 ASIO changes
