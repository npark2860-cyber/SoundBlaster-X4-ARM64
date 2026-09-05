# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_ARM64_APO_STAGE_A0_IMPLEMENTATION.md`
2. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
3. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
4. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
5. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
6. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

## Current architecture

### A. Firmware / CTCDC

Confirmed live examples:

- Direct Mode `0x39`
- Graphic EQ via PlaybackManager `0x96` params `9..20`
- AudioControl discovery/mute via `0x21/0x22/0x24`

Do not repeat blind `0x95` VoiceInputManager probing or generic `0x23` index probing.

### B. Normal Windows mixer

Direct ARM64 implementation is known:

- endpoint master/channel -> `IAudioEndpointVolume`
- monitoring -> `IDeviceTopology/IPart + IAudioVolumeLevel/IAudioMute`
- Mic Boost -> `KSNODETYPE_VOLUME + IAudioVolumeLevel`
- Mic AGC -> `KSNODETYPE_AGC + IAudioAutoGainControl`

Do not port/load x86 MalLgcy/CTAudEp for this subset.

### C. Creative effects / CrystalVoice

Official control plane:

`Creative Platform -> IAudioSystemEffectsPropertyStore -> Creative APO`

The exact SB1815 property schema and Win11 endpoint SFX/MFX/EFX bindings are statically recovered.

The original `CTUSBAPO64.dll` is x86-64 only and is not a viable direct in-process payload for native ARM64 AudioDG.

## Stage A0 implementation — created, not yet built

Source:

`src/x4-apo-arm64`

Manual workflow:

`.github/workflows/build-x4-apo-arm64-stage-a0.yml`

Workflow name:

`Build X4 APO ARM64 Stage A0`

Implemented classes use the official X4 identities:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

All three currently share a transparent `CBaseAudioProcessingObject` implementation.

Stage A0 deliberately contains:

- no Creative DSP algorithms;
- no property-store writes;
- no controllable effects;
- no endpoint/registry installation;
- no CTCDC access;
- no SPDIF/DDL;
- no B5 changes.

The real-time path follows SYSVAD constraints:

- float32 input/output;
- no allocation/blocking/COM/property/logging in `APOProcess`;
- AVRT code placement;
- AVRT vtable placement;
- transparent frame copy / silence propagation only.

Initialization accepts SystemEffects3/2/1 and currently only caches processing mode plus endpoint identity. Creative FX User/Default/Volatile stores are intentionally unopened until the endpoint/context selection predicate is implemented.

### Critical status

**No successful Stage A0 build exists yet.**

The manual workflow was added but the currently connected GitHub tool cannot dispatch a new `workflow_dispatch` run. Do not claim the DLL compiles or loads until an actual run is performed.

## Immediate priority 1 — build validation

Run the manual workflow:

`Build X4 APO ARM64 Stage A0`

Then:

1. inspect actual compiler/linker output;
2. fix only real build errors;
3. keep all DSP/property/setter functionality disabled;
4. verify output PE machine `0xAA64`;
5. record DLL SHA-256 and artifact.

Do not create an installable endpoint-binding package before this gate succeeds.

## Immediate priority 2 — componentized package review only after build

After a clean ARM64 DLL build, prepare a **non-installing `.inx` review template**, based on Microsoft's current componentized APO model:

1. Extension package associates an APO software component using `AddComponent`.
2. `AudioProcessingObject`-class software-component package installs the ARM64 DLL, COM registration and `AudioEngine\AudioProcessingObjects` registration.
3. X4 endpoint FX bindings must reproduce the recovered SB1815 Speaker/Headphone/Microphone graph only.

Do not initially include:

- SPDIF/DDL;
- CTUSBWrap/DGFX;
- Creative UpperFilter replacement;
- live registry modification;
- automatic installation.

The initial package must remain a review template until the base Microsoft `usbaudio2` interface/component matching is proven correct.

## Later priority — context/property discovery

Only after pass-through graph loading works:

1. recover/implement exact general-vs-headphone FX context selection;
2. open the correct `IAudioSystemEffectsPropertyStore` read-only;
3. register endpoint-property notifications;
4. validate Creative Platform repository discovery;
5. only then add one DSP feature at a time.

Do not fake `CEffectNodeInfo` or APO HW identifier 100 until its exact interface contract is recovered or a simpler Windows-native discovery path is proven sufficient.

## DSP split after graph validation

Playback candidates:

- Crystalizer
- Surround
- SVM

Capture candidates:

- Noise Reduction
- AEC
- MicBeam / Voice Focus
- Mic Smart Volume

Keep these separate; do not implement all DSP at once.

## Separate later track — SPDIF / Dolby Digital Live

Do not include SPDIF/DDL in Stage A0.

Official SB1815 SPDIF graph additionally uses:

- MFX chainer `{6E623752-66A4-4281-BD29-D9DA22328623}`
- EFX chainer `{CC401F70-ACFB-4FBD-9F14-20E7CEF2E1A3}`
- DGFX `{242249CC-E3C8-4571-9A0B-ED0906B7F994}`
- `CTUSBWrap64.dll`
- `CTUSBDGFX64.dll`
- DDL selection

Treat this as a separate port after normal render/capture APO hosting works.

## APO property schema — fixed facts

- bool -> `VT_BOOL`
- float -> `VT_R4`
- float vector -> `VT_VECTOR|VT_R4`
- string -> `VT_LPWSTR`

Examples:

- Crystalizer Level 0..1, step 0.01, default 0.65
- NR Strength 0..1, 0.01, default 0.5
- Surround Immersion 0..1, 0.01, default 0.4
- SVM mode = float 0.0 / 1.0 / 2.0
- XBass APO strength = 0..100 step 1 default 50

Do not substitute guessed integer encodings.

## Remaining CDC AudioLevel task

CDC Game/Voice raw UInt16 engineering-unit conversion remains unresolved.

Do not search MalLgcy, CTAudEp or APO property paths for it again unless a concrete reference proves relevance. Only continue by finding a real App/UI consumer of `CDCGameVoice`, `GameAudioLevel` or `ChatAudioLevel`.

## Safety

- Creative App fully closed for independent CTCDC tests.
- One variable at a time.
- Stage A0 remains pass-through/read-only.
- No hardware state changes automatically.
- No installable APO binding until build/package review gates pass.
- No generic `0x23` probing or blind `0x95` probing.
- No B5 ASIO modifications.
- No unrelated changes.
