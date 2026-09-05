# NEXT ACTION — X4 Native Controller / ARM64 APO

Updated: 2026-09-05 KST

Branch:

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify actual branch HEAD before work.

## Current Stage A0 status

The native ARM64 pass-through APO now has a real built artifact and its binary gate passed.

Validated `X4ApoArm64.dll`:

- size `176640` bytes
- SHA-256 `136aaa68e83a952e19b786526dae76ce026b3641b8cf84f13bbbe9df9152abcd`
- PE32+ ARM64 / machine `0xAA64`
- exports `DllCanUnloadNow`, `DllGetClassObject`
- no `DllRegisterServer`
- official X4 SFX/MFX/EFX CLSIDs physically present
- AVRT sections `RT_CODE`, `RT_CONST`, `RT_DATA` present
- no Creative x64 DLL imports

Canonical trace:

`DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_BINARY_VALIDATION.md`

## Immediate gate — offline COM probe

Do **not** install or bind the APO yet.

The next task is to validate ATL class-factory/runtime interface creation without touching AudioDG, registry or X4 endpoints.

Probe source:

`src/x4-apo-com-probe-arm64`

The probe directly performs:

`LoadLibraryW -> DllGetClassObject -> IClassFactory::CreateInstance -> QueryInterface`

for all three official X4 classes:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

It checks:

- `IAudioProcessingObject`
- `IAudioProcessingObjectRT`
- `IAudioProcessingObjectConfiguration`
- `IAudioSystemEffects`
- `IAudioSystemEffects2`
- `IAudioSystemEffects3`
- `IAudioProcessingObjectNotifications`
- `DllCanUnloadNow == S_OK`

It deliberately does not:

- call `Initialize`;
- access AudioDG;
- register COM classes;
- write registry values;
- open X4 endpoints;
- read/write Creative FX stores;
- issue CTCDC commands.

## How to obtain/run the probe

Run the existing manual workflow:

`Build X4 APO ARM64 Stage A0`

The artifact now contains:

- `X4ApoArm64.dll`
- `X4ApoComProbeArm64.exe`
- `RUN-COM-PROBE.cmd`
- README files

On the ARM64 test PC:

1. extract the artifact to one folder;
2. double-click `RUN-COM-PROBE.cmd`;
3. capture the complete console output.

Required gate:

`RESULT: PASS`

If it fails, fix only the actual loader/class-factory/QI failure. Do not proceed to install packaging.

## After offline COM probe PASS

Only then:

1. create a non-installing review `.inx` template for Windows 11 componentized APO packaging;
2. review matching against the current Microsoft `usbaudio2` ARM64 endpoint/interface;
3. perform the first AudioDG graph-loading test with pass-through only;
4. implement exact general-vs-headphone FX context selection;
5. open `IAudioSystemEffectsPropertyStore` read-only and validate notifications;
6. add DSP/effect setters one feature at a time only after those gates pass.

## Fixed architecture constraints

- retain Microsoft USB Audio 2.0 as base driver
- original `CTUSBAPO64.dll` is x86-64 only and is not the native ARM64 AudioDG solution
- Speaker/Headphone/Microphone use the official Creative SFX/MFX/EFX identities
- general FX context `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone FX context `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`
- SPDIF/DDL/CTUSBWrap/DGFX remain a separate later track
- no B5 ASIO changes from this branch

## Safety

- one variable at a time
- Stage A0 remains pass-through/read-only
- no new hardware state changes automatically
- no live APO install before offline COM probe PASS
- no blind `0x95` probing
- no generic `0x23` probing
- no unrelated changes
