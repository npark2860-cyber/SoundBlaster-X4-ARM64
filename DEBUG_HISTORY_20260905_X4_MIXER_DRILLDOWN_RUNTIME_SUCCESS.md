# DEBUG HISTORY — X4 Mixer Read-Only Drill-Down Runtime (2026-09-05)

## Scope

This document records the first successful SB1815/X4 read-only mixer drill-down on the native Windows ARM64 CTCDC path.

Branch at start of runtime interpretation:

`exp/windows-arm64-x4-native-controller@b6b8084f48d405aa82a7549bdb9aceebb7f026bc`

No ASIO source was changed. The probe sent no mixer SET, no Malcolm SET (`0x12`), no Direct Mode setter, and exposed no arbitrary raw-command CLI.

The user-provided runtime report was `X4_MIXER_DRILLDOWN_REPORT.txt`.

## Runtime prerequisite

The previously discovered CTCDC ownership/session condition still applies:

- Creative App running can prevent the independent CTCDC path from receiving the first `5A 03 00` readiness response.
- Creative App fully closed allows the same independent path to respond normally.

Do not change protocol bytes to work around this. Independent validation requires Creative App to be fully closed before opening the X4 CDC COM session.

## Session gate — PASS

Observed on COM3:

- CTCDC serial init: mask `0x05`, 115200/8N1, zero timeouts, purge `0x0F`, `SETDTR`
- `5A 03 00` -> `5A 03 02 3B 00`
- maximum payload = `59`
- firmware query returned `1.9.251008.0930`
- button query returned `5A 26 06 05 00 01 00 1E 00`

The session stayed valid through the complete read-only drill-down.

## Malcolm sub-feature support — decisive runtime result

Request:

`5A 10 00`

Response:

`5A 10 08 40 00 00 00 00 00 00 00`

Parsed:

- `FeatureMask = 0x00000040`
- `UnavailableMask = 0x00000000`

The statically recovered Malcolm feature-mask table maps `0x00000040` to **GraphicEQ**.

This strongly explains the previous capability-map runtime:

- PlaybackManager `0x96` Graphic EQ block `9..20` responded normally.
- PlaybackManager Surround / Dialog / SVM / Crystalizer / Bass parameters did not respond.
- VoiceInputManager `0x95` CrystalVoice parameters `0..45` did not respond.

Important interpretation boundary:

This result proves which Malcolm sub-feature bit the current SB1815 firmware/session reports. It does **not** prove that X4 lacks CrystalVoice or other Creative features globally. The current ARM64 machine is not running the complete official Creative filter/APO stack, so driver/APO-backed features must be traced separately.

Do not resume blind `0x95` parameter probing without new backend evidence.

## AudioControl information — 11 runtime-confirmed indices

Request:

`5A 21 00`

Header:

- Total = `11`
- Count = `11`
- AdditionalPacket = `0`
- AudioControlIndex = `0`

Runtime-confirmed descriptor/index map:

| Index | Raw | Type | Volume | Mute | Source | Recording | dB | Channel nibble |
|---:|---:|---|:---:|:---:|:---:|:---:|:---:|---:|
| 0 | `0x89C1` | Speaker | Y | Y | Y | N | Y | 8 |
| 1 | `0x89C2` | Headphone | Y | Y | Y | N | Y | 8 |
| 2 | `0x29CE` | SPDIF Output | Y | Y | Y | N | Y | 2 |
| 3 | `0x29CF` | Mic Monitoring | Y | Y | Y | N | Y | 2 |
| 4 | `0x29D0` | Line Monitoring | Y | Y | Y | N | Y | 2 |
| 5 | `0x2AC3` | Mic Input | Y | Y | N | Y | Y | 2 |
| 6 | `0x2AC4` | Line Input | Y | Y | N | Y | Y | 2 |
| 7 | `0x2AC5` | What U Hear Recording | Y | Y | N | Y | Y | 2 |
| 8 | `0x29D1` | SPDIF Monitoring | Y | Y | Y | N | Y | 2 |
| 9 | `0x2AC9` | SPDIF Input | Y | Y | N | Y | Y | 2 |
| 10 | `0x00BF` | Automatic Gain Control | N | Y | N | N | N | 0 |

The response also contained trailing bytes `01 00 00 00` after the 11 packed descriptors. They remain uninterpreted and must not be silently assigned meaning.

## Audio level ranges (`0x22`) — 10 volume controls returned

Request used the 11 candidate indices in the recovered fixed 32-byte index array.

Response reported:

- RangeCount = `10`
- AdditionalPacket = `0`

This is consistent with index 10 / AGC reporting `HasVolume = false`.

Raw range results:

| Index | Type | Max raw | Min raw | Step raw |
|---:|---|---:|---:|---:|
| 0 | Speaker | `0x0000` | `0xAFF3` | `0x0199` |
| 1 | Headphone | `0x0000` | `0xAFF3` | `0x0199` |
| 2 | SPDIF Output | `0x0000` | `0xAFF3` | `0x0040` |
| 3 | Mic Monitoring | `0x0900` | `0xA000` | `0x0100` |
| 4 | Line Monitoring | `0x0900` | `0xA000` | `0x0100` |
| 5 | Mic Input | `0x0900` | `0xA000` | `0x0100` |
| 6 | Line Input | `0x0900` | `0xA000` | `0x0100` |
| 7 | What U Hear Recording | `0x0000` | `0xAFF3` | `0x0040` |
| 8 | SPDIF Monitoring | `0x0000` | `0xAFF3` | `0x0040` |
| 9 | SPDIF Input | `0x0000` | `0xAFF3` | `0x0040` |

The probe also logged signed-16 interpretations, but the exact engineering-unit/fixed-point conversion is **not yet proven** for all controls. Do not present these raw values as final dB numbers until the official conversion code/call sites are recovered.

## Current AudioLevel GET (`0x23`) — partial success, semantics unresolved

Request form tested:

`5A 23 02 01 <index>`

### Index 0 — Speaker

Response:

`5A 23 04 00 80 D6 03`

Probe parser currently reports:

- index = `0`
- raw UInt16 candidate = `0xD680` / signed `-10624`
- one additional payload byte `0x03` remains unexplained by the earlier assumed 3-byte response model

### Index 1 — Headphone

Response:

`5A 23 04 01 B1 FC 03`

Probe parser currently reports:

- index = `1`
- raw UInt16 candidate = `0xFCB1` / signed `-847`
- the same additional payload byte `0x03` is present

### Indices 2..9

All returned ACK status `0x80` (`GeneralFailure`) for the same two-byte GET request shape.

Do **not** conclude that those controls lack volume support. Their `0x21` descriptors advertise `HasVolume`, and `0x22` returned valid range entries for all of them.

The correct conclusion is:

**The exact `AudioLevel` GET request/response semantics are still incomplete.**

Possible areas to resolve statically before another runtime test include:

- extra channel/count/target field in the response or associated API model;
- different GET form for source/monitor/recording controls;
- active/inactive control state requirements;
- a managed wrapper transforming the raw response before exposing level state.

Do not issue mixer SET based on the current `0x23` parser.

## AudioMute GET (`0x24`) — full success

Request form:

`5A 24 02 01 <index>`

All 11 runtime indices returned normal command-`0x24` responses.

Observed mute state:

| Index | Type | Mute |
|---:|---|---:|
| 0 | Speaker | 0 |
| 1 | Headphone | 0 |
| 2 | SPDIF Output | 0 |
| 3 | Mic Monitoring | 1 |
| 4 | Line Monitoring | 0 |
| 5 | Mic Input | 0 |
| 6 | Line Input | 0 |
| 7 | What U Hear Recording | 0 |
| 8 | SPDIF Monitoring | 0 |
| 9 | SPDIF Input | 0 |
| 10 | Automatic Gain Control | 1 |

This strongly validates the actual `0..10` AudioControl index map and confirms `0x24` as a live SB1815 mixer read path.

No mute SET was sent.

## Architecture implication

The current controller work must distinguish four backend classes:

1. X4 firmware / CTCDC raw controls;
2. Windows endpoint / Core Audio controls;
3. Creative driver/filter/APO processing;
4. app/profile orchestration.

Current evidence:

- Direct Mode: firmware/CTCDC hardware-confirmed.
- Graphic EQ state: firmware/CTCDC Malcolm block hardware-read confirmed.
- Mixer AudioControl information/ranges/mutes: firmware/CTCDC hardware-read confirmed.
- CrystalVoice: raw VoiceInputManager path is not exposed in the tested firmware/session; global feature support remains unresolved because the official Creative driver/APO stack is absent on the ARM64 setup.
- direct `0x44` GraphicEqualizerControl and `0xA7` SoundModeControl: SB1815 returned `NotSupported` in the tested session.

## Next action

1. Static-trace the exact official `AudioLevel` GET response model and managed/native call sites, especially the extra trailing `0x03` seen on indices 0 and 1.
2. Determine why indices 2..9 return `GeneralFailure` despite valid `HasVolume` descriptors and `0x22` ranges.
3. Keep the next runtime test read-only until `0x23` semantics are understood.
4. Separately trace CrystalVoice / Acoustic Engine backend ownership through Creative Platform, `CTUSBAPO64.dll`, `CTUSBfilt64.sys`, and related KS/APO paths. Do not infer hardware non-support from the current Microsoft-driver ARM64 environment.
5. Keep all controller work on `exp/windows-arm64-x4-native-controller`; do not modify B5 ASIO code.
