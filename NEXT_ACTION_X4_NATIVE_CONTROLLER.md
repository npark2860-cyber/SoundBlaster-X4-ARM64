# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Stage A0 status — binary + offline COM gates PASSED

Validated `X4ApoArm64.dll`:

- size `176640` bytes
- SHA-256 `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / machine `0xAA64`
- exports `DllCanUnloadNow`, `DllGetClassObject`
- official X4 SFX/MFX/EFX CLSIDs present
- AVRT sections `RT_CODE`, `RT_CONST`, `RT_DATA` present
- no Creative x64 DLL imports

Offline ARM64 COM runtime probe:

`RESULT: PASS`

Canonical traces:

- `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`
- `DEBUG_HISTORY_20260905_X4_ARM64_APO_COM_PROBE_RUNTIME_SUCCESS.md`

## usbaudio2 attachment discovery — runtime PASS

Canonical runtime record:

`DEBUG_HISTORY_20260905_X4_USBAUDIO2_ATTACHMENT_RUNTIME_SUCCESS.md`

Live X4 audio devnode:

`USB\VID_041E&PID_3278&MI_03\7&8197BA2&0&0003`

Confirmed:

- Class `MEDIA`
- Service `usbaudio2`
- Friendly name `Sound Blaster X4`
- KSCATEGORY_AUDIO `msft_wave`
- KSCATEGORY_AUDIO `msft_topo`
- KSCATEGORY_TOPOLOGY `msft_topo`

Runtime `msft_topo` categories include:

- Speaker
- SPDIF
- Microphone
- Line
- Digital audio interface

This closely reproduces the endpoint-category families recovered from the official SB1815 INF and confirms that the X4-specific package should target `USB\VID_041E&PID_3278&MI_03` while retaining Microsoft `usbaudio2` as the function driver.

Current bare `usbaudio2` state has no `FX` or `EP` subtree in the inspected devnode/driver/audio-interface/topology-interface registry locations. The Stage A0 APO therefore is not currently attached to AudioDG; this is not an APO DLL failure.

## Current blocking ambiguity — Headphone endpoint association

The live KS topology did **not** expose a `KSNODETYPE_HEADPHONES` category.

The official Creative SB1815 Win11 INF nevertheless has a distinct `FX\1` Headphone entry using the same SFX/MFX/EFX CLSIDs as Speaker.

Therefore:

- do not treat `FX\n` as a live KS pin number;
- do not guess that `FX\1` maps to KS pin 1;
- do not activate the review package yet.

The exact Headphone endpoint association must be recovered from MMDevice endpoint properties / official `PKEY_AudioEndpoint_Association` and `PKEY_FX_Association` semantics.

## Immediate priority 1 — one more read-only endpoint-property pass

Dump all property keys and values for the X4 MMDevice endpoints and identify at minimum:

1. `PKEY_AudioEndpoint_Association`;
2. `PKEY_AudioEndpoint_FormFactor`;
3. endpoint/device-interface identity properties;
4. render Speaker vs Headphone differentiation;
5. capture Microphone association;
6. any property that ties the endpoint back to the X4 `msft_topo` interface.

This must remain read-only.

Microsoft's current SYSVAD model uses:

- `PKEY_AudioEndpoint_Association = {1DA5D803-D492-4EDD-8C23-E0C0FFEE7F0E},2`
- `PKEY_FX_Association = {D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},0`

The association value is a KS category GUID; it is not inherently the same thing as an FX slot number.

## Immediate priority 2 — finalize pass-through test package only after association is exact

Review directory:

`packaging/x4-apo-arm64-review`

Keep `.inx.review` non-installing until Headphone/Speaker/Microphone associations are exact.

Then:

1. choose final software-component identity;
2. generate unique ExtensionId;
3. wire `AddComponent` to the X4 MI_03 extension;
4. attach only Speaker/Headphone/Microphone pass-through SFX/MFX/EFX metadata;
5. build/sign/verify package;
6. test rollback before enabling any DSP.

First live runtime gate:

`PnP package -> APO registration -> endpoint FX binding -> AudioDG Load/Initialize/LockForProcess/APOProcess -> transparent audio`

Success criteria:

- Speaker/Headphone/Microphone remain functional;
- no AudioDG crash or audio loss;
- native ARM64 APO loads in the live graph;
- pass-through remains transparent;
- uninstall restores bare Microsoft `usbaudio2` state.

## Fixed exclusions

- no Creative DSP yet
- no Creative FX property writes
- no CTCDC writes
- no SPDIF/DDL
- no CTUSBWrap/DGFX
- no Creative UpperFilter replacement without separate evidence
- no B5 ASIO changes

## Safety

- one variable at a time
- no manual FX registry writes
- no `regsvr32`
- no live APO install while Headphone association is unresolved
- no blind `0x95` probing
- no generic `0x23` probing
- no unrelated changes
