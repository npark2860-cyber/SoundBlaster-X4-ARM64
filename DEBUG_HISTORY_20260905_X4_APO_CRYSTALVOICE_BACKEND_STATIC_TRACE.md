# DEBUG HISTORY — 2026-09-05 X4 APO / CrystalVoice Backend Static Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace resolves the Windows-side control plane for Creative APO-backed processing using the supplied Creative binaries.

It specifically answers why raw `VoiceInputManager (0x95)` no-response does not imply that CrystalVoice or the non-EQ Acoustic Engine processing is absent from the official Windows stack.

No hardware runtime probe was added, no mixer SET was issued, and no B5 ASIO code was modified.

## Supplied binaries

- `Creative.Platform.CoreAudio.dll`
  - SHA-256 `189ee6750a7e70f24421f1e2100fa88847878456f11233e28ed2fa0a6d1d4823`
  - PE32 / i386 managed assembly
- `CTUSBAPO64.dll`
  - SHA-256 `fa23a53861087df19487497c54067128f61a266cce2eae000f1a40b8752a17d3`
  - PE32+ / x86-64 native DLL
- `CTUSBfilt64.sys`
  - SHA-256 `bc0140f821b4d2f83405a6e89b135f98b0df690b6fdefbab47c47d7ae8856105`
  - PE32+ / x86-64 native driver

The already-known exact `Creative.Platform.Devices.dll` baseline used for cross-reference is:

`2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`

## 1. Result summary

The official Windows control path for CrystalVoice and many non-EQ Acoustic Engine features is not the tested raw CTCDC `VoiceInputManager (0x95)` route.

Static recovery shows the following control plane:

`Creative App / Creative Platform feature model`
-> `ApoDeviceRepoKeyFactory`
-> `PropStoreRepository`
-> `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
-> `IPropertyStore::GetValue / SetValue`
-> Windows Audio System Effects property-store notification path
-> `CTUSBAPO64.dll` effect modules / processors

This is a Windows Audio System Effects / APO path spanning backend classes 2 and 3:

2. Windows Core Audio endpoint/property control
3. Creative APO DSP implementation

The Creative App/profile layer remains class 4 orchestration above it.

The raw `0x95` CTCDC result must therefore remain classified only as:

- that tested firmware/session route did not respond;
- it is not proof that CrystalVoice globally lacks an implementation.

## 2. `Creative.Platform.CoreAudio.dll` role

The supplied assembly is a managed Core Audio / DeviceTopology / property-store interop layer, not the DSP implementation itself.

Recovered interface names include:

- `IAudioEndpointVolume`
- `IAudioVolumeLevel`
- `IAudioAutoGainControl`
- `IDeviceTopology`
- `IPart`
- `IPropertyStore`
- `IAudioSystemEffectsPropertyStore`
- `IAudioSystemEffectsPropertyChangeNotificationClient`

Recovered Audio System Effects property-store operations include:

- `OpenDefaultPropertyStore`
- `OpenUserPropertyStore`
- `OpenVolatilePropertyStore`
- `ResetUserPropertyStore`
- `ResetVolatilePropertyStore`
- `RegisterPropertyChangeNotification`

Recovered interface GUID:

`IAudioSystemEffectsPropertyStore = 302AE7F9-D7E0-43E4-971B-1F8293613D2A`

This assembly provides the managed COM surface needed to reach Windows 11 Audio System Effects property stores.

## 3. Direct Creative Platform APO repository proof

The exact `Creative.Platform.Devices.dll` contains:

- `Creative.Platform.Devices.Features.Apo.PropStoreRepository`
- `Creative.Platform.Devices.Features.Apo.ApoDeviceRepoKeyFactory`
- `Creative.Platform.Devices.Models.Apo.ApoDeviceRepositoryInitializer`
- `PropertyKeyFinder`

`ApoDeviceRepoKeyFactory` directly references approximately 158 Creative `CTPKEY_*` fields covering playback and capture processing.

`PropStoreRepository` directly invokes the COM path:

- `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
- `IPropertyStore::GetValue`
- `IPropertyStore::SetValue`
- `IAudioSystemEffectsPropertyStore::RegisterPropertyChangeNotification`

Therefore these `CTPKEY_*` values are not merely unused metadata strings. They are the key material used by the official Platform APO repository.

## 4. Selected exact Creative APO PROPERTYKEYs

The following PROPERTYKEYs were recovered from the exact managed binary static constructors and then cross-checked against `CTUSBAPO64.dll` where noted.

### Playback / Acoustic Engine family

| Property | GUID | PID |
|---|---|---:|
| `CTPKEY_CMSS3D_Enable` | `5b4777a4-8ad4-4d34-893a-df34da0e56ca` | 0 |
| `CTPKEY_CMSS3D_Immersion` | `a5a78ea4-c156-4db7-85aa-81cff1c3f192` | 0 |
| `CTPKEY_Crystalizer_Enable` | `3cd83c04-868f-4f08-8d75-b4625ffe3b31` | 0 |
| `CTPKEY_Crystalizer_Level` | `0f03f0bb-72c7-4ec1-8422-7b8d7410694a` | 0 |
| `CTPKEY_Crystalizer_PreAttenuation` | `3c663165-48fc-41d8-8120-81cb0b12b230` | 0 |
| `CTPKEY_SVM_Enable` | `9ad782d7-f46e-465c-8df5-3cda75424987` | 0 |
| `CTPKEY_SVM_Strength` | `80b0c7bb-0989-434e-af5b-fb9020f471b3` | 0 |
| `CTPKEY_DA_Enable` | `ea3137f9-be10-4eaa-8fce-a36988bca7dd` | 0 |
| `CTPKEY_DA_Strength` | `a79717e9-81cf-4272-adc6-d12b69b389a0` | 0 |
| `CTPKEY_BassMgmt_RedirectionEnable` | `d3dcf273-cf72-40c5-a1ab-a7785a849ea8` | 0 |
| `CTPKEY_BassMgmt_XOverFrequency` | `836d3bc0-7c99-4e38-990f-68775abc8335` | 0 |
| `CTPKEY_BassMgmt_XBassEnable` | `f67cf426-f8cb-4a40-bdac-580802e3e193` | 0 |
| `CTPKEY_APODevice_DirectMode_Enable` | `f3eaf467-52bd-4853-baa0-82d23a8759f5` | 0 |
| `CTPKEY_GraphicEQ_Enable` | `9a9d0cb2-4dc9-494c-8210-9848ae1aa629` | 0 |
| `CTPKEY_GraphicEQ_PreampGain` | `ddcf8d90-de27-4de4-af57-088b8ad78fdf` | 0 |

Graphic EQ band gains use common GUID:

`2b88c76d-d07c-4e97-8922-1bac9f6d5935`

with PIDs `0..9` for `Gain0..Gain9`.

### Capture / CrystalVoice family

| Property | GUID | PID |
|---|---|---:|
| `CTPKEY_MicSVM_Enable` | `400d2ef9-cec3-4c2f-ab54-4f9b47f7d615` | 0 |
| `CTPKEY_MicSVM_Strength` | `22821d29-df1d-4907-a721-4b3937542e87` | 0 |
| `CTPKEY_AEC_Enable` | `35f00393-1adf-43ce-84cb-7a926ac012b6` | 0 |
| `CTPKEY_AEC_AcousticRefDelayMs` | `c3dff463-c723-43ef-af15-a179bcd26c68` | 0 |
| `CTPKEY_AEC_SysRefDelayMs` | `389add8a-4d5a-4956-94b8-72a64969ea87` | 0 |
| `CTPKEY_AEC_AdaptStep` | `c5e9808c-450b-4d7d-95be-f94f21fe99d7` | 0 |
| `CTPKEY_AEC_NonLinSubEnable` | `a76e4e13-8df0-4f52-95f3-c41e07a53875` | 0 |
| `CTPKEY_AEC_NonLinSubStrength` | `e88ce533-4051-498f-baad-5eca85e7f49b` | 0 |
| `CTPKEY_AEC_NoiseReduceEnable` | `a2639f16-bf52-4098-b01d-9577a5c65204` | 0 |
| `CTPKEY_AEC_NoiseReduceStrength` | `4ab280ce-6e99-4ffe-9214-3c42fc32fd42` | 0 |
| `CTPKEY_AEC_AutoRefDelayEnable` | `ac602b7c-f9de-4d2e-94fb-3b66f2cc4064` | 0 |
| `CTPKEY_AEC_MaxAttendB` | `9aaa4e2b-2ed1-4509-a1cd-4d564818598d` | 0 |
| `CTPKEY_MicBeamPlus_Enable` | `40d0d021-20bd-4d15-a93c-1dbe8922c642` | 0 |
| `CTPKEY_MicBeamPlus_MicDistance` | `40d0d021-20bd-4d15-a93c-1dbe8922c642` | 11 |
| `CTPKEY_MicBeamPlus_WedgeAngle` | `72e09675-2af9-485c-89f1-898e532bf06e` | 0 |
| `CTPKEY_MicBeamPlus_SourceAngle` | `a0d4f6a1-9775-48a2-8d4d-c0441436bf60` | 0 |
| `CTPKEY_MicBeamPlus_Gain` | `8d6ddb63-253d-424e-be3b-7391722c4227` | 0 |

### Noise Reduction family

| Property | GUID | PID |
|---|---|---:|
| `CTPKEY_NoiseReduction_Enable` | `40d0d021-20bd-4d15-a93c-1dbe8922c642` | 1 |
| `CTPKEY_NoiseReduction_Strength` | `6a72f5dd-6c09-4147-82c5-14c64b0e4e0f` | 0 |
| `CTPKEY_THX_NoiseReduction_AutoAdjustEnable` | `6a72f5dd-6c09-4147-82c5-14c64b0e4e0f` | 1 |
| `CTPKEY_THX_TDNoiseReduction_Enable` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 0 |
| `CTPKEY_THX_TDNoiseReduction_Strength` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 1 |
| `CTPKEY_THX_TDNoiseReduction_AutoAdjustEnable` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 2 |
| `CTPKEY_THX_TDNoiseReduction_CommAudioEnable` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 3 |
| `CTPKEY_THX_TDNoiseReduction_CommAudioBgEnable` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 4 |
| `CTPKEY_THX_TDNoiseReduction_SpeechDetectEnable` | `e370f545-381e-4961-9a94-7f97aafa77d7` | 5 |

Older/generic property families also exist in the Platform binary. Examples include:

- generic Crystalizer family GUID `7c060dbc-be6f-45a5-86b5-109f4400634c` with Enable PID 0, percentage PID 1, capability PID 2;
- generic SVM Enable GUID `0f12a9a5-3e81-4bad-a0ec-700ce2343711`, PID 0;
- effect master GUID `95cdd70d-8d6c-4ed5-8873-d7ab1ed0fcf4` with multiple product-family PIDs.

Do not choose between legacy and current key families only by name. The endpoint/product-specific repository mapping must determine which key set applies to X4.

## 5. Managed-key to native-APO cross-confirmation

The selected managed GUIDs above are physically present in the supplied `CTUSBAPO64.dll` binary.

Examples:

- Crystalizer Enable `3cd83c04-868f-4f08-8d75-b4625ffe3b31`
- Crystalizer Level `0f03f0bb-72c7-4ec1-8422-7b8d7410694a`
- CMSS3D Enable `5b4777a4-8ad4-4d34-893a-df34da0e56ca`
- SVM Enable `9ad782d7-f46e-465c-8df5-3cda75424987`
- AEC Enable `35f00393-1adf-43ce-84cb-7a926ac012b6`
- MicBeam Plus Wedge `72e09675-2af9-485c-89f1-898e532bf06e`
- TD Noise Reduction family `e370f545-381e-4961-9a94-7f97aafa77d7`
- Bass Redirection `d3dcf273-cf72-40c5-a1ab-a7785a849ea8`
- Direct Mode APO property `f3eaf467-52bd-4853-baa0-82d23a8759f5`

This cross-match strongly identifies the official Platform property repository and the supplied Creative APO as two sides of the same control plane.

## 6. `CTUSBAPO64.dll` Windows APO architecture

The supplied APO is a native x86-64 user-mode Audio Processing Object implementation.

Static strings/code include:

- `Initialize called with APOInitSystemEffects3`
- `GetApoNotificationRegistrationInfo`
- `HandleNotification`
- endpoint-property notifications
- system-effects-property notifications
- endpoint-volume notifications

The initialization path accepts and stores Windows system-effects initialization state including endpoint property-store/device information and processing-mode context.

The APO implements the modern Windows Audio System Effects Settings/Notifications architecture rather than relying on raw CTCDC firmware writes for these effect parameters.

### Notification registration

Recovered notification descriptors include:

- endpoint property change
- endpoint volume
- system-effects property change
- another system-effects property change registration

Two recovered context GUIDs used for system-effects property-change notification registration are:

- `852311bc-1afb-454e-92ca-c35252cacaaf`
- `3f5f306b-a033-4f19-843d-1c44a736ff4d`

Their exact Creative semantic labels are not resolved here. Do not assign names without direct evidence.

## 7. Creative DSP modules are in the APO

The native APO contains concrete effect-module classes including:

- `CCrystalizerEfxMod`
- `CTHXCrystalizerEfxMod`
- `CBassManagementEfxMod`
- `CBassMgmtEfxMod`
- `CSVMEfxMod`
- `CTHXSVMEfxMod`
- `CNoiseReductionEfxMod`
- `CTDNoiseReductionEfxMod`
- `CAECEfxMod`
- `CAECRefEfxMod`
- `CMicBeamPlusEfxMod`
- `CMicBeamEfxMod`
- `CVoiceFXEfxMod`
- `CMicSigConditionEfxMod`
- `CGraphicEQEfxMod`

Recovered effect-node names include:

- `EFXNODE_CRYSTALIZER`
- `EFXNODE_THX_CRYSTALIZER`
- `EFXNODE_THX_NOISEREDUCTION`
- `EFXNODE_THX_TDNOISEREDUCTION`
- `EFXNODE_THX_MICBEAMPLUS`
- `EFXNODE_THX_MICBEAM`
- `EFXNODE_THX_AEC`
- `EFXNODE_THX_SVM`
- `EFXNODE_THX_SVM_PB`
- `EFXNODE_THX_MIC_SIGNAL_CONDITION`
- `EFXNODE_CAPX_AEC`

This is direct evidence that the actual DSP implementations for these effects live in the Creative APO user-mode processing layer.

## 8. Recovered APO COM registration CLSIDs

The supplied native APO contains registrations for multiple FX positions:

| Registration | CLSID |
|---|---|
| GFX | `{CA854A19-6601-407B-8AFB-CB5C2801AFE6}` |
| LFX | `{DA3AD2CF-79F9-41B7-BE44-753ADEEC2EDD}` |
| SFX | `{71DAB6A1-39F3-423E-90A8-032729851157}` |
| MFX | `{C624D7B2-8333-448E-85C8-51EEFC2025ED}` |
| EFX | `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}` |
| OSFX | `{BD813F37-2483-4ED1-90A8-6C4587A6AACB}` |
| OMFX | `{05800E59-C53F-487A-91A7-C3FB4B91B9E6}` |
| EffectNodeInfo | `{A14358D4-2952-4E26-8D27-8976993C4E61}` |
| AEC MFX | `{9A626D17-A2FD-40DD-876B-0F9792DE4B4F}` |
| EffectNodeInfoAEC | `{4822573B-3A8C-48F6-BF2B-678F001F934A}` |

These registrations must not be assumed to be active on the current ARM64 Microsoft USB Audio 2.0 endpoint merely because the DLL contains them. Endpoint INF/property registration remains a separate requirement.

## 9. ML assets inside the APO

The APO contains references to:

- `Creative\ShareDLL\TF\x64\tensorflow.dll`
- `dnn_msd_48000.pb`
- `dnn_snd_48000.pb`
- VAD-related processing strings

This supports the conclusion that some modern Creative capture/noise-processing modules can use ML components.

Do not generalize this into a claim that every CrystalVoice or Acoustic Engine feature uses TensorFlow.

## 10. `CTUSBfilt64.sys` role

The supplied filter driver is a small older x86-64 WDM audio filter.

Imports are limited largely to normal kernel infrastructure plus:

`drmk.sys!DrmForwardContentToDeviceObject`

Recovered structure:

- `DriverEntry` installs unload, AddDevice and generic dispatch handlers;
- AddDevice creates a filter device and attaches it to the lower device stack;
- generic IRP dispatch routes requests through an internal per-device object;
- a DRM/content-forwarding path calls `DrmForwardContentToDeviceObject`;
- embedded PDB path references `apofilt.pdb`.

No high-level Audio System Effects property repository or CrystalVoice/Acoustic DSP implementation was identified in this driver.

Correct classification:

- supporting WDM audio filter / forwarding component;
- includes DRM/content-protection-related forwarding behavior;
- not the primary high-level Creative effect DSP/property backend recovered in this trace.

Do not say the filter does nothing; it has internal IRP logic. The narrower result is that the effect modules and property-store consumer evidence are in the user-mode APO, not in this small filter.

## 11. APO `DeviceIoControl` caution

`CTUSBAPO64.dll` does contain `CreateFileW` / `DeviceIoControl` paths.

One recovered IOCTL constant is:

`0x002F0003`

with GUID:

`e8e7b1c0-eb43-4aa5-98ee-7f5db42d902f`

The exact role of this path has not yet been assigned to a specific effect.

Do **not** replace the proven property-store architecture with a claim that CrystalVoice parameters are sent directly through this IOCTL. Current evidence is stronger for property-store control of effect parameters; the IOCTL may serve device support, licensing, configuration, or another auxiliary path.

## 12. ARM64 consequences

### Controller / UI side

The property-control side can be reproduced natively on ARM64 using the Windows Audio System Effects property-store COM interfaces and the recovered Creative PROPERTYKEYs, once the target endpoint actually exposes/has the corresponding FX stores and registrations.

The managed x86-marked `Creative.Platform.CoreAudio.dll` is not required as a runtime dependency if its COM contracts are reimplemented directly.

### DSP side

Writing a Creative PROPERTYKEY does not itself recreate Crystalizer, Noise Reduction, AEC, Mic Beam, SVM, VoiceFX, etc.

Those algorithms are implemented in `CTUSBAPO64.dll` and associated native dependencies/assets.

The supplied `CTUSBAPO64.dll` is x86-64, not a native ARM64 APO. The current ARM64 machine is also known to be using the Microsoft USB Audio 2.0 path without the complete configured Creative APO/filter stack.

Therefore full feature restoration requires a compatible APO processing layer and correct endpoint FX registration/hosting, not only a controller that writes the property store.

This trace does not establish whether Windows ARM64 can transparently host this exact x86-64 APO in the audio engine. Do not assume either compatibility or incompatibility without a dedicated architecture/hosting check.

## 13. Updated backend classification

| Feature / control | Primary recovered backend |
|---|---|
| Direct Mode firmware command | CTCDC firmware |
| Graphic EQ raw firmware block | CTCDC PlaybackManager `0x96` params 9..20 |
| Normal endpoint mixer | Windows Core Audio / DeviceTopology |
| Crystalizer | Windows FX property store + Creative APO DSP |
| Surround / CMSS-style processing | Windows FX property store + Creative APO DSP |
| Smart Volume / SVM | Windows FX property store + Creative APO DSP |
| Noise Reduction | Windows FX property store + Creative APO DSP |
| AEC | Windows FX property store + Creative APO DSP |
| Mic Beam | Windows FX property store + Creative APO DSP |
| Mic SVM | Windows FX property store + Creative APO DSP |
| VoiceFX | Creative APO DSP; exact Platform key mapping still to finish |
| CTUSBfilt64 | supporting WDM filter / forwarding layer |

The existence of both a firmware GEQ route and APO GraphicEQ property keys means not every named feature necessarily has only one implementation path. Product/endpoint-specific orchestration decides which path is active.

## 14. Remaining unknowns / next static work

Do not add a broad runtime probe yet.

Remaining high-value static work:

1. recover the exact `PROPVARIANT` type, range, scale and default for each X4-relevant APO key;
2. determine which of the many legacy/current Creative key families the X4 endpoint repository actually selects;
3. trace product/endpoint feature selection in `ApoDeviceRepositoryInitializer`, feature enrichers and support predicates;
4. recover endpoint APO registration/property requirements for SFX/MFX/EFX/AEC processing positions;
5. determine which extra native dependencies/assets are required by the relevant APO modules;
6. separately resolve the CDC Game/Voice raw `UInt16` engineering-unit conversion; this APO trace does not prove `/256`.

A later read-only endpoint FX property-store enumeration can be justified only after the static product/key selection is narrowed enough to make that test targeted.

## Safety boundary

- No mixer SET from this trace.
- No raw `0x95` repetition.
- No B5 ASIO changes.
- No assumption that an x64 APO DLL is active on the current ARM64 endpoint merely because its binary was supplied.
- Keep firmware, Core Audio property, APO DSP, and App orchestration layers separately classified.
