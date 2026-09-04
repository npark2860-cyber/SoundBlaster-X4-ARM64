# NEXT ACTION — X4 Native Controller / Driver Analysis

Updated: 2026-09-05 KST

## Branch

`exp/windows-arm64-x4-native-controller`

Use GitHub as source of truth and verify the actual branch HEAD before work.

## Immediate priority

Resolve the remaining read-only `AudioLevel` semantics before any mixer SET work.

Known runtime facts from `X4_MIXER_DRILLDOWN_REPORT.txt`:

- session gate is stable when Creative App is fully closed;
- Malcolm feature mask is `0x00000040` (GraphicEQ only in the recovered mask table);
- AudioControl indices `0..10` are runtime-confirmed;
- `0x22` range query returns entries for indices `0..9`;
- `0x24` Mute GET works for all indices `0..10`;
- `0x23` Level GET succeeds only for indices `0` and `1` with a 4-byte payload, not the previously assumed 3-byte payload;
- `0x23` Level GET returns `GeneralFailure (0x80)` for indices `2..9`.

## First task in the next tab

Perform static analysis of the supplied Creative Windows binaries to recover the exact AudioLevel read path.

Priority targets:

1. `Creative.Platform.Devices.dll`
2. `Creative.Platform.Mixer.dll`
3. `CTCDC.dll`
4. `CTIntrfu.dll`
5. related raw request/response models and call sites

Determine:

- exact request structure for `CDCRawCommand.AudioLevel (0x23)` GET;
- exact response structure, including the extra trailing byte `0x03` observed for Speaker/Headphone;
- whether source/recording/monitoring controls use a different operation or additional parameter;
- whether active output/input state gates current-level reads;
- exact conversion from raw `UInt16` range/level values to engineering units/dB.

Do not issue mixer SET until this is resolved.

## Secondary priority — backend classification

After the AudioLevel read path is understood, continue classifying X4 features by backend:

1. firmware / CTCDC;
2. Windows Core Audio endpoint/property;
3. Creative filter/APO/driver processing;
4. app/profile orchestration.

CrystalVoice and the non-EQ Acoustic Engine controls are especially important.

Current evidence does **not** prove that X4 lacks CrystalVoice. It only shows that the tested raw Malcolm `VoiceInputManager (0x95)` path did not respond and that the current ARM64 machine lacks the complete official Creative filter/APO stack.

Trace CrystalVoice through:

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
- Mixer AudioControl info/ranges/mute: `0x21/0x22/0x24`, hardware-read confirmed.

## Known rejected / unsupported routes in current session

- direct GraphicEqualizerControl `0x44`: ACK `0x81 NotSupported`.
- direct SoundModeControl `0xA7`: ACK `0x81 NotSupported`.
- raw VoiceInputManager `0x95` params `0..45`: no response.
- naked COM Direct Mode without CTCDC session setup: rejected previously.
- HID / BLE / UAC Extension Unit / vendor-interface guesses: do not repeat without new evidence.
