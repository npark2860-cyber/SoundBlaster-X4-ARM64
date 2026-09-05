# DEBUG HISTORY — 2026-09-05 X4 APO Property Schema Static Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace continues the recovered Creative Windows APO control path and closes the remaining static questions needed for an exact property-store schema:

1. how SB1815/X4 becomes eligible for an APO repository;
2. the endpoint APO-definition property used by the support filter;
3. the `PROPVARIANT` types used by `PropStoreRepository`;
4. exact feature parameter ranges/defaults recovered from the Creative Platform feature enrichers;
5. the repository-dependent XBass range selection;
6. the exact Smart Volume / SVM mode representation.

This is static analysis only. No hardware runtime probe was added, no FX property was written, no CTCDC SET was issued, and no B5 ASIO code was modified.

## Analysis basis

Exact Creative binaries used:

- `Creative.Platform.Devices.dll` SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.Platform.CoreAudio.dll` SHA-256 `189ee6750a7e70f24421f1e2100fa88847878456f11233e28ed2fa0a6d1d4823`
- `CTUSBAPO64.dll` SHA-256 `fa23a53861087df19487497c54067128f61a266cce2eae000f1a40b8752a17d3`

Related prior trace:

`DEBUG_HISTORY_20260905_X4_APO_CRYSTALVOICE_BACKEND_STATIC_TRACE.md`

## 1. Result summary

Static recovery now establishes the following X4/SB1815 APO-control facts:

- APO support is not a blind SB1815 boolean. `APODeviceFilter` reads an endpoint APO-definition property and evaluates the recovered APO information against Creative's supported-model table.
- The APO-definition property family is `{f1056047-b091-4d85-a5c0-b13d4d8bac57}` with PID 0 for Render and PID 1 for Capture.
- Creative's supported-model table maps APO hardware identifier `100` to product code `SB1815`.
- Once the support path succeeds, the endpoint model is allowed to carry `EDeviceRepositoryType.Apo` and `ApoDeviceRepositoryInitializer` can create/attach `PropStoreRepository`.
- `PropStoreRepository` writes booleans as `VT_BOOL`, floats as `VT_R4`, float arrays as `VT_VECTOR | VT_R4`, and strings as `VT_LPWSTR`.
- Smart Volume / SVM mode is a float property, not an integer: Normal=`0.0`, Loud=`1.0`, Night=`2.0`.
- XBass strength has two repository-specific public ranges. If an APO repository exists, the feature uses `0..100`, step `1`, default `50`. HID-only uses `0..1`, step `0.01`, default `0.5`.

The XBass result is an important correction to the earlier tentative intuition that APO would use the normalized 0..1 range. Static control flow shows the opposite.

## 2. APO support gate for SB1815/X4

### Endpoint APO-definition PROPERTYKEY

`PropStoreHelper` constructs two keys from the same GUID:

`{f1056047-b091-4d85-a5c0-b13d4d8bac57}`

- Render APO definition: PID `0`
- Capture APO definition: PID `1`

`APODeviceFilter.IsSupportedAsync` selects the Render or Capture key according to endpoint direction and reads the endpoint property before building its APO information object.

### Supported-model mapping

The recovered Creative supported-model table maps:

- APO hardware identifier `100` -> `SB1815`

This establishes the product-specific static gate used by the official Platform code.

Important scope rule:

This does **not** prove that the current Microsoft-USB-Audio ARM64 endpoint is presently configured with that APO-definition property. That is a runtime/installation-state question. The static result is that an official SB1815 endpoint with APO HW identifier 100 is recognized as supported.

### Repository enum values relevant to this trace

Recovered `EDeviceRepositoryType` values:

- Unknown = 0
- Apo = 1
- SoundCore = 2
- CDC = 3
- Lighting = 4
- HID = 5
- Mixer = 6
- Airoha = 7

These values are directly relevant to the XBass branch below.

## 3. PropStoreRepository value encoding

`PropStoreRepository` is the official Platform repository behind the recovered Audio System Effects user property store.

Static writer logic maps managed values to `PROPVARIANT` types as follows:

| Managed value | PROPVARIANT |
|---|---|
| `Boolean` | `VT_BOOL` = 11 |
| `Single` / `float` | `VT_R4` = 4 |
| `Single[]` / `float[]` | `VT_VECTOR | VT_R4` = `0x1004` |
| `String` | `VT_LPWSTR` = 31 |

Consequences for a future ARM64 controller:

- an effect toggle must not be written as an arbitrary DWORD when the official model expects `VT_BOOL`;
- level/strength/angle/frequency parameters recovered as `float` must be carried as `VT_R4`;
- GEQ-like float vectors, where that repository form is used, require `VT_VECTOR | VT_R4`.

This trace does not authorize state-changing writes. It only fixes the required schema.

## 4. Smart Volume / SVM mode — exact type and values

`Creative.Platform.Devices.Features.ApoSmartVolumeFeature` contains a `Dictionary<SVMMode,float>` and constructs the exact mapping:

- Normal -> `0.0f`
- Loud -> `1.0f`
- Night -> `2.0f`

`GetSvmMode()` calls `IDeviceRepository.GetValue<float>` and `SetSvmMode()` calls `IDeviceRepository.SetValue<float>`.

Therefore the official repository representation is:

- type: `float` / `VT_R4`
- Normal = `0.0`
- Loud = `1.0`
- Night = `2.0`

The property key selected by `ApoDeviceRepoKeyFactory` for this path is `CTPKEY_DynamX_SVM_Mode`:

- GUID `{e6ec3743-ddd2-4817-8466-b433761dcf9d}`
- PID `0`

Do not serialize this mode as an integer enum in the FX property store.

## 5. XBass strength — repository-aware range selection

The flattened async state machine for `XBassXFi23FeatureEnricher` was followed through its repository checks.

It evaluates:

- `DeviceRepositories.ContainsKey(EDeviceRepositoryType.Apo)` -> repository type `1`
- `DeviceRepositories.ContainsKey(EDeviceRepositoryType.HID)` -> repository type `5`

The initial range object is:

- min `0.0`
- max `1.0`
- step `0.01`
- default `0.5`

When the APO repository exists, that range is overwritten with:

- min `0.0`
- max `100.0`
- step `1.0`
- default `50.0`

Therefore:

| Repository condition | XBass Strength range |
|---|---|
| APO present | `0..100`, step `1`, default `50` |
| HID-only path | `0..1`, step `0.01`, default `0.5` |

This is static control-flow recovery, not a naming inference.

The APO property selected by `ApoDeviceRepoKeyFactory` is `CTPKEY_BassMgmt_XBassStrength`:

- GUID `{dd527e35-21a5-4ca6-ab90-8ad464fb55e3}`
- PID `0`

The same feature enricher constructs the crossover range:

- min `10`
- max `1000`
- step `1`
- default `80`

## 6. Recovered effect parameter ranges/defaults

The following parameter schemas were recovered from the Creative Platform feature enrichers. Unless separately noted, these are float parameters and therefore use `VT_R4` in `PropStoreRepository`.

| Feature parameter | Min | Max | Step | Default |
|---|---:|---:|---:|---:|
| Crystalizer Level | 0 | 1 | 0.01 | 0.65 |
| Crystalizer PreAttenuation | 0 | 1 | 0.01 | 1.0 |
| Noise Reduction Strength | 0 | 1 | 0.01 | 0.5 |
| Mic Smart Volume Strength | 0 | 1 | 0.01 | 0.74 |
| Surround Immersion | 0 | 1 | 0.01 | 0.4 |
| Dialog Plus Strength | 0 | 1 | 0.01 | 0.05 |
| Voice Focus / MicBeam wedge | 20 | 180 | 1 | 30 |
| Bass crossover | 10 | 1000 | 1 | 80 |

Enable/disable properties for the corresponding APO feature models are booleans and therefore use `VT_BOOL`.

### XBass exception

XBass Strength must use the repository-aware ranges documented in section 5 rather than blindly applying a normalized 0..1 assumption.

## 7. VoiceFX property family and ranges

VoiceFX uses the common property GUID:

`{f7e70860-8eb1-4c6a-b2e1-1033b409ff5d}`

- Enable: PID `99`, boolean / `VT_BOOL`
- parameter PIDs `0..7`: float / `VT_R4`

Recovered parameter mapping:

| PID | Parameter | Min | Max | Default |
|---:|---|---:|---:|---:|
| 0 | FormantFreq1 | 200 | 800 | 400 |
| 1 | FormantFreq2 | 800 | 1800 | 1400 |
| 2 | FormantFreq3 | 1600 | 2750 | 2000 |
| 3 | PitchFactor | 0.25 | 4 | 1 |
| 4 | FormantFactor | 0.5 | 2 | 1 |
| 5 | PitchVariability | -1 | 5 | 1 |
| 6 | QuiverDepth | 0 | 0.2 | 0 |
| 7 | ContourDepth | 0 | 0.4 | 0 |

Only the statically recovered min/max/default values are recorded here. Do not invent step values where the exact step was not separately recovered.

## 8. Selected exact keys relevant to the schema

The prior APO backend trace already recovered the broader key set. The following subset is restated because its value schema is now resolved:

| Feature | PROPERTYKEY | Value type / schema |
|---|---|---|
| Crystalizer Enable | `{3cd83c04-868f-4f08-8d75-b4625ffe3b31},0` | `VT_BOOL` |
| Crystalizer Level | `{0f03f0bb-72c7-4ec1-8422-7b8d7410694a},0` | `VT_R4`, 0..1 |
| SVM Enable | `{9ad782d7-f46e-465c-8df5-3cda75424987},0` | `VT_BOOL` |
| DynamX SVM Mode | `{e6ec3743-ddd2-4817-8466-b433761dcf9d},0` | `VT_R4`, 0/1/2 |
| XBass Strength | `{dd527e35-21a5-4ca6-ab90-8ad464fb55e3},0` | `VT_R4`, repository-aware range |
| AEC Enable | `{35f00393-1adf-43ce-84cb-7a926ac012b6},0` | `VT_BOOL` |
| Noise Reduction Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642},1` | `VT_BOOL` |
| Noise Reduction Strength | `{6a72f5dd-6c09-4147-82c5-14c64b0e4e0f},0` | `VT_R4`, 0..1 |
| MicBeam Plus Enable | `{40d0d021-20bd-4d15-a93c-1dbe8922c642},0` | `VT_BOOL` |
| MicBeam Wedge Angle | `{72e09675-2af9-485c-89f1-898e532bf06e},0` | `VT_R4`, 20..180 |
| VoiceFX Enable | `{f7e70860-8eb1-4c6a-b2e1-1033b409ff5d},99` | `VT_BOOL` |
| VoiceFX parameters | same GUID, PID 0..7 | `VT_R4` |

## 9. Runtime/implementation consequence

The official Platform schema is now sufficiently precise to build a **targeted read-only** ARM64 Audio System Effects property-store inspector later:

1. obtain the candidate X4 render/capture endpoint;
2. inspect the APO-definition property `{f1056047-b091-4d85-a5c0-b13d4d8bac57}` with direction-specific PID;
3. if an Audio System Effects property store is available, read only the exact known X4-relevant keys using their recovered `PROPVARIANT` types;
4. do not enumerate or write arbitrary properties.

However, the current ARM64 machine is known to use the Microsoft USB Audio 2.0 path without the complete configured Creative APO stack. Absence of the recovered property family in that environment would describe installation state, not prove the X4 product lacks the feature.

Property-store control also does not recreate the DSP by itself. `CTUSBAPO64.dll` contains the actual x86-64 effect implementation.

## 10. Remaining work

With the property schema closed, the next APO questions are:

1. recover the X4 Creative installation/INF registration that binds SFX/MFX/EFX/AEC positions and the recovered APO CLSIDs to the endpoint;
2. determine whether Windows ARM64 audio-engine APO hosting can use this exact x86-64 `CTUSBAPO64.dll`, or whether an ARM64 processing replacement is required;
3. inventory native dependencies/assets needed by the X4-relevant APO modules.

The unrelated CDC Game/Voice raw UInt16 engineering-unit conversion remains unresolved and must stay separate.

## Safety conclusions

- No new CTCDC runtime probe is justified by this trace.
- Do not repeat raw `0x95` probing.
- Do not write any APO property solely because its schema is now known.
- No B5 ASIO code/path is involved.
