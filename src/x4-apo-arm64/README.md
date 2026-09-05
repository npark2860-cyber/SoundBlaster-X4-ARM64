# X4 ARM64 APO — Stage A0 pass-through skeleton

This directory is intentionally isolated from the B5 ASIO work and from the existing X4 controller probes.

## Stage A0 goal

Prove that a **native ARM64** Audio Processing Object can be built and later loaded into the Windows 11 audio graph for Sound Blaster X4/SB1815 without changing audio samples or X4 hardware state.

Stage A0 contains three COM/APO classes using the official SB1815 Windows 11 CLSIDs recovered from the supplied `ctusbaud.inf`:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

All three currently share the same transparent pass-through implementation.

## Deliberately not implemented yet

Stage A0 does **not**:

- install or register itself on a live endpoint;
- modify the registry or Audio System Effects property stores;
- expose any controllable audio effects;
- write any Creative `CTPKEY_*` value;
- implement Crystalizer, Surround, SVM, Noise Reduction, AEC, MicBeam or VoiceFX DSP;
- emulate Creative `CEffectNodeInfo`;
- modify CTCDC state;
- include SPDIF / Dolby Digital Live / DGFX / wrapper chaining;
- modify any B5 ASIO source or behavior.

The APO returns an empty system-effects list and rejects effect-state changes. `APOProcess` only copies float32 samples when input/output buffers differ and propagates frame count/buffer flags.

## Why the Creative property store is not opened in Stage A0

The official X4 INF binds the same Creative SFX/MFX/EFX CLSIDs to both Speaker and Headphone, while Windows 11 defines different FX contexts:

- general `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`

Until the exact Creative context-selection predicate is reproduced, hard-coding one context would be incorrect. `Initialize(APOInitSystemEffects3)` therefore only caches the processing mode and endpoint identity.

## Build requirements

The project follows the Microsoft SYSVAD APO build model and requires:

- Visual Studio 2022 C++ build tools;
- Windows 11 SDK/WDK with the `WindowsApplicationForDrivers10.0` platform toolset;
- ARM64 C++ toolchain;
- ATL for ARM64.

Build example from a Developer Command Prompt with the WDK installed:

```text
msbuild src\x4-apo-arm64\X4ApoArm64.vcxproj /m /p:Configuration=Release /p:Platform=ARM64
```

Expected output:

`src\x4-apo-arm64\bin\ARM64\Release\X4ApoArm64.dll`

## Static basis

The skeleton follows the public Microsoft SYSVAD APO pattern:

- `CBaseAudioProcessingObject`
- `IAudioSystemEffects3`
- `IAudioProcessingObjectNotifications`
- `APOInitSystemEffects3`
- `APO_REG_PROPERTIES`

See the repository debug histories for the exact SB1815 INF bindings and Creative property schema.

## Next gate

Do not create/install an endpoint-binding INF until this DLL first compiles cleanly for ARM64. After build success, the next milestone is an isolated package/registration design that binds only the X4 Speaker/Headphone/Microphone graph to this pass-through APO and still performs no DSP/state changes.
