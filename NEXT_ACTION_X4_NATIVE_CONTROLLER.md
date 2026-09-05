# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Read first

1. `DEBUG_HISTORY_20260905_X4_AUDIOLEVEL_STATIC_TRACE.md`
2. `DEBUG_HISTORY_20260905_MALLGCY_NATIVE_FORWARD_TRACE.md`

## Immediate priority

The official `AudioLevel (0x23)` Windows call-path split has been statically recovered from the supplied exact Creative binaries.

### Static-confirmed `0x23` facts

- GET frame: `5A 23 02 01 <index>`.
- Official `RawResAudioLevelGet` payload: `AudioControlIndex : byte` + `CurValue : UInt16`, packed size 3.
- The runtime trailing `0x03` is not part of that managed response structure and is ignored by the official managed decode; its firmware semantic meaning remains unresolved.
- Creative Platform creates `0x23` GET/SET keys only for discovered `CDCGameVoice.GameIndex` and `VoiceIndex`.
- Those indices come from `AudioControlType.GameAudioLevel (19)` and `ChatAudioLevel (18)` descriptors.
- The runtime X4 descriptor list contains neither type 18 nor type 19.
- General Speaker/Input/Monitoring level control is routed through the Windows endpoint/topology path, not generic per-`0x21`-index `0x23` reads.

Therefore:

- do not repeat generic `0x23` probing across indices `0..9`;
- do not interpret index `2..9` `GeneralFailure` as missing volume support;
- do not issue `0x23` SET.

## MalLgcy native trace — completed

Supplied `MalLgcy.dll`:

- SHA-256 `bf2ba6d85fa1cdf20a2fa866d153cefa1e5e7f9af87107d83963ed393e4591aa`
- x86 / PE32

The relevant `CSCT*` functions are thin wrappers that forward the original parameters unchanged to matching `CTAudEp.dll!CT*` functions.

Confirmed forwarding covers:

- endpoint master/channel volume;
- monitoring open/range/level/mute;
- KS node type volume / Mic Boost;
- KS node Auto Gain Control / Mic AGC.

No scalar conversion, dB conversion, CDC framing, or raw `UInt16` conversion occurs inside these MalLgcy wrappers.

Because this exact DLL is x86, it cannot be loaded directly into an ARM64-native controller process.

## First remaining Windows Mixer task

The next native implementation target is now:

`CTAudEp.dll`

Goals:

1. recover the exact Windows COM/Core Audio calls behind endpoint master/channel volume;
2. recover DeviceTopology discovery for monitoring controls;
3. recover KS node/property calls for Mic Boost and Mic AGC;
4. record GUIDs/property IDs/node matching needed for direct ARM64 implementation;
5. identify any Creative-specific endpoint filtering/property behavior that must be reproduced.

The intended ARM64 direction is direct Windows Core Audio / DeviceTopology / KS implementation, not reuse of the supplied x86 MalLgcy wrapper, unless later static evidence proves an irreplaceable Creative-specific behavior.

## Remaining CDC AudioLevel unit task

Recover the exact official engineering-unit conversion for the CDC raw `UInt16` level/range representation.

Current exact evidence:

- `RawResAudioLevelGet.GetValue()` returns raw `UInt16 CurValue` unchanged;
- `AudioControlLevelRange` carries raw `UInt16` Min/Max/Step unchanged;
- `CDCGameVoiceFeature` passes those `UInt16` ranges through without converting to dB inside `Creative.Platform.Devices.dll`;
- `MalLgcy.dll` does not consume this CDC representation and contains no relevant conversion layer.

The observed hardware values are compatible with a signed fixed-point representation, but do **not** hard-code `/256` as confirmed until its actual conversion code is recovered.

Static targets for this remaining point:

1. higher Creative App/UI code that consumes CDC Game/Voice level values;
2. related Creative Platform assemblies if they contain the conversion helper;
3. native code only where there is direct evidence it consumes this CDC `UInt16` representation.

No new hardware probe is required first.

## Secondary priority — backend classification

After the remaining CDC raw-unit conversion and Windows endpoint implementation details are closed, continue classifying X4 features by backend:

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
- `CTAudEp.dll`
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
