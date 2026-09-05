# DEBUG HISTORY — 2026-09-05 X4 AudioLevel 0x23 Static Trace

Branch:

`exp/windows-arm64-x4-native-controller`

## Scope

This trace resolves the official Windows static call path for `CDCRawCommand.AudioLevel (0x23)` using the supplied Creative binaries. No hardware runtime probe was added, no mixer SET was issued, and no B5 ASIO code was modified.

## Analysis basis

Supplied binaries:

- `Creative.Platform.Devices.dll` SHA-256 `2d77172fb6ae850b6d03a09830892c8c3a0ab79e10dda28f40a76b3fadc47e93`
- `Creative.Platform.Mixer.dll` SHA-256 `33f6ac6c84e093c766e8b483660d49518a8a0c14da144bd7a6a4f8bf0a79ae45`
- `CTCDC.dll` SHA-256 `bc4010e8f7000bfe6217425a0622dd710a7626d90fb61008505337aa87a43dab`
- `CTIntrfu.dll` SHA-256 `ecf098101a0663568f4a406d7bed9775565a67213930e2487c17d858a5d0d9b6`

`Creative.Platform.Devices.dll`, `CTCDC.dll`, and `CTIntrfu.dll` match the previously recorded controller baseline hashes exactly.

## 1. Result summary

The previous interpretation of `0x23` as a generic per-`AudioControlIndex` Windows mixer getter is not the official Creative Platform call path.

Static recovery shows:

1. the exact `0x23` GET request body is two bytes: `Operation=Get (1)`, `AudioControlIndex`;
2. the exact complete frame is `5A 23 02 01 <index>`;
3. the official managed `RawResAudioLevelGet` payload is exactly three bytes: `AudioControlIndex` + `UInt16 CurValue`;
4. the fourth runtime byte `0x03` is not a field in the official managed response structure and is ignored by the managed parser;
5. Creative Platform creates `0x23` GET/SET keys only for the discovered `CDCGameVoice.GameIndex` and `VoiceIndex`;
6. those indices are selected from `AudioControlType.GameAudioLevel (19)` and `ChatAudioLevel (18)` descriptors;
7. the runtime X4 `0x21` descriptor list contains neither type 18 nor type 19;
8. general Speaker/Input/Monitoring Windows mixer controls are handled through `Creative.Platform.Mixer.dll` and its `MalLgcy.dll` endpoint/topology/KS APIs, not by treating every `0x21` control index as a generic `0x23` target.

Therefore the runtime result where index 0/1 responded and index 2..9 returned `GeneralFailure` must not be interpreted as evidence that indices 2..9 lack volume support.

## 2. Exact `0x23` managed structures

All relevant structures are packed with `StructLayout(Pack=1)`.

### `RawCmdAudioLevelGet`

Fields:

- `Operation : byte`
- `AudioControlIndex : byte`

For `DevCommOperation.Get = 1`, body:

`01 <index>`

Wrapped by `RawCmd5A<T>`:

- `StartId : byte`
- `CommandId : CDCRawCommand`
- `Length : byte`
- `Data : T`

Exact complete GET frame:

`5A 23 02 01 <index>`

### `RawCmdAudioLevelSet`

Fields:

- `Operation : byte`
- `AudioControlIndex : byte`
- `NewValue : UInt16`

This structure was recovered statically only. No SET is authorized by this trace.

### `RawResAudioLevelGet`

Fields:

- `AudioControlIndex : byte`
- `CurValue : UInt16`

Packed payload size: **3 bytes**.

`GetValue()` returns the raw `UInt16 CurValue` without engineering-unit conversion.

### `AudioLevelRange`

Fields:

- `AudioControlIndex : byte`
- `MaxValue : UInt16`
- `MinValue : UInt16`
- `StepValue : UInt16`

### `RawCmdAudioLevelRangesGet`

Fields:

- `Count : byte`
- `Indices : fixed byte[32]`

Packed body size: 33 bytes.

## 3. Meaning of the runtime fourth byte

The successful X4 runtime responses for raw index 0/1 contained four payload bytes, ending in `0x03`.

The exact official Windows managed model disproves two previous possibilities:

- it is not a fourth member of `RawResAudioLevelGet`;
- it is not managed struct alignment/padding because the response is `Pack=1` and has a three-byte packed payload.

`CDCConnection` reads the generic raw response header and unmarshals the response data beginning at payload offset 3 into the statically known response type. For `RawResAudioLevelGet`, only the three modeled bytes are consumed by that structure.

Therefore the trailing runtime `0x03` is an extra firmware response byte/extension beyond the Windows managed response model. Its semantic meaning is **not resolved by these four binaries**. The official Windows managed code does not require it to decode `RawResAudioLevelGet`.

Do not assign a channel-mask or other semantic meaning to `0x03` without additional direct evidence.

## 4. Official Creative Platform `0x23` call path

The repository initialization path does not create one `0x23` command key for every `AudioControl` descriptor.

It obtains:

- `CDCGameVoice.GameIndex`
- `CDCGameVoice.VoiceIndex`

and creates exactly the corresponding `RawCmdAudioLevelGet` / `RawCmdAudioLevelSet` key pairs.

The same two indices are passed to `RawMsgConverter.SetGameVoiceIndices(...)`.

Incoming `RawResAudioLevelGet` handling compares `AudioControlIndex` only against those stored Game/Voice indices before emitting the corresponding value-change event. There is no generic callback branch that maps arbitrary `AudioControlIndex` values to normal Windows mixer lines.

## 5. How Game/Voice indices are selected

Recovered `AudioControlType` values relevant here:

- `ChatAudioLevel = 18`
- `GameAudioLevel = 19`

The feature-checker searches the enumerated `AudioControl` descriptors for these exact types:

- type `19` -> `GameIndex`
- type `18` -> `VoiceIndex`

It then associates the matching `0x22` level ranges with the Game/Voice feature.

The hardware runtime X4 descriptor set was:

- 0 Speaker
- 1 Headphone
- 2 SPDIF Output
- 3 Mic Monitoring
- 4 Line Monitoring
- 5 Mic Input
- 6 Line Input
- 7 What U Hear Recording
- 8 SPDIF Monitoring
- 9 SPDIF Input
- 10 Automatic Gain Control

There is no `ChatAudioLevel (18)` or `GameAudioLevel (19)` descriptor in that runtime set.

Consequently, the official Windows `CDCGameVoice` `0x23` path has no Game/Voice descriptor target in this X4 enumeration. Raw firmware behavior seen when manually supplying index 0/1 is not equivalent to Creative Platform's generic Windows mixer architecture.

## 6. General Windows Mixer backend

`Creative.Platform.Mixer.dll` uses `ICTMalLgcyLibrary`, backed by native `MalLgcy.dll`, for ordinary Windows audio endpoint/topology control.

`MixerRepositoryInitializer` constructs a `MixerLine` from the selected `IDeviceEndpoint.DeviceEndpointId` and then discovers:

- monitoring lines;
- Mic Boost;
- Mic AGC.

This establishes a separate endpoint/topology path for normal mixer controls.

### Endpoint master/channel level

`MixerLine` calls:

- `CSCTGetMasterVolume(..., bool fScalar, out float pfLevel)`
- `CSCTSetMasterVolume(...)`
- `CSCTGetChannelVolume(..., bool fScalar, uint channel, out float pfLevel)`
- corresponding channel setter.

Creative's managed wrapper uses `fScalar=true`, receives normalized float level, and presents it as 0..100 by multiplying by 100. Set performs the inverse division by 100.

### Monitoring level

`MonitorLine` opens monitoring controls for the selected endpoint and calls the monitoring-level APIs using `fScalar=true`.

It likewise converts normalized scalar float values to/from the managed 0..100 representation.

The native metadata names for monitoring range outputs are explicitly:

- `pflMinLevelDB`
- `pflMaxLevelDB`
- `pflStepping`

### Mic Boost

Mic Boost is routed through KS node type volume APIs.

Recovered native signatures include:

- `CSCTGetLevelRangeOfKsNodeTypeVolumeOfAudioEndpoint(..., out float pfMinLevelDB, out float pfMaxLevelDB, out float pfStepping)`
- `CSCTGetLevelOfKsNodeTypeVolumeOfAudioEndpoint(handle, bool fScalar, out float pfLevel)`

The Mic Boost wrapper calls level GET/SET with `fScalar=false`, so its managed value is on the non-scalar/dB path.

### Mic AGC

Mic AGC is handled through the corresponding KS-node Auto Gain Control APIs, not through generic raw `0x23` AudioLevel reads.

## 7. Endpoint/channel gating conclusion

For the general Windows Mixer path, control selection is explicitly endpoint/topology based:

- select `DeviceEndpointId`;
- open/use the endpoint master or channel control;
- or open the monitoring/KS node belonging to that endpoint.

Therefore source/recording/monitoring controls do not need an undocumented extra byte appended to `RawCmdAudioLevelGet`. They use a different backend/call path.

Do not attribute the raw `0x23` index 2..9 `GeneralFailure` specifically to an active-endpoint gate. The stronger static result is that those generic controls are not the official Creative Platform `CDCGameVoice` `0x23` targets in the first place.

## 8. Engineering units

### General Windows Mixer path — resolved

The `MalLgcy` API contract explicitly distinguishes:

- `fScalar=true` -> scalar normalized level;
- `fScalar=false` -> non-scalar/dB level.

Creative's `MixerLine` and `MonitorLine` use scalar mode and expose 0..100 to the managed layer. Mic Boost uses the dB path.

### CDC `0x22/0x23` raw UInt16 path — not yet fully resolved

`Creative.Platform.Devices.dll` does not convert `AudioLevelRange` or `RawResAudioLevelGet.CurValue` into dB. It carries the raw `UInt16` Min/Max/Step/current values through the CDC Game/Voice feature configuration.

The captured X4 values are numerically compatible with a signed fixed-point interpretation, but the exact official raw-UInt16-to-dB conversion code has not been recovered from these four binaries. Do not hard-code or document a `/256` conversion as confirmed yet.

## 9. Native DLL roles

### `CTCDC.dll`

Native CDC/serial firmware transport. Static strings and exports confirm the established session/passthrough role, including COM initialization, raw command execution, incoming parser, passthrough data and Malcolm command handling.

It is not the general Windows Core Audio mixer backend.

### `CTIntrfu.dll`

Creative component/object activation and registry infrastructure (`CTCreateInstance`, object/component enumeration and related functions). No AudioLevel-specific engineering-unit or Windows mixer semantics were found here.

### `MalLgcy.dll`

This is the native dependency actually used by `Creative.Platform.Mixer.dll` for endpoint, monitoring and KS-node mixer operations. It was not part of the four supplied binaries in this trace.

## 10. Consequences and next action

Confirmed consequences:

- Do not send generic raw `0x23` GETs across all `0x21` indices as the model for the final controller.
- Do not interpret index 2..9 `GeneralFailure` as missing volume capability.
- Do not infer a semantic meaning for the trailing runtime `0x03`.
- Do not send `0x23` SET.
- No new hardware probe is required to establish the call-path split above.

Remaining static work for AudioLevel:

1. recover the exact official fixed-point/engineering-unit conversion for CDC `UInt16` range/current values from the appropriate higher/native layer;
2. inspect `MalLgcy.dll` if available to complete native implementation provenance for the already-recovered Windows mixer APIs.

After that, continue the planned CrystalVoice / non-EQ Acoustic Engine backend classification through Creative Platform/CoreAudio, Creative APO/filter and KS/property paths.
