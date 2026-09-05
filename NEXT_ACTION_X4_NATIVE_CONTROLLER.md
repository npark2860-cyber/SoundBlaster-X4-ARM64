# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
2. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
3. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
4. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`

## Current architecture — recovered

The controller work now has three distinct Windows-side control paths that must not be conflated.

### A. CTCDC firmware

Confirmed examples:

- Direct Mode `0x39`;
- firmware Graphic EQ via PlaybackManager `0x96` params `9..20`;
- AudioControl descriptor/range/mute discovery via `0x21/0x22/0x24`;
- official `0x23` Platform usage is Game/Voice-indexed, not generic Windows mixer control.

### B. Native Windows mixer

Recovered chain:

`Creative.Platform.Mixer.dll`
-> `MalLgcy.dll`
-> `CTAudEp.dll`
-> public Core Audio / DeviceTopology

Direct ARM64 replacements are known:

| Control | Windows API |
|---|---|
| Endpoint master/channel | `IAudioEndpointVolume` |
| Monitoring volume | `IDeviceTopology/IPart` + `IAudioVolumeLevel` |
| Monitoring mute | `IDeviceTopology/IPart` + `IAudioMute` |
| Mic Boost | `KSNODETYPE_VOLUME` + `IAudioVolumeLevel` |
| Mic AGC | `KSNODETYPE_AGC` + `IAudioAutoGainControl` |

Do not port/load the supplied x86 MalLgcy/CTAudEp binaries into the ARM64 controller for this subset.

### C. Creative effects / CrystalVoice / non-EQ Acoustic Engine

Static recovery from the supplied exact binaries now shows:

`Creative App / Platform`
-> `ApoDeviceRepoKeyFactory`
-> `PropStoreRepository`
-> `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
-> `IPropertyStore::GetValue/SetValue`
-> Windows Audio System Effects notification path
-> `CTUSBAPO64.dll` DSP modules

This is the primary recovered Windows path for features such as:

- Crystalizer;
- Surround / CMSS-style processing;
- Smart Volume / SVM;
- Noise Reduction;
- AEC;
- Mic Beam;
- Mic SVM;
- VoiceFX.

Therefore the raw `VoiceInputManager (0x95)` no-response must not be interpreted as global CrystalVoice unsupported proof.

## Supplied APO/backend binaries

- `Creative.Platform.CoreAudio.dll`
  - SHA-256 `189ee6750a7e70f24421f1e2100fa88847878456f11233e28ed2fa0a6d1d4823`
  - managed Core Audio / Audio System Effects COM interop layer
- `CTUSBAPO64.dll`
  - SHA-256 `fa23a53861087df19487497c54067128f61a266cce2eae000f1a40b8752a17d3`
  - x86-64 native Creative APO with effect implementations
- `CTUSBfilt64.sys`
  - SHA-256 `bc0140f821b4d2f83405a6e89b135f98b0df690b6fdefbab47c47d7ae8856105`
  - x86-64 supporting WDM audio filter / forwarding component

`CTUSBfilt64.sys` is not the high-level effect property/DSP implementation recovered here. The actual Crystalizer/NR/AEC/MicBeam/SVM/etc modules are visibly present in `CTUSBAPO64.dll`.

## APO property-store control — direct static proof

The exact `Creative.Platform.Devices.dll` contains:

- `Features.Apo.ApoDeviceRepoKeyFactory`;
- `Features.Apo.PropStoreRepository`;
- `Models.Apo.ApoDeviceRepositoryInitializer`.

`ApoDeviceRepoKeyFactory` references approximately 158 Creative `CTPKEY_*` fields.

`PropStoreRepository` directly calls:

- `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`;
- `IPropertyStore::GetValue`;
- `IPropertyStore::SetValue`;
- `IAudioSystemEffectsPropertyStore::RegisterPropertyChangeNotification`.

Selected exact key examples:

| Feature | PROPERTYKEY |
|---|---|
| Crystalizer Enable | `{3cd83c04-868f-4f08-8d75-b4625ffe3b31}, PID 0` |
| Crystalizer Level | `{0f03f0bb-72c7-4ec1-8422-7b8d7410694a}, PID 0` |
| SVM Enable | `{9ad782d7-f46e-465c-8df5-3cda75424987}, PID 0` |
| AEC Enable | `{35f00393-1adf-43ce-84cb-7a926ac012b6}, PID 0` |
| Noise Reduction Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642}, PID 1` |
| MicBeam Plus Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642}, PID 0` |
| TD Noise Reduction family | `{e370f545-381e-4961-9a94-7f97aafa77d7}, PID 0..5` |
| Graphic EQ Enable | `{9a9d0cb2-4dc9-494c-8210-9848ae1aa629}, PID 0` |
| APO Direct Mode Enable | `{f3eaf467-52bd-4853-baa0-82d23a8759f5}, PID 0` |

These GUIDs are also present in the supplied `CTUSBAPO64.dll`, directly tying Platform repository keys to the native APO.

Full table and module/CLSID details are in:

`DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`

## Immediate priority 1 — X4-specific APO key/product selection

Do **not** create another broad runtime probe.

The next static task is to determine which APO property family and effect registrations the X4 endpoint actually selects.

Trace:

1. `ApoDeviceRepositoryInitializer` product/endpoint initialization;
2. `ApoDeviceRepoKeyFactory` selection logic;
3. feature enrichers/support predicates for SB1815/X4;
4. endpoint property/INF registration for SFX/MFX/EFX/AEC positions;
5. exact `PROPVARIANT` type, scale, range and default for each X4-relevant key.

Do not choose a legacy/current `CTPKEY_*` family merely because its name looks appropriate. Several generations coexist in the binary.

A later read-only endpoint FX property-store enumeration is justified only after this static selection is narrowed enough to make the runtime test targeted.

## Immediate priority 2 — APO hosting on Windows ARM64

The control/UI property-write path can be reimplemented natively on ARM64 using Windows Audio System Effects COM interfaces.

However, property writes alone do not recreate the DSP.

The supplied `CTUSBAPO64.dll` is x86-64 and contains the actual effect modules. The current ARM64 machine lacks the configured complete Creative APO/filter stack.

Determine statically/through installation metadata:

1. how the supplied APO is registered on Creative-supported Windows systems;
2. which FX positions/CLSIDs X4 uses;
3. which native dependencies and model files are required;
4. whether an ARM64-compatible processing/hosting path exists or an ARM64 replacement APO would be required.

Do **not** assume either that Windows ARM64 can host this exact x64 APO or that it cannot. That compatibility point is not yet proven.

## Remaining CDC AudioLevel task

The CDC Game/Voice raw `UInt16` engineering-unit conversion remains separate and unresolved.

Known facts:

- `RawResAudioLevelGet` returns raw `UInt16` unchanged;
- `AudioControlLevelRange` carries raw `UInt16` unchanged;
- `CDCGameVoiceFeature` does not convert it;
- MalLgcy/CTAudEp do not consume this representation;
- the APO property-store trace above is a separate feature path.

Observed values remain compatible with signed Q8.8, but `/256` is still not confirmed.

Only continue this point by finding an actual App/UI consumer of `CDCGameVoice`, `GameAudioLevel` or `ChatAudioLevel` values.

## `AudioLevel (0x23)` safety status

Static-confirmed:

- GET frame: `5A 23 02 01 <index>`;
- managed response payload is 3 bytes: index + `UInt16`;
- trailing runtime `0x03` is outside the managed struct and ignored;
- official Platform keys are created only for Game/Voice indices selected from type 19/18 descriptors;
- X4 runtime descriptor list contains neither type.

Therefore:

- do not repeat generic index `0..9` `0x23` probing;
- do not infer missing volume support from idx2..9 `GeneralFailure`;
- do not issue `0x23` SET.

## Runtime safety rules

- Creative App must be fully closed for independent CTCDC runtime tests.
- Keep read-only tests read-only unless a specific state-changing operation has exact static evidence and separate validation intent.
- Every new state-changing hardware command requires physical X4 confirmation.
- `WriteFile` success alone is not hardware validation.
- Do not repeat blind raw `0x95` probing.
- Do not modify B5 ASIO from this branch.
- Do not change unrelated paths.
