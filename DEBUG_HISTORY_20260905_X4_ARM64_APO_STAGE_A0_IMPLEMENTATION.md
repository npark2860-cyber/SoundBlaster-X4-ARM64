# DEBUG HISTORY — 2026-09-05 X4 ARM64 APO Stage A0 Implementation

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

Stage A0 starts the first native ARM64 Creative-effects restoration implementation after the SB1815/X4 APO control plane, property schema and official `ctusbaud.inf` endpoint bindings were statically recovered.

The milestone is deliberately narrow:

- native ARM64 APO source only;
- pass-through audio only;
- no Creative DSP algorithms;
- no Creative FX property writes;
- no CTCDC writes;
- no live endpoint/registry installation;
- no SPDIF/DDL chain;
- no B5 ASIO changes.

## Source directory

`src/x4-apo-arm64`

Files:

- `X4Apo.h`
- `X4Apo.cpp`
- `X4ApoDll.cpp`
- `X4ApoArm64.def`
- `X4ApoArm64.vcxproj`
- `README.md`

Manual build workflow:

`.github/workflows/build-x4-apo-arm64-stage-a0.yml`

The workflow is `workflow_dispatch` only. No push/pull_request trigger was added.

## Official SB1815 class identities used

The implementation uses the exact Windows 11 Creative APO CLSIDs recovered from the supplied SB1815 `ctusbaud.inf`:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

These are the official X4 Speaker/Headphone/Microphone SFX/MFX/EFX identities on the amd64 package.

Stage A0 intentionally does not include the SPDIF chainer/DGFX CLSIDs.

## Class architecture

Three ATL COM classes are present:

- `CX4SfxApo`
- `CX4MfxApo`
- `CX4EfxApo`

They share `CX4PassThroughApoBase` and expose:

- `IAudioProcessingObject`
- `IAudioProcessingObjectRT`
- `IAudioProcessingObjectConfiguration`
- `IAudioSystemEffects`
- `IAudioSystemEffects2`
- `IAudioSystemEffects3`
- `IAudioProcessingObjectNotifications`

The common processing base derives from Microsoft's `CBaseAudioProcessingObject`.

`CRegAPOProperties<1>` is registered at version 1.0 with primary IID `IAudioProcessingObject`, matching the current Microsoft SYSVAD AEC APO pattern.

## Real-time behavior

`APOProcess` is intentionally transparent.

For one locked input/output connection it:

- uses the standard float32 APO buffer model;
- handles `BUFFER_VALID` by copying frames only when input/output buffers differ;
- handles `BUFFER_SILENT` by writing zeroes to the output buffer;
- propagates buffer flags and valid-frame count;
- performs no allocation;
- performs no COM operation;
- performs no property-store access;
- performs no logging;
- performs no blocking operation.

This follows the real-time restrictions and buffer model used by Microsoft SYSVAD APO samples.

## Initialization behavior

`Initialize` currently accepts:

- `APOInitSystemEffects3`
- `APOInitSystemEffects2`
- `APOInitSystemEffects`

For `APOInitSystemEffects3`, Stage A0 caches only:

- `AudioProcessingMode`;
- the final `IMMDevice` in `pDeviceCollection`, following the current SYSVAD endpoint-selection convention.

It deliberately does **not** open `IAudioSystemEffectsPropertyStore` yet.

Reason: the official X4 INF binds the same Creative SFX/MFX/EFX CLSIDs to both Speaker and Headphone, but the Windows 11 Creative FX context differs:

- general `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

Until the exact endpoint/context selection predicate is implemented, hard-coding either context would be incorrect.

## System-effects behavior

Stage A0 advertises no controllable effects:

- `GetEffectsList` -> empty list;
- `GetControllableSystemEffectsList` -> empty list;
- `SetAudioSystemEffectState` -> rejected/not found;
- notification-registration list -> empty;
- notification handler -> no-op.

Therefore Stage A0 cannot change Crystalizer, Surround, SVM, NR, AEC, MicBeam, VoiceFX or any other effect state.

## COM DLL behavior

`X4ApoDll.cpp` provides an ATL DLL module and the standard exports:

- `DllCanUnloadNow`
- `DllGetClassObject`

The three APO registration-property blocks are exposed in `gCoreAPOs`.

`DECLARE_NO_REGISTRY()` is used on the classes because the eventual Windows 11 APO component/extension package, not self-registration from this DLL, should own installation.

No `DllRegisterServer` path was added.

## Build project

`X4ApoArm64.vcxproj` is ARM64-only and follows the current Microsoft SYSVAD APO project model:

- Visual Studio / MSBuild project;
- `WindowsApplicationForDrivers10.0` toolset;
- Windows Driver target platform;
- dynamic library;
- ATL;
- C++17;
- W4 + warnings-as-errors;
- Release/Debug ARM64 configurations.

Link dependencies currently include:

- `Kernel32.lib`
- `ole32.lib`
- `oleaut32.lib`
- `advapi32.lib`
- `user32.lib`
- `uuid.lib`
- `AudioBaseProcessingObjectV140.lib`
- `audiomediatypecrt.lib`
- `AudioEng.lib`

`rtworkq.lib` is intentionally not linked because Stage A0 does not use an RT work queue.

## Manual GitHub Actions build

Workflow:

`Build X4 APO ARM64 Stage A0`

It:

1. checks out the controller branch;
2. runs MSBuild for Release/ARM64;
3. verifies the output PE Machine is `0xAA64`;
4. uploads the DLL and README artifact.

### Critical status

**The workflow has not been executed yet.**

The currently connected GitHub tool can inspect/re-run existing workflow runs but cannot dispatch a new `workflow_dispatch` run. Therefore no claim of successful compilation is made by this document.

Current status is:

- implementation created;
- Microsoft SYSVAD ABI/pattern statically cross-checked;
- compile/run validation still pending.

## Static corrections already made

Before build validation, static review corrected:

1. `APO_CONNECTION_PROPERTY::pBuffer` handling from an invalid `static_cast` to the standard `reinterpret_cast<FLOAT32*>` used by SYSVAD;
2. explicit `mmdeviceapi.h` inclusion for cached `IMMDevice`;
3. `APO_REG_PROPERTIES` version/primary-IID alignment to version 1.0 / `IAudioProcessingObject`;
4. duplicate initialization guard using `APOERR_ALREADY_INITIALIZED`;
5. C++ CLSID object definitions to avoid `DEFINE_GUID` linkage ambiguity with ATL non-type template arguments.

## Packaging boundary

No installable INF/extension package was added in Stage A0.

Microsoft's current componentized APO model was reviewed and uses two roles:

1. a device extension component association (`AddComponent`);
2. an `AudioProcessingObject` software-component package that installs the APO DLL/COM/APO registration.

The official X4 `ctusbaud.inf` provides the endpoint SFX/MFX/EFX binding data, but the current Microsoft `usbaudio2` ARM64 endpoint/interface matching must be designed carefully before any live extension INF is produced.

Do not convert the research template directly into a live install package before the DLL first builds cleanly.

## Next gate

1. Run the manual `Build X4 APO ARM64 Stage A0` workflow.
2. Fix only actual compiler/linker errors from that run.
3. Verify the produced DLL is ARM64 PE `0xAA64`.
4. Only after clean build, create a **non-installing review template** for the Windows 11 componentized APO package/extension.
5. Do not bind or install the APO on the live X4 endpoint until package review is complete.

## Safety

No X4 hardware command, endpoint FX property, registry value or audio-driver binding was changed by Stage A0 implementation.
