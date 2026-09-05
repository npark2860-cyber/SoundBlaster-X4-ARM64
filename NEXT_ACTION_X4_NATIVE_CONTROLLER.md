# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Immediate priority

The official `AudioLevel (0x23)` Windows call-path split has now been statically recovered from the supplied exact Creative binaries.

Read first:

`DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`

### Static-confirmed `0x23` facts

- GET frame: `5A 23 02 01 <index>`.
- Official `RawResAudioLevelGet` payload: `AudioControlIndex : byte` + `CurValue : UInt16`, packed size 3.
- The runtime trailing `0x03` is not part of that managed response structure and is ignored by the official managed decode; its firmware semantic meaning remains unresolved.
- Creative Platform creates `0x23` GET/SET keys only for discovered `CDCGameVoice.GameIndex` and `VoiceIndex`.
- Those indices come from `AudioControlType.GameAudioLevel (19)` and `ChatAudioLevel (18)` descriptors.
- The runtime X4 descriptor list contains neither type 18 nor type 19.
- General Speaker/Input/Monitoring level control is routed through `Creative.Platform.Mixer.dll` / `MalLgcy.dll` Windows endpoint/topology/KS APIs, not generic per-`0x21`-index `0x23` reads.

Therefore:

- do not repeat generic `0x23` probing across indices `0..9`;
- do not interpret index `2..9` `GeneralFailure` as missing volume support;
- do not issue `0x23` SET.

## First remaining AudioLevel task

Recover the exact official engineering-unit conversion for the CDC raw `UInt16` level/range representation.

Current exact evidence:

- `RawResAudioLevelGet.GetValue()` returns raw `UInt16 CurValue` unchanged;
- `AudioControlLevelRange` carries raw `UInt16` Min/Max/Step unchanged;
- `CDCGameVoiceFeature` passes those `UInt16` ranges through without converting to dB inside `Creative.Platform.Devices.dll`.

The observed hardware values are compatible with a signed fixed-point representation, but do **not** hard-code `/256` as confirmed until its actual conversion code is recovered.

Static targets for this remaining point:

1. higher Creative App/UI conversion code that consumes CDC Game/Voice level values;
2. related Creative Platform assemblies if they contain the conversion helper;
3. native code only where there is direct evidence it consumes this CDC `UInt16` representation.

No new hardware probe is required first.

## Windows Mixer backend — already resolved at managed/native boundary

`Creative.Platform.Mixer.dll` calls native `MalLgcy.dll`.

Recovered native API contracts distinguish `bool fScalar`:

- endpoint master/channel and monitoring wrappers use `fScalar=true`;
- normalized float is converted to/from managed 0..100;
- Mic Boost uses KS node volume with `fScalar=false`;
- KS/monitor range parameter names explicitly use `MinLevelDB` / `MaxLevelDB`.

If `MalLgcy.dll` becomes available, inspect it to complete native implementation provenance. This is not required to re-prove the already-recovered scalar/dB contract.

## Secondary priority — backend classification

After the remaining CDC raw-unit conversion is closed, continue classifying X4 features by backend:

1. firmware / CTCDC;
2. Windows Core Audio endpoint/property;
3. Creative filter/APO/driver processing;
4. app/profile orchestration.

CrystalVoice and non-EQ Acoustic Engine controls are the next important targets.

Current evidence does **not** prove that X4 lacks CrystalVoice. It only shows that the tested raw Malcolm `VoiceInputManager (0x95)` path did not respond and that the current ARM64 machine lacks the complete official Creative filter/APO stack.

Trace through:

- `Creative.Platform.Devices.dll`
- `Creative.Platform.Mixer.dll`
- `Creative.Platform.CoreAudio.dll`
- `CTUSBAPO64.dll`
- `CTUSBfilt64.sys`
- Creative KS/property GUID paths already identified in earlier static analysis

Do not repeat blind `0x95` probing without new backend evidence.

## Runtime safety rules

- Creative App must be fully closed for independent CTCDC runtime tests.
- Keep read-only tests read-only unless a specific state-changing command has been statically justified.
- Every new state-changing hardware command requires physical X4 confirmation.
- `WriteFile` success alone is not physical validation.
- Do not weaken or alter B5 ASIO runtime behavior.
- Do not change unrelated paths.

## Known live firmware routes

- Direct Mode: `0x39`, hardware-confirmed ON/OFF.
- Graphic EQ state: `0x11`, module `0x96`, params `9..20`, hardware-read confirmed.
- Mixer AudioControl descriptor/range/mute: `0x21/0x22/0x24`, hardware-read confirmed.
- `0x23`: official Creative Platform use is Game/Voice-indexed; do not treat it as generic Windows mixer level control.

## Known rejected / unsupported routes in current session

- direct GraphicEqualizerControl `0x44`: ACK `0x81 NotSupported`.
- direct SoundModeControl `0xA7`: ACK `0x81 NotSupported`.
- raw VoiceInputManager `0x95` params `0..45`: no response.
- naked COM Direct Mode without CTCDC session setup: rejected previously.
- HID / BLE / UAC Extension Unit / vendor-interface guesses: do not repeat without new evidence.
