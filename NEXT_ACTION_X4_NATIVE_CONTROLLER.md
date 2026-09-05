# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_SB1815_INF_APO_BINDING_ARM64_TRACE.md`
2. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
3. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
4. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
5. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

## Architecture state

Three Windows-side control paths are now separated:

### A. Firmware / CTCDC

Confirmed live examples:

- Direct Mode `0x39`
- Graphic EQ via PlaybackManager `0x96` params `9..20`
- AudioControl discovery/mute via `0x21/0x22/0x24`

Do not repeat blind `0x95` VoiceInputManager probing.

### B. Normal Windows mixer

Direct ARM64 implementation is known:

- endpoint master/channel -> `IAudioEndpointVolume`
- monitoring -> `IDeviceTopology/IPart + IAudioVolumeLevel/IAudioMute`
- Mic Boost -> `KSNODETYPE_VOLUME + IAudioVolumeLevel`
- Mic AGC -> `KSNODETYPE_AGC + IAudioAutoGainControl`

Do not port/load the supplied x86 MalLgcy/CTAudEp binaries for this subset.

### C. Creative effects / CrystalVoice

Official control plane:

`Creative Platform -> IAudioSystemEffectsPropertyStore -> Creative APO`

Property type schema and key ranges are statically recovered.

The actual DSP is implemented in `CTUSBAPO64.dll`.

## SB1815 INF result — exact endpoint binding

Supplied `ctusbaud.inf`:

- SHA-256 `adc7b2128b9d90625efab36c6fc499d8d8f4328e368265f03222cf6720b98b0b`
- DriverVer `09/26/2024,3.06.03.00`
- X4 HWID `USB\VID_041E&PID_3278&MI_03`

The package has x86/amd64 install sections only. There is no ARM64 target.

Primary Creative APO CLSIDs:

- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`

Windows 11 SB1815 bindings:

| Path | Official FX |
|---|---|
| Speaker | Creative SFX/MFX/EFX |
| Headphone | same Creative SFX/MFX/EFX |
| Microphone | same Creative SFX/MFX/EFX |
| Line In | Microsoft effects |
| What U Hear | Creative SFX/MFX only |
| SPDIF Out | Creative SFX + chainer MFX/EFX + Creative EFX + DGFX/DDL chain |

Windows 11 Creative FX contexts:

- general `{852311BC-1AFB-454E-92CA-C35252CACAAF}`
- headphone `{3F5F306B-A033-4F19-843D-1C44A736FF4D}`
- each has `Default`, `Volatile`, `User` property stores

## ARM64 APO hosting — resolved direction

The official `CTUSBAPO64.dll` is plain x86-64 and the INF has no `ntarm64` path.

Custom APOs are in-process COM DLLs in the Windows audio engine. A native Arm64 process cannot directly load a plain x64 DLL; Arm64X only helps when an actual Arm64 view/implementation exists.

Therefore **do not attempt to register the original x64 CTUSBAPO64.dll directly into native ARM64 AudioDG as the final solution.**

## Immediate priority 1 — minimal ARM64 APO skeleton

Start the first implementation milestone narrowly:

1. keep Microsoft `usbaudio2` as base audio driver;
2. create an ARM64-native APO package/extension for Windows 11;
3. initially support the X4 Speaker/Headphone/Microphone SFX/MFX/EFX binding model only;
4. implement required Windows APO COM interfaces and registration/discovery;
5. implement `IAudioSystemEffectsPropertyStore` consumption/notification compatible with the recovered Creative key schema;
6. provide the equivalent EffectNodeInfo/product identity needed for Creative Platform discovery where appropriate;
7. keep the initial build read-only/pass-through DSP until graph loading is proven.

Do **not** implement effect setters/DSP algorithms in the first graph-loading milestone.

For current Windows 11 packaging, prefer an `AudioProcessingObject`-class APO package/extension as documented by Microsoft rather than copying the legacy x64 Creative MEDIA-INF deployment literally.

## Immediate priority 2 — isolate DSP modules

After the ARM64 APO can load/pass audio safely, split DSP work by function:

Playback first candidates:

- Crystalizer
- Surround
- SVM

Capture first candidates:

- Noise Reduction
- AEC
- MicBeam / Voice Focus
- Mic Smart Volume

Do not merge all effects into one initial implementation step.

## Separate later track — SPDIF / Dolby Digital Live

Do not include SPDIF/DDL in the first APO milestone.

Official SB1815 SPDIF graph additionally uses:

- MFX chainer `{6E623752-66A4-4281-BD29-D9DA22328623}`
- EFX chainer `{CC401F70-ACFB-4FBD-9F14-20E7CEF2E1A3}`
- DGFX `{242249CC-E3C8-4571-9A0B-ED0906B7F994}`
- `CTUSBWrap64.dll`
- `CTUSBDGFX64.dll`
- DDL selection

Treat this as a separate port after normal render/capture APO hosting works.

## APO property schema — important fixed facts

- bool -> `VT_BOOL`
- float -> `VT_R4`
- float vector -> `VT_VECTOR|VT_R4`
- string -> `VT_LPWSTR`

Examples:

- Crystalizer Level 0..1, step 0.01, default 0.65
- NR Strength 0..1, 0.01, default 0.5
- Surround Immersion 0..1, 0.01, default 0.4
- SVM mode is float values 0.0 / 1.0 / 2.0
- XBass APO strength is 0..100 step 1 default 50

Do not replace these with guessed integer formats.

## Remaining CDC AudioLevel task

CDC Game/Voice raw UInt16 engineering-unit conversion remains unresolved.

Do not search MalLgcy, CTAudEp or APO property paths for this conversion again unless a concrete reference proves relevance.

Only continue by finding a real App/UI consumer of `CDCGameVoice`, `GameAudioLevel` or `ChatAudioLevel` values.

## Safety

- Creative App fully closed for independent CTCDC tests.
- One variable at a time.
- First APO implementation milestone must be pass-through/read-only.
- Do not issue new hardware state changes automatically.
- Do not repeat generic `0x23` probing or blind `0x95` probing.
- Do not modify B5 ASIO from this branch.
- Do not change unrelated paths.
