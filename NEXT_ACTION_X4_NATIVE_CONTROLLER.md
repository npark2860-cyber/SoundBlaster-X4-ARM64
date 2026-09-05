# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`
2. `DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`
3. `DEBUG_HISTORY_20260905_CTAUDEP_WINDOWS_MIXER_NATIVE_TRACE.md`
4. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
5. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`

## Current architecture — recovered

Keep these Windows-side control paths separate.

### A. CTCDC firmware

Confirmed examples:

- Direct Mode `0x39`;
- firmware Graphic EQ via PlaybackManager `0x96` params `9..20`;
- AudioControl descriptor/range/mute discovery via `0x21/0x22/0x24`;
- official `0x23` Platform usage is Game/Voice-indexed, not generic Windows mixer control.

Do not repeat broad raw probing.

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

Recovered official Windows control path:

`Creative App / Platform`
-> `ApoDeviceRepoKeyFactory`
-> `PropStoreRepository`
-> `IAudioSystemEffectsPropertyStore::OpenUserPropertyStore`
-> `IPropertyStore::GetValue/SetValue`
-> Windows Audio System Effects notification
-> `CTUSBAPO64.dll` DSP

This is the primary recovered Windows path for Crystalizer, Surround, Smart Volume/SVM, Noise Reduction, AEC, MicBeam, Mic SVM and VoiceFX.

Raw `VoiceInputManager (0x95)` no-response is therefore not proof that these features are globally unsupported.

## APO property schema — static recovery complete

Full trace:

`DEBUG_HISTORY_20260905_X4_APO_PROPERTY_SCHEMA_STATIC_TRACE.md`

### SB1815 APO support gate

Endpoint APO-definition PROPERTYKEY family:

`{f1056047-b091-4d85-a5c0-b13d4d8bac57}`

- Render PID `0`
- Capture PID `1`

`APODeviceFilter` reads the direction-specific endpoint property and evaluates the returned APO information against Creative's supported-model mapping.

Recovered table:

- APO hardware identifier `100` -> `SB1815`

This describes the official support predicate. It does not prove that the current Microsoft USB Audio 2.0 ARM64 endpoint is presently configured with that property.

### Repository types relevant to current work

- `Apo = 1`
- `CDC = 3`
- `HID = 5`
- `Mixer = 6`

### Official PropStore value encodings

`PropStoreRepository` writes:

- Boolean -> `VT_BOOL (11)`
- Single/float -> `VT_R4 (4)`
- float[] -> `VT_VECTOR | VT_R4 (0x1004)`
- string -> `VT_LPWSTR (31)`

Do not use arbitrary DWORD types for APO toggles or levels.

### Smart Volume / SVM mode

SVM mode is definitively float / `VT_R4`:

- Normal = `0.0`
- Loud = `1.0`
- Night = `2.0`

`CTPKEY_DynamX_SVM_Mode`:

`{e6ec3743-ddd2-4817-8466-b433761dcf9d}, PID 0`

### XBass range correction

The feature enricher checks repository presence explicitly.

- APO present -> XBass Strength `0..100`, step `1`, default `50`
- HID-only -> XBass Strength `0..1`, step `0.01`, default `0.5`

This is the opposite of an earlier tentative intuition and is now statically resolved.

APO XBass Strength key:

`{dd527e35-21a5-4ca6-ab90-8ad464fb55e3}, PID 0`

### Recovered X4-relevant parameter ranges

| Parameter | Range / step / default |
|---|---|
| Crystalizer Level | `0..1`, `0.01`, `0.65` |
| Crystalizer PreAttenuation | `0..1`, `0.01`, `1.0` |
| Noise Reduction Strength | `0..1`, `0.01`, `0.5` |
| Mic Smart Volume Strength | `0..1`, `0.01`, `0.74` |
| Surround Immersion | `0..1`, `0.01`, `0.4` |
| Dialog Plus Strength | `0..1`, `0.01`, `0.05` |
| Voice Focus / MicBeam wedge | `20..180`, step `1`, default `30` |
| Bass crossover | `10..1000`, step `1`, default `80` |

Enable properties use `VT_BOOL`; the listed numeric parameters use `VT_R4`.

VoiceFX uses common GUID `{f7e70860-8eb1-4c6a-b2e1-1033b409ff5d}`, Enable PID 99 (`VT_BOOL`), and parameter PIDs 0..7 (`VT_R4`). See the property-schema trace for exact min/max/default values.

## Immediate priority 1 — recover Creative APO registration / endpoint binding

The property schema itself is now sufficiently resolved. Do not spend another turn re-deriving the same ranges.

Next static target:

1. recover installation/INF or endpoint registration that binds the Creative APO CLSIDs to SB1815 render/capture endpoints;
2. determine which SFX/MFX/EFX/AEC positions SB1815 actually registers;
3. identify the exact endpoint properties used for those registrations;
4. inventory native DLL/model/config dependencies needed by the X4-relevant APO modules.

Known APO CLSIDs from the supplied binary include:

- GFX `{CA854A19-6601-407B-8AFB-CB5C2801AFE6}`
- LFX `{DA3AD2CF-79F9-41B7-BE44-753ADEEC2EDD}`
- SFX `{71DAB6A1-39F3-423E-90A8-032729851157}`
- MFX `{C624D7B2-8333-448E-85C8-51EEFC2025ED}`
- EFX `{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}`
- OSFX `{BD813F37-2483-4ED1-90A8-6C4587A6AACB}`
- OMFX `{05800E59-C53F-487A-91A7-C3FB4B91B9E6}`
- AEC MFX `{9A626D17-A2FD-40DD-876B-0F9792DE4B4F}`

Binary presence alone does not prove which are registered for X4.

## Immediate priority 2 — Windows ARM64 APO hosting feasibility

The controller/property-write side can be native ARM64, but property writes do not recreate the DSP.

The supplied `CTUSBAPO64.dll` is x86-64 and contains the actual Creative effect algorithms.

Determine with official Windows APO architecture/registration evidence whether the Windows ARM64 audio engine can host this exact x86-64 APO in-process, or whether an ARM64 APO/replacement processing layer is required.

Do not infer this from normal x64 application emulation. Audio-engine APO hosting is a separate compatibility question.

## Targeted runtime gate — not yet automatic authorization

The recovered property schema is now precise enough to design a narrow **read-only** FX property-store inspector later.

A justified read-only inspection would:

1. locate the X4 render/capture endpoint;
2. read only the direction-specific APO-definition key;
3. attempt Audio System Effects property-store access;
4. read only the exact known feature keys using their recovered `PROPVARIANT` types.

Do not create or run a broad property enumerator, and do not write values, until installation/registration expectations are understood.

## Remaining CDC AudioLevel task

The CDC Game/Voice raw `UInt16` engineering-unit conversion remains separate and unresolved.

Known facts:

- `RawResAudioLevelGet` returns raw `UInt16` unchanged;
- `AudioControlLevelRange` carries raw `UInt16` unchanged;
- `CDCGameVoiceFeature` does not convert it;
- MalLgcy/CTAudEp do not consume this representation;
- the APO property-store path is unrelated.

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
- Keep runtime work read-only unless a state-changing operation has exact static evidence and separate validation intent.
- Every new state-changing hardware command requires physical X4 confirmation.
- `WriteFile` success alone is not hardware validation.
- Do not repeat blind raw `0x95` probing.
- Do not modify B5 ASIO from this branch.
- Do not change unrelated paths.
